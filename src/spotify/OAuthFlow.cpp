#include "spotify/OAuthFlow.h"

#include "spotify/SpotifyError.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QCoroNetworkReply>

#include <memory>
#include <utility>

namespace swiftify {

namespace {

QByteArray callbackPage()
{
    return QByteArray("<!doctype html><meta charset=utf-8><title>Swiftify</title>"
                      "<style>body{font:16px system-ui,sans-serif;background:#141313;color:#e6e1e1;"
                      "display:grid;place-items:center;height:100vh;margin:0}</style><p>")
        + QCoreApplication::translate("OAuthFlow", "You're signed in. You can close this tab and return to Swiftify.")
              .toHtmlEscaped().toUtf8()
        + "</p>";
}

QByteArray httpResponse(int status, const QByteArray &reason, const QByteArray &body)
{
    return "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;
}

QByteArray base64Url(const QByteArray &bytes)
{
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace

QUrl OAuthFlow::redirectUri()
{
    return QUrl(QStringLiteral("http://127.0.0.1:8898/login"));
}

namespace {
QUrl s_tokenEndpoint {QStringLiteral("https://accounts.spotify.com/api/token")};
}

QUrl OAuthFlow::tokenEndpoint()
{
    return s_tokenEndpoint;
}

void OAuthFlow::setTokenEndpoint(const QUrl &url)
{
    s_tokenEndpoint = url;
}

OAuthFlow::OAuthFlow(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
{
    connect(&m_server, &QTcpServer::newConnection, this, &OAuthFlow::acceptConnection);
}

OAuthFlow::~OAuthFlow() = default;

bool OAuthFlow::isCallbackUrl(const QUrl &url)
{
    const QUrl redirect = redirectUri();
    return url.scheme() == redirect.scheme() && url.host() == redirect.host() && url.port() == redirect.port()
        && url.path() == redirect.path();
}

QString OAuthFlow::randomUrlSafeString(int byteCount)
{
    QByteArray bytes(byteCount, Qt::Uninitialized);
    QRandomGenerator::system()->generate(bytes.begin(), bytes.end());
    return QString::fromLatin1(base64Url(bytes));
}

QUrl OAuthFlow::beginAuthorization(const QString &rawClientId, const QStringList &scopes)
{
    const QString clientId = rawClientId.trimmed();
    if (clientId.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::OAuth, QStringLiteral("Enter the Client ID from your Spotify developer app."));
    }

    stopListening();
    if (!m_server.listen(QHostAddress::LocalHost, kRedirectPort)) {
        throw SpotifyError(SpotifyError::Kind::OAuth,
                           QStringLiteral("Swiftify needs port %1 for the Spotify login, but %2. Close whatever is using it and try again.")
                               .arg(kRedirectPort)
                               .arg(m_server.errorString().toLower()));
    }

    const QString verifier = randomUrlSafeString(32);
    const QString challenge = QString::fromLatin1(
        base64Url(QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256)));
    const QString state = randomUrlSafeString(24);

    QUrl url(QStringLiteral("https://accounts.spotify.com/authorize"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri().toString());
    query.addQueryItem(QStringLiteral("scope"), scopes.join(u' '));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("code_challenge"), challenge);
    query.addQueryItem(QStringLiteral("state"), state);
    url.setQuery(query);

    m_codeVerifier = verifier;
    m_expectedState = state;
    m_receivedCallback = false;
    return url;
}

void OAuthFlow::acceptConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { handleRequest(socket); });
    }
}

