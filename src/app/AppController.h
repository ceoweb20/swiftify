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

#pragma once

#include "motion/AmbientClock.h"

#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

/// app-wide requests that come from outside the windows: the media session,
/// a second launch, the tray, the dock. the qml shell carries them out.
class AppController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(QString version READ version CONSTANT FINAL)
    Q_PROPERTY(bool desktopIntegration READ hasDesktopIntegration CONSTANT FINAL)
    /// the window keeps the mac's traffic lights, so the app draws no
    /// buttons of its own and leaves the title bar's height clear for them
    Q_PROPERTY(bool macOS READ isMacOS CONSTANT FINAL)
    Q_PROPERTY(bool windows READ isWindows CONSTANT FINAL)
    /// the platform draws native menus: the mac's own, not the offscreen
    /// one a test or a headless run gets, which has none to give
    Q_PROPERTY(bool nativeMenuBar READ hasNativeMenuBar CONSTANT FINAL)
    /// something on the desktop shows tray icons (a StatusNotifier host, such
    /// as end4's bar); it can come and go with the bar
    Q_PROPERTY(bool trayAvailable READ isTrayAvailable NOTIFY trayAvailableChanged FINAL)
    /// the pointer has not moved over the app's windows for a few seconds.
    /// what moves while hovered (a cover's wave, a button's turn) eases to
    /// rest then, rather than having a window drawn every frame for as long
    /// as the pointer is left lying on it; it moves on as the pointer does
    Q_PROPERTY(bool pointerResting READ isPointerResting NOTIFY pointerRestingChanged FINAL)
    /// the app is quitting (cmd-q, the dock's or the tray's quit, a
    /// terminate), not just having its window closed: qt asks each window to
    /// close first, and a window that hides instead would cancel the quit
    Q_PROPERTY(bool quitting READ isQuitting NOTIFY quittingChanged FINAL)
    /// the clock what turns with the music follows (UiState sets its speed)
    Q_PROPERTY(swiftify::AmbientClock *spinClock READ spinClock CONSTANT FINAL)

public:
    /// no default argument: qml builds a singleton with a default constructor
    /// in preference to create(), and would end up with an instance of its own
    explicit AppController(QObject *parent);
    ~AppController() override;

    static AppController *instance();
    static AppController *create(QQmlEngine *, QJSEngine *);

    QString version() const;
    bool hasDesktopIntegration() const;
    bool hasNativeMenuBar() const
    {
        return isMacOS() && QGuiApplication::platformName() == QLatin1StringView("cocoa");
    }
    bool isMacOS() const
    {
#ifdef Q_OS_MACOS
        return true;
#else
        return false;
#endif
    }
    bool isTrayAvailable() const { return m_trayAvailable; }
    bool isWindows() const
    {
#ifdef Q_OS_WIN
        return true;
#else
        return false;
#endif
    }
    bool isPointerResting() const { return m_pointerResting; }
    bool isQuitting() const { return m_quitting; }
    AmbientClock *spinClock() const { return AmbientClock::spin(); }
    /// how long the pointer lies still before it counts as resting
    static constexpr int kPointerRestMs = 3000;

    /// a launch's command: raise, immersive, close-immersive, play-pause,
    /// next, previous; and the library's pages, for desktop key
    /// bindings and scripted measuring: home, playlists, songs, back,
    /// "playlist N" for the Nth of the library's playlists, play-recent, and
    /// "setting <reduce-motion|performance|visualizers> <on|off>"
    void handleCommand(const QString &command);

    Q_INVOKABLE void quit();
    /// gives back what a view that has just gone held: its script objects,
    /// the scene graph's caches nothing uses now, and the heap pages that
    /// freeing left empty, which the system counts against the app until
    /// they are handed back. a moment after asking, so whatever is closing
    /// has finished going; asking again meanwhile starts the wait over.
    Q_INVOKABLE void trimMemory();
    Q_INVOKABLE void copyToClipboard(const QString &text) const;
    Q_INVOKABLE void openUrl(const QString &url) const;
    Q_INVOKABLE void setNativeTitleBar(QObject *target, bool enabled) const;

signals:
    void raiseRequested();
    void immersiveRequested();
    void immersiveCloseRequested();
    void trayAvailableChanged();
    void pointerRestingChanged();
    void quittingChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setTrayAvailable(bool available);
    void setPointerResting(bool resting);
    /// what each visible window draws, by the component it is part of, for
    /// measuring what a frame costs ("census <file>")
    void writeCensus(const QString &path) const;
    void collect();
    void releaseMemory();

    bool m_trayAvailable = false;
    QPointer<QJSEngine> m_engine;
    QTimer m_trim;
    QTimer m_pointerRest;
    QPointF m_pointer;
    bool m_pointerResting = false;
    bool m_quitting = false;
};

} // namespace swiftify
