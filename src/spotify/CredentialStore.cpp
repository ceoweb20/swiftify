/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "spotify/CredentialStore.h"

#include "spotify/SpotifyError.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <array>

namespace swiftify {

QByteArray StoredCredentials::toJson() const
{
    QJsonObject object {
        {QStringLiteral("clientID"), clientId},
        {QStringLiteral("refreshToken"), refreshToken},
        {QStringLiteral("authorizationVersion"), authorizationVersion},
    };
    if (!playbackLogin.isEmpty()) {
        object.insert(QStringLiteral("playbackLogin"), playbackLogin);
    }
    if (!accessToken.isEmpty() && accessTokenExpiration.isValid()) {
        object.insert(QStringLiteral("accessToken"), accessToken);
        object.insert(QStringLiteral("accessTokenExpiration"), accessTokenExpiration.toMSecsSinceEpoch());
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<StoredCredentials> StoredCredentials::fromJson(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    StoredCredentials credentials;
    credentials.clientId = object.value(QStringLiteral("clientID")).toString();
    credentials.refreshToken = object.value(QStringLiteral("refreshToken")).toString();
    credentials.authorizationVersion = object.value(QStringLiteral("authorizationVersion")).toInt();
    credentials.playbackLogin = object.value(QStringLiteral("playbackLogin")).toString();
    credentials.accessToken = object.value(QStringLiteral("accessToken")).toString();
    if (const qint64 expiration = object.value(QStringLiteral("accessTokenExpiration")).toInteger(); expiration > 0) {
        credentials.accessTokenExpiration = QDateTime::fromMSecsSinceEpoch(expiration, QTimeZone::UTC);
    }
    // a login spotify revoked can still play through librespot's own
    if (credentials.clientId.isEmpty() || (credentials.refreshToken.isEmpty() && credentials.playbackLogin.isEmpty())) {
        return std::nullopt;
    }
    return credentials;
}

QString CredentialStore::accountName(Account account)
{
    return account == Account::Playback ? QStringLiteral("spotify-oauth") : QStringLiteral("spotify-library-oauth");
}

QCoro::Task<std::optional<StoredCredentials>> MemoryCredentialStore::load(Account account)
{
    const auto found = items.constFind(account);
    co_return found == items.cend() ? std::nullopt : std::optional(*found);
}

struct CachedCredentialStore::State {
    std::array<bool, 2> known {};
    std::array<std::optional<StoredCredentials>, 2> values;
    std::array<quint64, 2> revision {};
    bool closed = false;
};

CachedCredentialStore::CachedCredentialStore(CredentialStore *backend)
    : m_backend(backend), m_state(std::make_shared<State>())
{
    Q_ASSERT(backend);
}

CachedCredentialStore::~CachedCredentialStore()
{
    m_state->closed = true;
    for (auto &value : m_state->values) value.reset();
}

QCoro::Task<std::optional<StoredCredentials>> CachedCredentialStore::load(Account account)
{
    const auto state = m_state;
    const auto index = size_t(account);
    if (state->known.at(index)) {
        // QCoro may move from an lvalue in return_value(). Return a copy,
        // otherwise a read would consume the cached credential strings.
        co_return std::optional<StoredCredentials>(state->values.at(index));
    }
    const auto revision = state->revision.at(index);
    const auto value = co_await m_backend->load(account);
    if (state->closed) co_return std::nullopt;
    if (revision == state->revision.at(index)) {
        state->values.at(index) = value;
        state->known.at(index) = true;
    }
    co_return state->known.at(index) ? state->values.at(index) : std::nullopt;
}

QCoro::Task<> CachedCredentialStore::save(Account account, StoredCredentials credentials)
{
    const auto state = m_state;
    const auto index = size_t(account);
    if (state->known.at(index) && state->values.at(index)
        && state->values.at(index)->toJson() == credentials.toJson()) co_return;
    const auto revision = ++state->revision.at(index);
    co_await m_backend->save(account, credentials);
    if (!state->closed && revision == state->revision.at(index)) {
        state->values.at(index) = credentials;
        state->known.at(index) = true;
    }
}

QCoro::Task<> CachedCredentialStore::removeAll()
{
    const auto state = m_state;
    for (size_t index = 0; index < 2; ++index) {
        ++state->revision.at(index);
        state->values.at(index).reset();
        state->known.at(index) = false;
    }
    const auto revisions = state->revision;
    co_await m_backend->removeAll();
    if (!state->closed) {
        for (size_t index = 0; index < 2; ++index) {
            if (revisions.at(index) == state->revision.at(index)) state->known.at(index) = true;
        }
    }
}

QCoro::Task<> MemoryCredentialStore::save(Account account, StoredCredentials credentials)
{
    items.insert(account, credentials);
    co_return;
}

QCoro::Task<> MemoryCredentialStore::removeAll()
{
    items.clear();
    co_return;
}

} // namespace swiftify
