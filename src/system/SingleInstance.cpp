#include "system/SingleInstance.h"

#include <QDir>
#include <QLocalSocket>

namespace swiftify {

QString SingleInstance::commandFrom(const QStringList &arguments)
{
    for (const QString &argument : arguments.mid(1)) {
        if (argument == QLatin1StringView("--immersive")) return QStringLiteral("immersive");
        if (argument == QLatin1StringView("--play-pause")) return QStringLiteral("play-pause");
        if (argument == QLatin1StringView("--next")) return QStringLiteral("next");
        if (argument == QLatin1StringView("--previous")) return QStringLiteral("previous");
        if (argument == QLatin1StringView("--stats")) return QStringLiteral("stats");
    }
    return QStringLiteral("raise");
}

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                while (socket->canReadLine()) {
                    emit commandReceived(QString::fromUtf8(socket->readLine()).trimmed());
                }
            });
        }
    });
}

QString SingleInstance::serverName()
{
    return QStringLiteral("swiftify-") + QDir::home().dirName();
}

bool SingleInstance::forwardToRunningInstance(const QStringList &arguments)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(300)) {
        return false;
    }
    socket.write(commandFrom(arguments).toUtf8() + '\n');
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

void SingleInstance::listen()
{
    // a crashed copy leaves its socket file behind; nobody is answering on it.
    QLocalServer::removeServer(serverName());
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.listen(serverName());
}

} // namespace swiftify