void OAuthFlow::handleRequest(QTcpSocket *socket)
{
    // only the request line matters. browsers send it in the first packet.
    if (!socket->canReadLine()) {
        if (socket->bytesAvailable() > 8192) {
            socket->abort();
        }
        return;
    }
    const QList<QByteArray> requestLine = socket->readLine().trimmed().split(' ');
    QUrl url;
    if (requestLine.size() >= 2 && requestLine[0] == "GET") {
        url = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(kRedirectPort) + QString::fromLatin1(requestLine[1]));
    }

    if (!isCallbackUrl(url)) {
        socket->write(httpResponse(404, "Not Found", QByteArray()));
        socket->disconnectFromHost();
        return;
    }

    socket->write(httpResponse(200, "OK", callbackPage()));
    socket->disconnectFromHost();
    if (m_receivedCallback) {
        return;
    }
    m_receivedCallback = true;
    stopListening();
    emit callbackReceived(url);
}

void OAuthFlow::stopListening()
{
    if (m_server.isListening()) {
        m_server.close();
    }
}

void OAuthFlow::cancel()
{
    stopListening();
    m_codeVerifier.clear();
    m_expectedState.clear();
}

QCoro::Task<OAuthToken> OAuthFlow::exchangeCallback(QUrl callbackUrl, QString clientId)
{
    if (!isCallbackUrl(callbackUrl) || m_codeVerifier.isEmpty() || m_expectedState.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::OAuth, QStringLiteral("Spotify returned an invalid authorization callback."));
    }
    const QString verifier = std::exchange(m_codeVerifier, {});
    const QString expectedState = std::exchange(m_expectedState, {});

    const QUrlQuery query(callbackUrl);
    if (query.hasQueryItem(QStringLiteral("error"))) {
        throw SpotifyError(SpotifyError::Kind::OAuth,
                           QStringLiteral("Spotify authorization was denied: %1")
                               .arg(query.queryItemValue(QStringLiteral("error"), QUrl::FullyDecoded)));
    }
    if (query.queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded) != expectedState) {
        throw SpotifyError(SpotifyError::Kind::OAuth,
                           QStringLiteral("Spotify returned an authorization response with an invalid state."));
    }
    const QString code = query.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
    if (code.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::OAuth, QStringLiteral("Spotify returned an invalid authorization callback."));
    }

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("redirect_uri"), redirectUri().toString());
    form.addQueryItem(QStringLiteral("code_verifier"), verifier);
    co_return co_await requestToken(form);
}

QCoro::Task<OAuthToken> OAuthFlow::refreshAccessToken(QString refreshToken, QString clientId)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    form.addQueryItem(QStringLiteral("client_id"), clientId);
    co_return co_await requestToken(form);
}

QCoro::Task<OAuthToken> OAuthFlow::requestToken(QUrlQuery form)
{
    QNetworkRequest request(tokenEndpoint());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/x-www-form-urlencoded"));
    request.setTransferTimeout(30'000);

    std::unique_ptr<QNetworkReply> reply(m_network->post(request, form.toString(QUrl::FullyEncoded).toUtf8()));
    co_await qCoro(reply.get()).waitForFinished();

    const QByteArray data = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        const QString detail = status == 0 ? reply->errorString()
                                           : (data.isEmpty() ? QStringLiteral("HTTP %1").arg(status) : QString::fromUtf8(data));
        // the one answer that means the login itself is gone, as opposed to
        // the network or spotify having a moment
        const bool revoked = status == 400
            && QJsonDocument::fromJson(data).object().value(QStringLiteral("error")).toString()
                == QLatin1StringView("invalid_grant");
        throw SpotifyError(revoked ? SpotifyError::Kind::LoginRevoked : SpotifyError::Kind::OAuth,
                           QStringLiteral("Spotify token exchange failed: %1").arg(detail));
    }

    const QJsonObject object = QJsonDocument::fromJson(data).object();
    OAuthToken token;
    token.accessToken = object.value(QStringLiteral("access_token")).toString();
    token.refreshToken = object.value(QStringLiteral("refresh_token")).toString();
    token.expiration = QDateTime::currentDateTimeUtc().addSecs(object.value(QStringLiteral("expires_in")).toInt());
    if (token.accessToken.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::OAuth,
                           QStringLiteral("Spotify token exchange failed: the response had no access token."));
    }
    co_return token;
}

} // namespace swiftify
