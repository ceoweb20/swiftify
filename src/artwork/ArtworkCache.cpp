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

#include "artwork/ArtworkCache.h"

#include "app/Paths.h"
#include "artwork/PlaylistCovers.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QPointer>
#include <QThreadPool>
#include <QUrlQuery>
#include <QtConcurrent>

#include <algorithm>
#include <optional>
#include <utility>
#include <QCoroSignal>

namespace swiftify {

Q_LOGGING_CATEGORY(lcArtwork, "swiftify.artwork")

namespace {

ArtworkCache *s_instance = nullptr;

constexpr qint64 kDiskCacheBytes = 200LL * 1024 * 1024;
// the disk cache holds every cover fetched; these only spare it a read, or a
// decode, for what was shown a moment ago. what is on screen is on the gpu,
// its decoded pixels let go once there (QSG_TRANSIENT_IMAGES), so decoded
// covers kept here would only be kept twice. what reads a cover's bytes
// again (another size of it, its colours) does so within moments
constexpr int kEncodedCacheBytes = 2 * 1024 * 1024;
constexpr int kDecodedCacheBytes = 4 * 1024 * 1024;

QImage decode(const QByteArray &data, int pixels)
{
    QBuffer buffer;
    buffer.setData(data);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && std::max(size.width(), size.height()) > pixels) {
        reader.setScaledSize(size.scaled(pixels, pixels, Qt::KeepAspectRatio));
    }
    QImage image = reader.read();
    // the format the scene graph uploads without converting
    return image.isNull() ? image : image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

} // namespace

ArtworkCache::ArtworkCache(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_encoded(kEncodedCacheBytes)
    , m_decoded(kDecodedCacheBytes)
{
    if (!s_instance) {
        s_instance = this;
    }
    auto *disk = new QNetworkDiskCache(this);
    disk->setCacheDirectory(paths::cacheDirectory() + QStringLiteral("/artwork"));
    disk->setMaximumCacheSize(kDiskCacheBytes);
    m_network->setCache(disk);
}

ArtworkCache::~ArtworkCache()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ArtworkCache *ArtworkCache::instance()
{
    return s_instance;
}

int ArtworkCache::bucket(int pixels)
{
    // the size ArtworkImage asks for, rounded up to 32 as it rounds it: a
    // cover is decoded at the size it is shown, give or take a few pixels
    return std::clamp((pixels + 31) / 32 * 32, 32, 2048);
}

void ArtworkCache::bytes(const QUrl &url, BytesCallback callback)
{
    if (!url.isValid() || url.isEmpty()) {
        callback({});
        return;
    }
    if (const QByteArray *cached = m_encoded.object(url)) {
        callback(*cached);
        return;
    }
    auto pending = m_downloads.find(url);
    if (pending != m_downloads.end()) {
        pending->append(std::move(callback));
        return;
    }
    m_downloads.insert(url, {std::move(callback)});

    if (url.isLocalFile()) {
        // a cover on disk, taken out of a music file of the person's own or
        // kept beside it: read off the gui thread, never through the cache
        QtConcurrent::run(QThreadPool::globalInstance(), [path = url.toLocalFile()] {
            QFile file(path);
            return file.size() < 32 * 1024 * 1024 && file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        }).then(this, [this, url](const QByteArray &data) { finishDownload(url, data); });
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    request.setTransferTimeout(30'000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, url, reply] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray data = status >= 200 && status < 300 ? reply->readAll() : QByteArray();
        // readAll grows its buffer as the reply comes in, and a cover kept
        // as read took up to twice what the memory cache counted for it
        data.squeeze();
        finishDownload(url, data);
    });
}

/// what a coroutine waits on while the callback form fetches
class BytesWaiter : public QObject {
    Q_OBJECT

signals:
    void done();
};

QCoro::Task<QByteArray> ArtworkCache::bytes(QUrl url)
{
    const auto waiter = std::make_shared<BytesWaiter>();
    const auto result = std::make_shared<std::optional<QByteArray>>();
    bytes(url, [waiter, result](const QByteArray &data) {
        *result = data;
        emit waiter->done();
    });
    if (!result->has_value()) {
        co_await qCoro(waiter.get(), &BytesWaiter::done);
    }
    co_return result->value_or(QByteArray());
}

void ArtworkCache::finishDownload(const QUrl &url, const QByteArray &data)
{
    if (!data.isEmpty()) {
        m_encoded.insert(url, new QByteArray(data), int(std::min<qsizetype>(data.size(), kEncodedCacheBytes)));
    }
    const QList<BytesCallback> callbacks = m_downloads.take(url);
    for (const BytesCallback &callback : callbacks) {
        callback(data);
    }
}

void ArtworkCache::image(const QUrl &url, int pixels, ImageCallback callback)
{
    const int size = bucket(pixels);
    const QString key = QString::number(size) + u':' + url.toString();
    if (const QImage *cached = m_decoded.object(key)) {
        callback(*cached);
        return;
    }
    auto pending = m_decodes.find(key);
    if (pending != m_decodes.end()) {
        pending->append(std::move(callback));
        return;
    }
    m_decodes.insert(key, {std::move(callback)});

    if (url.scheme() == QStringLiteral("swiftify-cover")) {
        const QString identity = QUrlQuery(url).queryItemValue(QStringLiteral("key"), QUrl::FullyDecoded);
        const bool dark = url.path() == QStringLiteral("/dark");
        QtConcurrent::run(QThreadPool::globalInstance(), covers::fallback, identity, dark, size)
            .then(this, [this, key](const QImage &image) {
                m_decoded.insert(key, new QImage(image), int(image.sizeInBytes()));
                const QList<ImageCallback> callbacks = m_decodes.take(key);
                for (const ImageCallback &callback : callbacks) {
                    callback(image);
                }
            });
        return;
    }

    bytes(url, [this, key, size](const QByteArray &data) {
        if (data.isEmpty()) {
            const QList<ImageCallback> callbacks = m_decodes.take(key);
            for (const ImageCallback &callback : callbacks) {
                callback({});
            }
            return;
        }
        QtConcurrent::run(QThreadPool::globalInstance(), decode, data, size)
            .then(this, [this, key](const QImage &image) {
                if (!image.isNull()) {
                    m_decoded.insert(key, new QImage(image), int(std::min<qsizetype>(image.sizeInBytes(), kDecodedCacheBytes)));
                }
                const QList<ImageCallback> callbacks = m_decodes.take(key);
                for (const ImageCallback &callback : callbacks) {
                    callback(image);
                }
            });
    });
}

void ArtworkCache::invalidate(const QUrl &url)
{
    m_encoded.remove(url);
    const QString suffix = u':' + url.toString();
    for (const QString &key : m_decoded.keys()) {
        if (key.endsWith(suffix)) {
            m_decoded.remove(key);
        }
    }
    if (QAbstractNetworkCache *disk = m_network->cache()) {
        disk->remove(url);
    }
}

namespace {

class ArtworkResponse : public QQuickImageResponse {
public:
    ArtworkResponse(const QUrl &url, int pixels)
    {
        // the provider may be asked from the image loader's thread; the cache
        // and its network live on the gui thread.
        QPointer<ArtworkResponse> self(this);
        QMetaObject::invokeMethod(ArtworkCache::instance(), [self, url, pixels] {
            ArtworkCache::instance()->image(url, pixels, [self](const QImage &image) {
                if (self) {
                    self->m_image = image;
                    emit self->finished();
                }
            });
        }, Qt::QueuedConnection);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_image.isNull() ? QStringLiteral("The artwork could not be loaded.") : QString();
    }

private:
    QImage m_image;
};

} // namespace

QQuickImageResponse *ArtworkImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // id: <url, percent-encoded>. requestedSize carries the device pixels wanted.
    const QUrl url(QUrl::fromPercentEncoding(id.toUtf8()));
    const int pixels = std::max({requestedSize.width(), requestedSize.height(), 48});
    return new ArtworkResponse(url, pixels);
}

} // namespace swiftify

#include "ArtworkCache.moc"
