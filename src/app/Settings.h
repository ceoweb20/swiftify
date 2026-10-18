#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

/// the app's preferences, under the macos app's key names and defaults
/// (AppDesign.swift) so the two read the same way. stores that keep their own
/// data (volume, pins, search history, the equalizer) use value() and setValue().
class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool visualizersEnabled READ visualizersEnabled WRITE setVisualizersEnabled NOTIFY visualizersEnabledChanged FINAL)
    Q_PROPERTY(bool circularVisualizer READ circularVisualizer WRITE setCircularVisualizer NOTIFY circularVisualizerChanged FINAL)
    /// the players' covers round, with their squiggle edge, while visualizers
    /// are off; with them on, the circular style rounds them
    Q_PROPERTY(bool roundArtwork READ roundArtwork WRITE setRoundArtwork NOTIFY roundArtworkChanged FINAL)
    Q_PROPERTY(QString visualizerShape READ visualizerShape WRITE setVisualizerShape NOTIFY visualizerShapeChanged FINAL)
    Q_PROPERTY(double visualizerSensitivity READ visualizerSensitivity WRITE setVisualizerSensitivity NOTIFY visualizerSensitivityChanged FINAL)
    Q_PROPERTY(int streamingQuality READ streamingQuality WRITE setStreamingQuality NOTIFY streamingQualityChanged FINAL)
    Q_PROPERTY(bool crossfadeEnabled READ crossfadeEnabled WRITE setCrossfadeEnabled NOTIFY crossfadeEnabledChanged FINAL)
    Q_PROPERTY(int crossfadeDurationSeconds READ crossfadeDurationSeconds WRITE setCrossfadeDurationSeconds NOTIFY crossfadeDurationSecondsChanged FINAL)
    /// how much of the songs streamed is kept, so playing one again needs no
    /// download: 500, 1000 or 2000 megabytes
    Q_PROPERTY(int songCacheMegabytes READ songCacheMegabytes WRITE setSongCacheMegabytes NOTIFY songCacheMegabytesChanged FINAL)
    Q_PROPERTY(bool livingBackdrop READ livingBackdrop WRITE setLivingBackdrop NOTIFY livingBackdropChanged FINAL)
    Q_PROPERTY(double backdropBlur READ backdropBlur WRITE setBackdropBlur NOTIFY backdropBlurChanged FINAL)
    Q_PROPERTY(double backdropReactivity READ backdropReactivity WRITE setBackdropReactivity NOTIFY backdropReactivityChanged FINAL)
    Q_PROPERTY(bool debuggingEnabled READ debuggingEnabled WRITE setDebuggingEnabled NOTIFY debuggingEnabledChanged FINAL)
    Q_PROPERTY(bool pinnedPlaylistsExpanded READ pinnedPlaylistsExpanded WRITE setPinnedPlaylistsExpanded NOTIFY pinnedPlaylistsExpandedChanged FINAL)
    Q_PROPERTY(bool allPlaylistsExpanded READ allPlaylistsExpanded WRITE setAllPlaylistsExpanded NOTIFY allPlaylistsExpandedChanged FINAL)
    Q_PROPERTY(QString immersivePlayerPanel READ immersivePlayerPanel WRITE setImmersivePlayerPanel NOTIFY immersivePlayerPanelChanged FINAL)
    Q_PROPERTY(QString sleepTimerFade READ sleepTimerFade WRITE setSleepTimerFade NOTIFY sleepTimerFadeChanged FINAL)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged FINAL)
    Q_PROPERTY(bool performanceMode READ performanceMode WRITE setPerformanceMode NOTIFY performanceModeChanged FINAL)
    /// no shapes behind the pages or the immersive player, and every other
    /// shape a plain circle, but for the favourite songs' heart
    Q_PROPERTY(bool noShapes READ noShapes WRITE setNoShapes NOTIFY noShapesChanged FINAL)
    /// "system" (a Matugen scheme), "builtin", or a theme id
    Q_PROPERTY(QString themeSource READ themeSource WRITE setThemeSource NOTIFY themeSourceChanged FINAL)
    /// "system", "light" or "dark"; selects a variant when Matugen provides both
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged FINAL)
    Q_PROPERTY(QString matugenColorsPath READ matugenColorsPath WRITE setMatugenColorsPath NOTIFY matugenColorsPathChanged FINAL)
    Q_PROPERTY(bool showWindowControls READ showWindowControls WRITE setShowWindowControls NOTIFY showWindowControlsChanged FINAL)
    Q_PROPERTY(bool windowControlsOnRight READ windowControlsOnRight WRITE setWindowControlsOnRight NOTIFY windowControlsOnRightChanged FINAL)
    Q_PROPERTY(bool nativeTitleBar READ nativeTitleBar WRITE setNativeTitleBar NOTIFY nativeTitleBarChanged FINAL)
    Q_PROPERTY(bool trayIcon READ trayIcon WRITE setTrayIcon NOTIFY trayIconChanged FINAL)
    /// the whole window's zoom, as ctrl + and ctrl - set it
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged FINAL)
    Q_PROPERTY(bool discordEnabled READ discordEnabled WRITE setDiscordEnabled NOTIFY discordEnabledChanged FINAL)
    Q_PROPERTY(QString discordApplicationId READ discordApplicationId WRITE setDiscordApplicationId NOTIFY discordApplicationIdChanged FINAL)

