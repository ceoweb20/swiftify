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

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>

#include <memory>

class QWindow;

namespace swiftify {

class PlaybackController;
class Theme;

class WindowsIntegration : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    WindowsIntegration(PlaybackController *player, Theme *theme, QObject *parent = nullptr);
    ~WindowsIntegration() override;

    static void initializeProcess();
    static void setNativeTitleBar(QWindow *window, bool enabled);
    void setWindow(QWindow *window);
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    void handleMediaButton(int button);

private:
    struct Native;

    void attachWindow();
    void styleWindow();
    void initializeTaskbar();
    void updateButtons();
    void updateProgress();
    void initializeMediaControls();
    void updateMediaControls();
    void updateMediaMetadata();
    void updateMediaTimeline();

    PlaybackController *m_player;
    Theme *m_theme;
    QPointer<QWindow> m_window;
    std::unique_ptr<Native> m_native;
};

} // namespace swiftify
