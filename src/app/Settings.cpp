#include "app/Settings.h"

#include <QQmlEngine>
#include <QRegularExpression>

#include <algorithm>
#include <initializer_list>

namespace swiftify {

namespace {

Settings *s_instance = nullptr;

QString oneOf(const QString &value, std::initializer_list<QString> allowed, const QString &fallback)
{
    return std::find(allowed.begin(), allowed.end(), value) != allowed.end() ? value : fallback;
}

int quality(int kbps, int fallback)
{
    return kbps == 96 || kbps == 160 || kbps == 320 ? kbps : fallback;
}

template <typename T>
T stored(const QSettings &store, const char *key, const T &fallback)
{
    const QVariant value = store.value(QLatin1StringView(key));
    return value.isValid() && value.canConvert<T>() ? value.value<T>() : fallback;
}

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_store(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Swiftify"),
              QStringLiteral("Swiftify"))
{
    load();
    if (!s_instance) {
        s_instance = this;
    }
}

Settings::Settings(const QString &path, QObject *parent)
    : QObject(parent)
    , m_store(path, QSettings::IniFormat)
{
    load();
    // a test's settings are the ones its views read, never the real file
    if (!s_instance) {
        s_instance = this;
    }
}

Settings::~Settings()
{
    m_store.sync();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

Settings *Settings::instance()
{
    return s_instance;
}

Settings *Settings::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariant Settings::value(const QString &key, const QVariant &fallback) const
{
    return m_store.value(key, fallback);
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    m_store.setValue(key, value);
}

void Settings::remove(const QString &key)
{
    m_store.remove(key);
}

bool Settings::contains(const QString &key) const
{
    return m_store.contains(key);
}

void Settings::sync()
{
    m_store.sync();
}

void Settings::load()
{
    m_visualizersEnabled = stored<bool>(m_store, "visualizersEnabled", m_visualizersEnabled);
    m_playerBarVisualizer = stored<bool>(m_store, "playerBarVisualizer", m_playerBarVisualizer);
    m_trackListVisualizer = stored<bool>(m_store, "trackListVisualizer", m_trackListVisualizer);
    m_immersiveVisualizer = stored<bool>(m_store, "immersiveVisualizer", m_immersiveVisualizer);
    m_circularVisualizer = stored<bool>(m_store, "circularVisualizer", m_circularVisualizer);
    m_roundArtwork = stored<bool>(m_store, "roundArtwork", m_roundArtwork);
    {
        const QString raw = stored<QString>(m_store, "circularVisualizerShape", m_visualizerShape);
        m_visualizerShape = oneOf(raw, {QStringLiteral("bars"), QStringLiteral("wave")}, QStringLiteral("bars"));
    }
    {
        const double raw = stored<double>(m_store, "visualizerSensitivity", m_visualizerSensitivity);
        m_visualizerSensitivity = std::clamp(raw, 0.6, 1.8);
    }
    {
        const int raw = stored<int>(m_store, "streamingQuality", m_streamingQuality);
        m_streamingQuality = quality(raw, 160);
        const int cache = stored<int>(m_store, "songCacheMegabytes", m_songCacheMegabytes);
        m_songCacheMegabytes = cache == 500 || cache == 2000 ? cache : 1000;
    }
    m_crossfadeEnabled = stored<bool>(m_store, "crossfadeEnabled", m_crossfadeEnabled);
    m_spotifyConnect = stored<bool>(m_store, "reportToSpotify", m_spotifyConnect);
    m_connectDeviceName = stored<QString>(m_store, "connectDeviceName", m_connectDeviceName).trimmed().left(64);
    if (m_connectDeviceName.isEmpty()) {
        m_connectDeviceName = QStringLiteral("Swiftify");
    }
    m_crossfadeDurationSeconds = std::clamp(
        stored<int>(m_store, "crossfadeDurationSeconds", m_crossfadeDurationSeconds), 1, 12);
    m_livingBackdrop = stored<bool>(m_store, "livingBackdrop", m_livingBackdrop);
    {
        const double raw = stored<double>(m_store, "backdropBlur", m_backdropBlur);
        m_backdropBlur = std::clamp(raw, 0.0, 60.0);
    }
    {
        const double raw = stored<double>(m_store, "backdropReactivity", m_backdropReactivity);
        m_backdropReactivity = std::clamp(raw, 0.0, 3.0);
    }
    m_debuggingEnabled = stored<bool>(m_store, "debuggingEnabled", m_debuggingEnabled);
    m_pinnedPlaylistsExpanded = stored<bool>(m_store, "pinnedPlaylistsExpanded", m_pinnedPlaylistsExpanded);
    m_allPlaylistsExpanded = stored<bool>(m_store, "allPlaylistsExpanded", m_allPlaylistsExpanded);
    {
        const QString raw = stored<QString>(m_store, "immersivePlayerPanel", m_immersivePlayerPanel);
        m_immersivePlayerPanel = oneOf(raw, {QStringLiteral("artwork"), QStringLiteral("lyrics"), QStringLiteral("queue"), QStringLiteral("sleep"), QStringLiteral("audio")}, QStringLiteral("lyrics"));
    }
    {
        const QString raw = stored<QString>(m_store, "sleepTimerFade", m_sleepTimerFade);
        m_sleepTimerFade = oneOf(raw, {QStringLiteral("none"), QStringLiteral("finalSeconds"), QStringLiteral("entire")}, QStringLiteral("finalSeconds"));
    }
    m_reduceMotion = stored<bool>(m_store, "reduceMotion", m_reduceMotion);
    m_performanceMode = stored<bool>(m_store, "performanceMode", m_performanceMode);
    m_noShapes = stored<bool>(m_store, "noShapes", m_noShapes);
    {
        // an unknown theme falls back to the app's own colours in Theme
        const QString raw = stored<QString>(m_store, "themeSource", m_themeSource).trimmed();
        if (!raw.isEmpty()) {
            m_themeSource = raw;
        }
    }
    {
        const QString raw = stored<QString>(m_store, "themeMode", m_themeMode);
        m_themeMode = oneOf(raw, {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")}, QStringLiteral("system"));
    }
    {
        const QString raw = stored<QString>(m_store, "matugenColorsPath", m_matugenColorsPath);
        m_matugenColorsPath = raw.trimmed();
    }
    m_showWindowControls = stored<bool>(m_store, "showWindowControls", m_showWindowControls);
    m_windowControlsOnRight = stored<bool>(m_store, "windowControlsOnRight", m_windowControlsOnRight);
    m_nativeTitleBar = stored<bool>(m_store, "nativeTitleBar", m_nativeTitleBar);
    m_trayIcon = stored<bool>(m_store, "trayIcon", m_trayIcon);
    m_zoom = std::clamp(stored<double>(m_store, "zoom", m_zoom), 0.5, 2.0);
    m_discordEnabled = stored<bool>(m_store, "discordEnabled", false);
    const QString id = stored<QString>(m_store, "discordApplicationId", {}).trimmed();
    if (QRegularExpression(QStringLiteral("^[0-9]{17,20}$")).match(id).hasMatch()) {
        m_discordApplicationId = id;
    }
    m_language = oneOf(stored<QString>(m_store, "language", m_language),
                      {QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("ru")}, QStringLiteral("system"));

}

void Settings::setVisualizersEnabled(bool value)
{
    if (m_visualizersEnabled == value) {
        return;
    }
    m_visualizersEnabled = value;
    m_store.setValue(QStringLiteral("visualizersEnabled"), value);
    emit visualizersEnabledChanged();
}

void Settings::setPlayerBarVisualizer(bool value)
{
    if (m_playerBarVisualizer == value) {
        return;
    }
    m_playerBarVisualizer = value;
    m_store.setValue(QStringLiteral("playerBarVisualizer"), value);
    emit playerBarVisualizerChanged();
}

void Settings::setTrackListVisualizer(bool value)
{
    if (m_trackListVisualizer == value) {
        return;
    }
    m_trackListVisualizer = value;
    m_store.setValue(QStringLiteral("trackListVisualizer"), value);
    emit trackListVisualizerChanged();
}

void Settings::setImmersiveVisualizer(bool value)
{
    if (m_immersiveVisualizer == value) {
        return;
    }
    m_immersiveVisualizer = value;
    m_store.setValue(QStringLiteral("immersiveVisualizer"), value);
    emit immersiveVisualizerChanged();
}

void Settings::setCircularVisualizer(bool value)
{
    if (m_circularVisualizer == value) {
        return;
    }
    m_circularVisualizer = value;
    m_store.setValue(QStringLiteral("circularVisualizer"), value);
    emit circularVisualizerChanged();
}

void Settings::setRoundArtwork(bool value)
{
    if (m_roundArtwork == value) {
        return;
    }
    m_roundArtwork = value;
    m_store.setValue(QStringLiteral("roundArtwork"), value);
    emit roundArtworkChanged();
}

void Settings::setVisualizerShape(const QString &raw)
{
    const QString value = oneOf(raw, {QStringLiteral("bars"), QStringLiteral("wave")}, QStringLiteral("bars"));
    if (m_visualizerShape == value) {
        return;
    }
    m_visualizerShape = value;
    m_store.setValue(QStringLiteral("circularVisualizerShape"), value);
    emit visualizerShapeChanged();
}

void Settings::setVisualizerSensitivity(double raw)
{
    const double value = std::clamp(raw, 0.6, 1.8);
    if (m_visualizerSensitivity == value) {
        return;
    }
    m_visualizerSensitivity = value;
    m_store.setValue(QStringLiteral("visualizerSensitivity"), value);
    emit visualizerSensitivityChanged();
}

void Settings::setStreamingQuality(int raw)
{
    const int value = quality(raw, 160);
    if (m_streamingQuality == value) {
        return;
    }
    m_streamingQuality = value;
    m_store.setValue(QStringLiteral("streamingQuality"), value);
    emit streamingQualityChanged();
}

void Settings::setSongCacheMegabytes(int megabytes)
{
    const int value = megabytes == 500 || megabytes == 2000 ? megabytes : 1000;
    if (m_songCacheMegabytes == value) {
        return;
    }
    m_songCacheMegabytes = value;
    m_store.setValue(QStringLiteral("songCacheMegabytes"), value);
    emit songCacheMegabytesChanged();
}

void Settings::setSpotifyConnect(bool value)
{
    if (m_spotifyConnect == value) {
        return;
    }
    m_spotifyConnect = value;
    m_store.setValue(QStringLiteral("reportToSpotify"), value);
    emit spotifyConnectChanged();
}

void Settings::setConnectDeviceName(const QString &raw)
{
    const QString value = raw.trimmed().left(64);
    if (value.isEmpty() || m_connectDeviceName == value) {
        return;
    }
    m_connectDeviceName = value;
    m_store.setValue(QStringLiteral("connectDeviceName"), value);
    emit connectDeviceNameChanged();
}

void Settings::setCrossfadeEnabled(bool value)
{
    if (m_crossfadeEnabled == value) {
        return;
    }
    m_crossfadeEnabled = value;
    m_store.setValue(QStringLiteral("crossfadeEnabled"), value);
    emit crossfadeEnabledChanged();
}

void Settings::setCrossfadeDurationSeconds(int raw)
{
    const int value = std::clamp(raw, 1, 12);
    if (m_crossfadeDurationSeconds == value) {
        return;
    }
    m_crossfadeDurationSeconds = value;
    m_store.setValue(QStringLiteral("crossfadeDurationSeconds"), value);
    emit crossfadeDurationSecondsChanged();
}

void Settings::setLivingBackdrop(bool value)
{
    if (m_livingBackdrop == value) {
        return;
    }
    m_livingBackdrop = value;
    m_store.setValue(QStringLiteral("livingBackdrop"), value);
    emit livingBackdropChanged();
}

void Settings::setBackdropBlur(double raw)
{
    const double value = std::clamp(raw, 0.0, 60.0);
    if (m_backdropBlur == value) {
        return;
    }
    m_backdropBlur = value;
    m_store.setValue(QStringLiteral("backdropBlur"), value);
    emit backdropBlurChanged();
}

void Settings::setBackdropReactivity(double raw)
{
    const double value = std::clamp(raw, 0.0, 3.0);
    if (m_backdropReactivity == value) {
        return;
    }
    m_backdropReactivity = value;
    m_store.setValue(QStringLiteral("backdropReactivity"), value);
    emit backdropReactivityChanged();
}

void Settings::setDebuggingEnabled(bool value)
{
    if (m_debuggingEnabled == value) {
        return;
    }
    m_debuggingEnabled = value;
    m_store.setValue(QStringLiteral("debuggingEnabled"), value);
    emit debuggingEnabledChanged();
}

void Settings::setPinnedPlaylistsExpanded(bool value)
{
    if (m_pinnedPlaylistsExpanded == value) {
        return;
    }
    m_pinnedPlaylistsExpanded = value;
    m_store.setValue(QStringLiteral("pinnedPlaylistsExpanded"), value);
    emit pinnedPlaylistsExpandedChanged();
}

void Settings::setAllPlaylistsExpanded(bool value)
{
    if (m_allPlaylistsExpanded == value) {
        return;
    }
    m_allPlaylistsExpanded = value;
    m_store.setValue(QStringLiteral("allPlaylistsExpanded"), value);
    emit allPlaylistsExpandedChanged();
}

void Settings::setImmersivePlayerPanel(const QString &raw)
{
    const QString value = oneOf(raw, {QStringLiteral("artwork"), QStringLiteral("lyrics"), QStringLiteral("queue"), QStringLiteral("sleep"), QStringLiteral("audio")}, QStringLiteral("lyrics"));
    if (m_immersivePlayerPanel == value) {
        return;
    }
    m_immersivePlayerPanel = value;
    m_store.setValue(QStringLiteral("immersivePlayerPanel"), value);
    emit immersivePlayerPanelChanged();
}

void Settings::setSleepTimerFade(const QString &raw)
{
    const QString value = oneOf(raw, {QStringLiteral("none"), QStringLiteral("finalSeconds"), QStringLiteral("entire")}, QStringLiteral("finalSeconds"));
    if (m_sleepTimerFade == value) {
        return;
    }
    m_sleepTimerFade = value;
    m_store.setValue(QStringLiteral("sleepTimerFade"), value);
    emit sleepTimerFadeChanged();
}

void Settings::setReduceMotion(bool value)
{
    if (m_reduceMotion == value) {
        return;
    }
    m_reduceMotion = value;
    m_store.setValue(QStringLiteral("reduceMotion"), value);
    emit reduceMotionChanged();
}

void Settings::setPerformanceMode(bool value)
{
    if (m_performanceMode == value) {
        return;
    }
    m_performanceMode = value;
    m_store.setValue(QStringLiteral("performanceMode"), value);
    emit performanceModeChanged();
}

void Settings::setNoShapes(bool value)
{
    if (m_noShapes == value) {
        return;
    }
    m_noShapes = value;
    m_store.setValue(QStringLiteral("noShapes"), value);
    emit noShapesChanged();
}

void Settings::setThemeSource(const QString &raw)
{
    const QString value = raw.trimmed();
    if (value.isEmpty() || m_themeSource == value) {
        return;
    }
    m_themeSource = value;
    m_store.setValue(QStringLiteral("themeSource"), value);
    emit themeSourceChanged();
}

void Settings::setThemeMode(const QString &raw)
{
    const QString value = oneOf(raw, {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")}, QStringLiteral("system"));
    if (m_themeMode == value) {
        return;
    }
    m_themeMode = value;
    m_store.setValue(QStringLiteral("themeMode"), value);
    emit themeModeChanged();
}

void Settings::setMatugenColorsPath(const QString &raw)
{
    const QString value = raw.trimmed();
    if (m_matugenColorsPath == value) {
        return;
    }
    m_matugenColorsPath = value;
    m_store.setValue(QStringLiteral("matugenColorsPath"), value);
    emit matugenColorsPathChanged();
}

void Settings::setShowWindowControls(bool value)
{
    if (m_showWindowControls == value) {
        return;
    }
    m_showWindowControls = value;
    m_store.setValue(QStringLiteral("showWindowControls"), value);
    emit showWindowControlsChanged();
}

void Settings::setWindowControlsOnRight(bool value)
{
    if (m_windowControlsOnRight == value) {
        return;
    }
    m_windowControlsOnRight = value;
    m_store.setValue(QStringLiteral("windowControlsOnRight"), value);
    emit windowControlsOnRightChanged();
}

void Settings::setNativeTitleBar(bool value)
{
    if (m_nativeTitleBar == value) {
        return;
    }
    m_nativeTitleBar = value;
    m_store.setValue(QStringLiteral("nativeTitleBar"), value);
    emit nativeTitleBarChanged();
}

void Settings::setTrayIcon(bool value)
{
    if (m_trayIcon == value) {
        return;
    }
    m_trayIcon = value;
    m_store.setValue(QStringLiteral("trayIcon"), value);
    emit trayIconChanged();
}

void Settings::setZoom(double raw)
{
    const double value = std::clamp(raw, 0.5, 2.0);
    if (qFuzzyCompare(m_zoom, value)) {
        return;
    }
    m_zoom = value;
    m_store.setValue(QStringLiteral("zoom"), value);
    emit zoomChanged();
}


void Settings::setDiscordEnabled(bool value)
{
    if (m_discordEnabled == value) return;
    m_discordEnabled = value;
    m_store.setValue(QStringLiteral("discordEnabled"), value);
    emit discordEnabledChanged();
}

void Settings::setDiscordApplicationId(const QString &raw)
{
    const QString value = raw.trimmed();
    if (!value.isEmpty() && !QRegularExpression(QStringLiteral("^[0-9]{17,20}$")).match(value).hasMatch()) return;
    if (m_discordApplicationId == value) return;
    m_discordApplicationId = value;
    m_store.setValue(QStringLiteral("discordApplicationId"), value);
    emit discordApplicationIdChanged();
}
void Settings::setLanguage(const QString &raw)
{
    const QString value = oneOf(raw, {QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("ru")}, QStringLiteral("system"));
    if (m_language == value) return;
    m_language = value;
    m_store.setValue(QStringLiteral("language"), value);
    emit languageChanged();
}



} // namespace swiftify
