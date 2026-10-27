#include "app/AppController.h"

#include "app/Settings.h"
#include "library/CollectionModels.h"
#include "library/LibraryStore.h"
#include "library/SongListModel.h"
#include "playback/PlaybackController.h"

#include <QClipboard>
#include <QFile>
#include <QMap>
#include <QQuickItem>
#include <QTextStream>
#include <QQuickWindow>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QUrl>
#include <QQmlEngine>
#include <QWindow>
#include <QtGui/qevent.h>

#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#include "system/windows/WindowsIntegration.h"
#include <QSystemTrayIcon>
#endif

#ifdef Q_OS_MACOS
#include <malloc/malloc.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

#if defined(SWIFTIFY_HAS_DBUS) && defined(Q_OS_LINUX)
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#endif

namespace swiftify {

namespace {
AppController *s_instance = nullptr;
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    if (!s_instance) {
        s_instance = this;
    }
    m_trim.setSingleShot(true);
    m_trim.setInterval(1500);
    connect(&m_trim, &QTimer::timeout, this, &AppController::collect);
    m_pointerRest.setSingleShot(true);
    m_pointerRest.setInterval(kPointerRestMs);
    connect(&m_pointerRest, &QTimer::timeout, this, [this] { setPointerResting(true); });
    m_pointerRest.start();
    if (qApp) {
        qApp->installEventFilter(this);
    }
#ifdef Q_OS_WIN
    m_trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
#endif
#if defined(SWIFTIFY_HAS_DBUS) && defined(Q_OS_LINUX)
    // qt's tray icon on linux is a StatusNotifierItem, which needs a watcher
    // to register with; without one it has nowhere to appear
    const QString watcher = QStringLiteral("org.kde.StatusNotifierWatcher");
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        auto *watch = new QDBusServiceWatcher(watcher, bus,
                                              QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
        connect(watch, &QDBusServiceWatcher::serviceRegistered, this, [this] { setTrayAvailable(true); });
        connect(watch, &QDBusServiceWatcher::serviceUnregistered, this, [this] { setTrayAvailable(false); });
        m_trayAvailable = bus.interface()->isServiceRegistered(watcher);
    }
#endif
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    // the pointer moving over a window, as the system reports it: qt
    // delivers hover again with every frame drawn, from where it already was
    const QEvent::Type type = event->type();
    // however a quit was asked for, qt delivers it to the app before it
    // closes the windows
    if (watched == qApp && type == QEvent::Quit && !m_quitting) {
        m_quitting = true;
        emit quittingChanged();
    }
    if ((type == QEvent::MouseMove || type == QEvent::MouseButtonPress || type == QEvent::Wheel)
        && watched->isWindowType()) {
        const auto *pointer = static_cast<const QSinglePointEvent *>(event);
        if (type != QEvent::MouseMove || pointer->globalPosition() != m_pointer) {
            m_pointer = pointer->globalPosition();
            setPointerResting(false);
            m_pointerRest.start();
        }
    }
#ifdef Q_OS_MACOS
    // the dock icon clicked while every window is closed brings the main one
    // back, as on any mac app. qt reports the click as the app becoming
    // active, even when it already was.
    if (watched == qApp && event->type() == QEvent::ApplicationStateChange
        && static_cast<QApplicationStateChangeEvent *>(event)->applicationState() == Qt::ApplicationActive) {
        const QWindowList windows = QGuiApplication::topLevelWindows();
        const bool showing = std::any_of(windows.cbegin(), windows.cend(), [](const QWindow *window) {
            return window->isVisible() && window->type() != Qt::Popup && window->type() != Qt::ToolTip;
        });
        if (!showing && !m_quitting) {
            emit raiseRequested();
        }
    }
#endif
    return QObject::eventFilter(watched, event);
}

void AppController::setPointerResting(bool resting)
{
    if (m_pointerResting == resting) {
        return;
    }
    m_pointerResting = resting;
    emit pointerRestingChanged();
}

void AppController::setTrayAvailable(bool available)
{
    if (m_trayAvailable == available) {
        return;
    }
    m_trayAvailable = available;
    emit trayAvailableChanged();
}

AppController::~AppController()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

AppController *AppController::instance()
{
    return s_instance;
}

AppController *AppController::create(QQmlEngine *, QJSEngine *engine)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    if (s_instance) {
        s_instance->m_engine = engine;
    }
    return s_instance;
}

