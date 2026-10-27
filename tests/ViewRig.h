#pragma once

#include "FakeCore.h"

#include "app/AppController.h"
#include "app/ErrorCenter.h"
#include "app/Settings.h"
#include "artwork/ArtworkCache.h"
#include "audio/AudioShaping.h"
#include "library/LibraryStore.h"
#include "playback/PlaybackController.h"
#include "render/SpectrumSource.h"
#include "stats/PlayHistory.h"
#include "stats/StatsStore.h"
#include "spotify/CredentialStore.h"
#include "spotify/OAuthFlow.h"
#include "spotify/SpotifySession.h"
#include "theme/PlayerStyle.h"
#include "theme/Theme.h"
#include "system/DiscordPresence.h"
#include "system/Updates.h"

#include <QCoreApplication>
#include <QImage>
#include <QNetworkAccessManager>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <rhi/qrhi.h>

#include <memory>

namespace swiftify::testing {

/// everything main() builds, around a core that plays nothing
struct ViewRig {
    QTemporaryDir directory;
    Settings settings {directory.filePath(QStringLiteral("swiftify.conf"))};
    Theme theme {&settings};
    ErrorCenter errors {nullptr};
    MemoryCredentialStore credentials;
    QNetworkAccessManager network;
    FakeCore core;
    SpotifySession session {&core, &credentials, &network, &errors};
    PlaybackController player {&core, &session, &settings, &errors};
    DiscordPresence discord {&player, &settings};
    LibraryStore library {&session, &settings, &errors};
    AudioShaping sound {&core, &settings, &session};
    AppController app {nullptr};
    Updates updates {&settings};
    ArtworkCache artwork {&network};
    PlayerStyle style {&player, &theme, &artwork};
    SpectrumSource spectrum {&core, &player};
    // the plays are kept in memory only, so a run leaves nothing behind
    PlayHistory listeningHistory {QString {}};
    PlayTracker listeningTracker {&player, &listeningHistory};
    StatsStore stats {&settings, &session, &network, &listeningHistory, &listeningTracker};
    QQmlEngine engine;

    static constexpr QSize kSize {1280, 780};
    std::unique_ptr<QQuickRenderControl> control = std::make_unique<QQuickRenderControl>();
    std::unique_ptr<QQuickWindow> window = std::make_unique<QQuickWindow>(control.get());
    std::unique_ptr<QRhiTexture> texture;
    std::unique_ptr<QRhiRenderBuffer> depthStencil;
    std::unique_ptr<QRhiTextureRenderTarget> target;
    std::unique_ptr<QRhiRenderPassDescriptor> pass;
    bool gpu = false;

    ViewRig()
    {
        // Fallback sleeves use the same image provider as ordinary covers.
        // Install it just as main() does, even when a fixture has no remote art.
        engine.addImageProvider(QStringLiteral("art"), new ArtworkImageProvider);
        library.attachPlayer(&player);
        window->resize(kSize);
        gpu = control->initialize() && attachTexture();
        if (!gpu) {
            window = std::make_unique<QQuickWindow>();
            control.reset();
            window->resize(kSize);
        }
    }

    ~ViewRig()
    {
        // a request still waiting on the core finishes, cancelled, while
        // the player it belongs to is still here, not in the next test
        core.reset();
        settle();
        // the target goes before the window that draws into it
        window.reset();
        pass.reset();
        target.reset();
        depthStencil.reset();
        texture.reset();
        control.reset();
    }