public:
    /// the default file, ~/.config/Swiftify/Swiftify.conf. no default argument:
    /// qml builds a singleton with a default constructor in preference to
    /// create(), and would end up with an instance of its own
    explicit Settings(QObject *parent);
    /// a specific file, for tests
    explicit Settings(const QString &path, QObject *parent = nullptr);
    ~Settings() override;

    static Settings *instance();
    static Settings *create(QQmlEngine *, QJSEngine *);

    QVariant value(const QString &key, const QVariant &fallback = {}) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);
    bool contains(const QString &key) const;
    void sync();

    /// every audio visualizer in the app
    bool visualizersEnabled() const { return m_visualizersEnabled; }
    void setVisualizersEnabled(bool value);
    /// ring around round artwork instead of a band of bars
    bool circularVisualizer() const { return m_circularVisualizer; }
    void setCircularVisualizer(bool value);
    bool roundArtwork() const { return m_roundArtwork; }
    void setRoundArtwork(bool value);
    /// bars or wave. stored under its first name, from when it applied to the ring alone
    QString visualizerShape() const { return m_visualizerShape; }
    void setVisualizerShape(const QString &value);
    double visualizerSensitivity() const { return m_visualizerSensitivity; }
    void setVisualizerSensitivity(double value);
    /// kbps: 96, 160 or 320
    int streamingQuality() const { return m_streamingQuality; }
    int songCacheMegabytes() const { return m_songCacheMegabytes; }
    void setSongCacheMegabytes(int megabytes);
    void setStreamingQuality(int value);
    bool crossfadeEnabled() const { return m_crossfadeEnabled; }
    void setCrossfadeEnabled(bool value);
    int crossfadeDurationSeconds() const { return m_crossfadeDurationSeconds; }
    void setCrossfadeDurationSeconds(int value);
    /// animate the immersive background with the music
    bool livingBackdrop() const { return m_livingBackdrop; }
    void setLivingBackdrop(bool value);
    double backdropBlur() const { return m_backdropBlur; }
    void setBackdropBlur(double value);
    double backdropReactivity() const { return m_backdropReactivity; }
    void setBackdropReactivity(double value);
    /// adds a seconds field to the sleep timer
    bool debuggingEnabled() const { return m_debuggingEnabled; }
    void setDebuggingEnabled(bool value);
    bool pinnedPlaylistsExpanded() const { return m_pinnedPlaylistsExpanded; }
    void setPinnedPlaylistsExpanded(bool value);
    bool allPlaylistsExpanded() const { return m_allPlaylistsExpanded; }
    void setAllPlaylistsExpanded(bool value);
    QString immersivePlayerPanel() const { return m_immersivePlayerPanel; }
    void setImmersivePlayerPanel(const QString &value);
    QString sleepTimerFade() const { return m_sleepTimerFade; }
    void setSleepTimerFade(const QString &value);
    /// linux only. seeded from the desktop's animation setting on first launch
    bool reduceMotion() const { return m_reduceMotion; }
    void setReduceMotion(bool value);
    /// as little drawn as can be: nothing moves, fades or turns that does not
    /// have to, beyond reduced motion's fades
    bool performanceMode() const { return m_performanceMode; }
    void setPerformanceMode(bool value);
    bool noShapes() const { return m_noShapes; }
    void setNoShapes(bool value);
    /// system follows a discovered or explicitly selected Matugen scheme
    QString themeSource() const { return m_themeSource; }
    void setThemeSource(const QString &value);
    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString &value);
    /// empty discovers Matugen JSON in common config/cache locations
    QString matugenColorsPath() const { return m_matugenColorsPath; }
    void setMatugenColorsPath(const QString &value);
    /// linux only. minimise, maximise and close in the top bar
    bool showWindowControls() const { return m_showWindowControls; }
    void setShowWindowControls(bool value);
    bool windowControlsOnRight() const { return m_windowControlsOnRight; }
    void setWindowControlsOnRight(bool value);
    bool nativeTitleBar() const { return m_nativeTitleBar; }
    void setNativeTitleBar(bool value);
    /// linux only
    bool trayIcon() const { return m_trayIcon; }
    void setTrayIcon(bool value);
    double zoom() const { return m_zoom; }
    void setZoom(double value);
    void setLanguage(const QString &value);
    bool discordEnabled() const { return m_discordEnabled; }
    void setDiscordEnabled(bool value);
    QString discordApplicationId() const { return m_discordApplicationId; }
    void setDiscordApplicationId(const QString &value);