void AppController::setNativeTitleBar(QObject *target, bool enabled) const
{
#ifdef Q_OS_WIN
    WindowsIntegration::setNativeTitleBar(qobject_cast<QWindow *>(target), enabled);
#else
    Q_UNUSED(target)
    Q_UNUSED(enabled)
#endif
}

void AppController::trimMemory()
{
    m_trim.start();
}

void AppController::collect()
{
    // script objects first: what they own is deleted later, from the event
    // loop, and only then is there anything to hand back
    if (m_engine) {
        m_engine->collectGarbage();
    }
    QTimer::singleShot(250, this, &AppController::releaseMemory);
}

void AppController::releaseMemory()
{
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (auto *quick = qobject_cast<QQuickWindow *>(window); quick && quick->isVisible()) {
            quick->releaseResources();
        }
    }
#ifdef Q_OS_MACOS
    malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__GLIBC__)
    malloc_trim(0);
#endif
}

QString AppController::version() const
{
    return QStringLiteral(SWIFTIFY_VERSION);
}

bool AppController::hasDesktopIntegration() const
{
#ifdef SWIFTIFY_DESKTOP_INTEGRATION
    return true;
#else
    return false;
#endif
}

void AppController::handleCommand(const QString &command)
{
    PlaybackController *player = PlaybackController::instance();
    LibraryStore *library = LibraryStore::instance();
    if (command == QLatin1StringView("immersive")) {
        emit immersiveRequested();
    } else if (command == QLatin1StringView("close-immersive")) {
        emit immersiveCloseRequested();
    } else if (command == QLatin1StringView("home") && library) {
        library->showHome();
    } else if (command == QLatin1StringView("playlists") && library) {
        library->showPlaylists();
    } else if (command == QLatin1StringView("songs") && library) {
        library->showSavedSongs();
    } else if (command == QLatin1StringView("stats") && library) {
        library->showStats();
    } else if (command == QLatin1StringView("back") && library) {
        library->goBack();
    } else if (command == QLatin1StringView("play-recent") && library && player) {
        // what played last, from the start of the list
        if (library->recentlyPlayed()->count() > 0) {
            player->playFromModel(library->recentlyPlayed(), 0);
        }
    } else if (command.startsWith(QLatin1StringView("setting ")) && Settings::instance()) {
        // "setting <name> on|off", for the settings that change what is drawn
        const QStringList words = command.split(u' ', Qt::SkipEmptyParts);
        if (words.size() == 3) {
            const bool on = words.at(2) == QLatin1StringView("on");
            Settings *settings = Settings::instance();
            if (words.at(1) == QLatin1StringView("reduce-motion")) {
                settings->setReduceMotion(on);
            } else if (words.at(1) == QLatin1StringView("performance")) {
                settings->setPerformanceMode(on);
            } else if (words.at(1) == QLatin1StringView("visualizers")) {
                settings->setVisualizersEnabled(on);
            } else if (words.at(1) == QLatin1StringView("no-shapes")) {
                settings->setNoShapes(on);
            }
        }
    } else if (command.startsWith(QLatin1StringView("census "))) {
        writeCensus(command.mid(7));
    } else if (command.startsWith(QLatin1StringView("playlist ")) && library) {
        // the playlist at that place in the library's list, from 0
        bool number = false;
        const int index = command.mid(9).toInt(&number);
        if (number && index >= 0 && index < library->playlists()->count()) {
            library->selectPlaylist(library->playlists()->at(index));
        }
    } else if (command == QLatin1StringView("play-loading-test") && player) {
        player->playLoadingTest();
    } else if (command == QLatin1StringView("play-pause") && player) {
        player->togglePlayback();
    } else if (command == QLatin1StringView("next") && player) {
        player->skipForward();
    } else if (command == QLatin1StringView("previous") && player) {
        player->skipBackward();
    } else {
        emit raiseRequested();
    }
}