    bool attachTexture()
    {
        QRhi *rhi = control->rhi();
        // as many pixels a point as the window has (QT_SCALE_FACTOR): one,
        // unless a run asks to see what a retina screen costs
        const qreal ratio = window->devicePixelRatio();
        const QSize pixels = (QSizeF(kSize) * ratio).toSize();
        texture.reset(rhi->newTexture(QRhiTexture::RGBA8, pixels, 1,
                                      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        depthStencil.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixels, 1));
        if (!texture->create() || !depthStencil->create()) {
            return false;
        }
        QRhiTextureRenderTargetDescription description {QRhiColorAttachment(texture.get())};
        description.setDepthStencilBuffer(depthStencil.get());
        target.reset(rhi->newTextureRenderTarget(description));
        pass.reset(target->newCompatibleRenderPassDescriptor());
        target->setRenderPassDescriptor(pass.get());
        if (!target->create()) {
            return false;
        }
        QQuickRenderTarget renderTarget = QQuickRenderTarget::fromRhiRenderTarget(target.get());
        renderTarget.setDevicePixelRatio(ratio);
        window->setRenderTarget(renderTarget);
        return true;
    }

    QImage render()
    {
        if (!gpu) {
            window->show();
            settle();
            return window->grabWindow();
        }
        control->polishItems();
        control->beginFrame();
        control->sync();
        control->render();
        QRhiReadbackResult result;
        QRhiResourceUpdateBatch *readback = control->rhi()->nextResourceUpdateBatch();
        readback->readBackTexture(texture.get(), &result);
        control->commandBuffer()->resourceUpdate(readback);
        control->endFrame();
        const QImage image(reinterpret_cast<const uchar *>(result.data.constData()), result.pixelSize.width(),
                           result.pixelSize.height(), QImage::Format_RGBA8888_Premultiplied);
        return control->rhi()->isYUpInFramebuffer() ? image.flipped() : image.copy();
    }

    /// brings playback back from a saved login, the way a launch does
    void connectPlayback()
    {
        if (session.isConnected()) {
            return;
        }
        credentials.items.insert(CredentialStore::Account::Playback,
                                 {QString::fromLatin1(OAuthFlow::kPlaybackClientId), QStringLiteral("refresh"),
                                  OAuthFlow::kPlaybackAuthorizationVersion, QStringLiteral("stored-login"), {}, {}});
        session.setInitialLibraryLoader({});
        session.restoreSavedSessions();
        std::shared_ptr<PendingCall> connect;
        QTRY_VERIFY(std::any_of(core.pending.cbegin(), core.pending.cend(), [](const auto &call) { return call->name == QLatin1StringView("connectStored"); }));
        connect = core.take(QStringLiteral("connectStored"));
        connect->succeed(QStringLiteral("stored-login"));
        QTRY_VERIFY(session.isConnected());
    }

    void playSomething()
    {
        QList<Song> queue;
        for (int index = 0; index < 4; ++index) {
            Song song;
            song.name = QStringLiteral("song%1").arg(index);
            song.artists = QStringLiteral("Artist");
            song.albumName = QStringLiteral("Album");
            song.uri = QStringLiteral("spotify:track:%1").arg(index, 22, 10, QLatin1Char('0'));
            song.durationMs = 200'000;
            queue.append(song);
        }
        connectPlayback();
        player.play(queue[0], queue);
        settle();
        const auto call = core.take(QStringLiteral("playTrack"));
        QVERIFY(call);
        call->succeed();
        settle();
        QVERIFY(player.isPlaying());
    }

    /// builds a view into the window, draws a frame of it, and hands it back
    std::unique_ptr<QQuickItem> show(const QString &type)
    {
        QQmlComponent component(&engine);
        component.loadFromModule("Swiftify", type);
        std::unique_ptr<QObject> object(component.create());
        if (!object) {
            qWarning().noquote() << component.errorString();
            return {};
        }
        std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(object.release()));
        item->setParentItem(window->contentItem());
        item->setSize(kSize);
        settle();
        // twice: the first frame lays out what the second shows
        render();
        settle();
        frame = render();
        return item;
    }

    static void settle()
    {
        for (int pass = 0; pass < 5; ++pass) {
            QCoreApplication::processEvents();
        }
    }

    QImage frame;
};

} // namespace swiftify::testing