signals:
    void visualizersEnabledChanged();
    void circularVisualizerChanged();
    void roundArtworkChanged();
    void visualizerShapeChanged();
    void visualizerSensitivityChanged();
    void streamingQualityChanged();
    void crossfadeEnabledChanged();
    void crossfadeDurationSecondsChanged();
    void songCacheMegabytesChanged();
    void livingBackdropChanged();
    void backdropBlurChanged();
    void backdropReactivityChanged();
    void debuggingEnabledChanged();
    void pinnedPlaylistsExpandedChanged();
    void allPlaylistsExpandedChanged();
    void immersivePlayerPanelChanged();
    void sleepTimerFadeChanged();
    void reduceMotionChanged();
    void performanceModeChanged();
    void noShapesChanged();
    void themeSourceChanged();
    void themeModeChanged();
    void matugenColorsPathChanged();
    void showWindowControlsChanged();
    void windowControlsOnRightChanged();
    void nativeTitleBarChanged();
    void trayIconChanged();
    void zoomChanged();
    void discordEnabledChanged();
    void discordApplicationIdChanged();

private:
    void load();

    QSettings m_store;

    bool m_visualizersEnabled = true;
    bool m_circularVisualizer = false;
    bool m_roundArtwork = false;
    QString m_visualizerShape = QStringLiteral("bars");
    double m_visualizerSensitivity = 1.0;
    int m_streamingQuality = 160;
    bool m_crossfadeEnabled = false;
    int m_crossfadeDurationSeconds = 5;
    int m_songCacheMegabytes = 1000;
    bool m_livingBackdrop = true;
    double m_backdropBlur = 16.0;
    double m_backdropReactivity = 0.3;
    bool m_debuggingEnabled = false;
    bool m_pinnedPlaylistsExpanded = false;
    bool m_allPlaylistsExpanded = false;
    QString m_immersivePlayerPanel = QStringLiteral("lyrics");
    QString m_sleepTimerFade = QStringLiteral("finalSeconds");
    bool m_reduceMotion = false;
    bool m_performanceMode = false;
    bool m_noShapes = false;
    QString m_themeSource = QStringLiteral("spotify");
    QString m_themeMode = QStringLiteral("system");
    QString m_matugenColorsPath = QString();
    bool m_showWindowControls = true;
    bool m_windowControlsOnRight = false;
#ifdef Q_OS_WIN
    bool m_nativeTitleBar = false;
#else
    bool m_nativeTitleBar = true;
#endif
    bool m_trayIcon = true;
    double m_zoom = 1.0;
    bool m_discordEnabled = false;
    QString m_discordApplicationId;
};

} // namespace swiftify