namespace {

QString typeName(const QObject *object)
{
    QString name = QString::fromLatin1(object->metaObject()->className());
    if (const qsizetype mark = name.indexOf(QLatin1StringView("_QMLTYPE_")); mark >= 0) {
        name.truncate(mark);
    }
    if (const qsizetype mark = name.indexOf(QLatin1StringView("_QML_")); mark >= 0) {
        name.truncate(mark);
    }
    return name;
}

bool isOwnComponent(const QObject *object)
{
    const QString name = QString::fromLatin1(object->metaObject()->className());
    return name.contains(QLatin1StringView("_QMLTYPE_")) || name.startsWith(QLatin1StringView("swiftify::"));
}

void census(QQuickItem *item, const QString &owner, const QRectF &window, QMap<QString, int> &drawn)
{
    if (!item->isVisible() || item->opacity() <= 0.001) {
        return;
    }
    const QString here = isOwnComponent(item) ? typeName(item) : owner;
    if (item->flags().testFlag(QQuickItem::ItemHasContents) && item->mapRectToScene(item->boundingRect()).intersects(window)) {
        ++drawn[here + QStringLiteral(" / ") + typeName(item)];
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        census(child, here, window, drawn);
    }
}

} // namespace

void AppController::writeCensus(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    // every window qt knows of, shown or not: the display link looks at each
    // of them on every tick
    for (QWindow *window : QGuiApplication::allWindows()) {
        out << "window " << window->metaObject()->className() << " \"" << window->title() << "\" visible "
            << window->isVisible() << " native " << (window->handle() != nullptr) << "\n";
    }
    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        auto *quick = qobject_cast<QQuickWindow *>(window);
        if (!quick || !quick->isVisible()) {
            continue;
        }
        QMap<QString, int> drawn;
        census(quick->contentItem(), QStringLiteral("(window)"), QRectF(QPointF(), quick->size()), drawn);
        int total = 0;
        QList<std::pair<int, QString>> sorted;
        for (auto it = drawn.cbegin(); it != drawn.cend(); ++it) {
            total += it.value();
            sorted.append({it.value(), it.key()});
        }
        std::sort(sorted.begin(), sorted.end(), std::greater<>());
        out << "== " << quick->title() << " " << quick->width() << "x" << quick->height() << ": " << total
            << " items drawing\n";
        for (const auto &[count, owner] : std::as_const(sorted)) {
            out << QString::number(count).rightJustified(5) << "  " << owner << "\n";
        }
        // every face the window's text is laid out in, shown or not: each
        // is a font engine of its own, with its own copy of the font's tables
        QMap<QString, int> faces;
        QList<QQuickItem *> items {quick->contentItem()};
        for (qsizetype index = 0; index < items.size(); ++index) {
            items.append(items.at(index)->childItems());
        }
        for (const QQuickItem *item : std::as_const(items)) {
            const QVariant font = item->property("font");
            if (!font.isValid() || !item->metaObject()->className()) {
                continue;
            }
            const QFont face = font.value<QFont>();
            QStringList axes;
            for (const QFont::Tag &tag : face.variableAxisTags()) {
                axes << QString::fromLatin1(tag.toString()) + u'=' + QString::number(face.variableAxisValue(tag));
            }
            const int renderType = item->property("renderType").toInt();
            ++faces[QStringLiteral("%1 %2px w%3 %4%5")
                        .arg(face.family())
                        .arg(face.pixelSize())
                        .arg(face.weight())
                        .arg(axes.join(u','))
                        .arg(renderType == 1 ? QStringLiteral(" native") : QString())];
        }
        out << "-- " << faces.size() << " faces\n";
        for (auto it = faces.cbegin(); it != faces.cend(); ++it) {
            out << QString::number(it.value()).rightJustified(5) << "  " << it.key() << "\n";
        }
    }
}

void AppController::quit()
{
    if (!m_quitting) {
        m_quitting = true;
        emit quittingChanged();
    }
    QCoreApplication::quit();
}

void AppController::copyToClipboard(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

void AppController::openUrl(const QString &url) const
{
    QDesktopServices::openUrl(QUrl(url));
}

} // namespace swiftify
