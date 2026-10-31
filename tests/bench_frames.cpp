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

#include "FakeCore.h"
#include "ViewRig.h"

#include "app/Fonts.h"
#include "library/CollectionModels.h"

#include <QAnimationDriver>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSGGeometryNode>
#include <QTemporaryDir>
#include <private/qquickwindow_p.h>
#include <private/qsgabstractrenderer_p.h>
#include <private/qsgrenderer_p.h>

#include <cxxabi.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/qos.h>
#include <malloc/malloc.h>
#endif
#include <pthread.h>
#include <ctime>
#include <map>
#include <utility>
#include <vector>

using namespace swiftify;
using namespace Qt::StringLiterals;
using swiftify::testing::ViewRig;

namespace {

/// what a frame of the main window costs on the cpu, not a test: run by hand
/// (QT_QPA_PLATFORM=offscreen bench_frames) before and after a change to
/// what the window draws. the window is built as main() builds it, around a
/// library with covers, a song playing and visualizers on.

#ifdef __APPLE__
// what the render loop wraps each frame in on macos: metal's command buffers
// and descriptors are autoreleased, and only a pool's end lets them go
extern "C" void *objc_autoreleasePoolPush();
extern "C" void objc_autoreleasePoolPop(void *pool);
#endif

double threadMs()
{
    timespec now {};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return double(now.tv_sec) * 1e3 + double(now.tv_nsec) / 1e6;
}

/// animations step a frame at a time, as they would at 60 hz
class StepDriver : public QAnimationDriver {
public:
    void step()
    {
        m_elapsed += 16;
        advance();
    }
    qint64 elapsed() const override { return m_elapsed; }

private:
    qint64 m_elapsed = 0;
};

/// the footprint activity monitor shows, and what of it is the heap
QString memory()
{
#ifdef __APPLE__
    task_vm_info_data_t info {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count);
    malloc_statistics_t heap {};
    malloc_zone_statistics(nullptr, &heap);
    return QStringLiteral("footprint %1 MB, heap in use %2 MB")
        .arg(double(info.phys_footprint) / 1048576.0, 0, 'f', 1)
        .arg(double(heap.size_in_use) / 1048576.0, 0, 'f', 1);
#else
    return QStringLiteral("footprint - MB, heap in use - MB");
#endif
}

QStringList *capturedRenderLines = nullptr;
QtMessageHandler previousHandler = nullptr;

void captureRenderer(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    // QSG_RENDERER_DEBUG=render: a line for every batch of every frame
    static const QStringList kRendererLines {QStringLiteral(" - 0x"), QStringLiteral(" -> "),
                                             QStringLiteral("Renderer::render()"), QStringLiteral("Batch thresholds"),
                                             QStringLiteral("Rendering:")};
    if (message.startsWith(QLatin1StringView("Renderer::render()")) && capturedRenderLines) {
        capturedRenderLines->append(QStringLiteral("PASS ") + message);
    }
    for (const QString &start : kRendererLines) {
        if (message.startsWith(start)) {
            if (capturedRenderLines && start == kRendererLines.first()) {
                capturedRenderLines->append(message);
            }
            return;
        }
    }
    previousHandler(type, context, message);
}

QString demangled(const char *name)
{
    int status = 0;
    char *text = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    QString result = QString::fromLatin1(status == 0 && text ? text : name);
    std::free(text);
    return result;
}

/// the drawn nodes in the order they are drawn, a letter for each material:
/// a run of the same letter can be one draw, a change cannot
void sequence(QSGNode *node, QString &out)
{
    if (node->isSubtreeBlocked()) {
        return;
    }
    if (node->type() == QSGNode::ClipNodeType) {
        out += u'[';
    }
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGMaterial *material = static_cast<QSGGeometryNode *>(node)->activeMaterial();
        const QString name = material ? demangled(typeid(*material).name()) : QString();
        QChar letter = u'?';
        if (name.contains(QLatin1StringView("ShaderEffect"))) letter = u'E';
        else if (name.contains(QLatin1StringView("DistanceField"))) letter = u't';
        else if (name.contains(QLatin1StringView("TextMask"))) letter = u'i';
        else if (name.contains(QLatin1StringView("SmoothColor"))) letter = u'r';
        else if (name.contains(QLatin1StringView("ShapeMaterial"))) letter = u's';
        else if (name.contains(QLatin1StringView("SymbolMaterial"))) letter = u'y';
        else if (name.contains(QLatin1StringView("ShadowMaterial"))) letter = u'h';
        else if (name.contains(QLatin1StringView("Texture"))) letter = u'p';
        else if (name.contains(QLatin1StringView("Spectrum"))) letter = u'v';
        else if (name.contains(QLatin1StringView("VertexColor"))) letter = u'c';
        out += letter;
    }
    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling()) {
        sequence(child, out);
    }
    if (node->type() == QSGNode::ClipNodeType) {
        out += u']';
    }
}

void countNodes(QSGNode *node, std::map<QString, int> &byMaterial, int &total)
{
    if (node->isSubtreeBlocked()) {
        return;
    }
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGMaterial *material = static_cast<QSGGeometryNode *>(node)->activeMaterial();
        ++byMaterial[material ? demangled(typeid(*material).name()) : QStringLiteral("(none)")];
        ++total;
    }
    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling()) {
        countNodes(child, byMaterial, total);
    }
}

QString typeName(const QObject *object)
{
    QString name = QString::fromLatin1(object->metaObject()->className());
    if (const qsizetype mark = name.indexOf(QLatin1StringView("_QMLTYPE_")); mark >= 0) {
        name.truncate(mark);
    }
    return name;
}

bool isOwnComponent(const QObject *object)
{
    const QString name = QString::fromLatin1(object->metaObject()->className());
    return name.contains(QLatin1StringView("_QMLTYPE_")) || name.startsWith(QLatin1StringView("swiftify::"));
}

/// what draws: every item with contents, shown and inside the window, by
/// the nearest component of the app's own it belongs to
void countItems(QQuickItem *item, const QString &owner, const QRectF &window, std::map<QString, int> &byOwner)
{
    if (!item->isVisible() || item->opacity() <= 0.001) {
        return;
    }
    const QString here = isOwnComponent(item) ? typeName(item) : owner;
    if (item->flags().testFlag(QQuickItem::ItemHasContents)
        && item->mapRectToScene(item->boundingRect()).intersects(window)) {
        ++byOwner[here + QStringLiteral(" / ") + typeName(item)];
    }
    for (QQuickItem *child : item->childItems()) {
        countItems(child, here, window, byOwner);
    }
}

const char *kShell = R"(
import QtQuick
import Swiftify

Item {
    id: root
    property int page: 0
    /// the lyrics beside the page, as the inspector shows them
    property bool lyrics: false
    width: 1280
    height: 780

    Rectangle { anchors.fill: parent; color: Appearance.colors.layer0 }
    Rectangle {
        id: inspector
        anchors.right: parent.right
        anchors.rightMargin: root.lyrics ? 8 : 0
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: root.lyrics ? Appearance.sizes.sideSheetWidth : 0
        visible: root.lyrics && root.page !== 3
        radius: Appearance.rounding.large
        color: Appearance.colors.layer1

        LyricsPanel {
            anchors.fill: parent
            visible: root.lyrics
        }
    }

    Component { id: home; HomeView {} }
    Component { id: savedSongs; SavedSongsView {} }
    Component { id: playlists; PlaylistsView {} }

    Loader {
        anchors.fill: parent
        active: root.page === 3
        sourceComponent: ImmersivePlayer {}
    }

    // the top bar asks its window about being maximized; never shown
    Window { id: hostWindow; visible: false }

    Sidebar {
        id: sidebar
        visible: root.page !== 3
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 36
        anchors.leftMargin: 8
        width: 232
    }

    Rectangle {
        id: sheet
        // nothing under the opaque immersive player is drawn, as in Main
        visible: root.page !== 3
        anchors.left: sidebar.right
        anchors.leftMargin: 8
        anchors.right: inspector.left
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        radius: Appearance.rounding.large
        color: Appearance.colors.layer1

        TopBar {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            window: hostWindow
        }
        // the page host shows a page only with the library connected,
        // which takes a login; its pages are shown the same way here
        Loader {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBar.bottom
            anchors.bottom: parent.bottom
            clip: true
            sourceComponent: [home, savedSongs, playlists, null][root.page]
        }
        PlayerBar {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            width: Math.max(560, Math.min(720, sheet.width - 36))
        }
    }
}
)";

} // namespace

class FrameBench : public QObject {
    Q_OBJECT

private:
    QList<QUrl> m_covers;
    /// covers much like spotify's: 640 pixels, jpeg, each its own
    void makeCovers(const QString &directory, int count)
    {
        QRandomGenerator random(7);
        for (int index = 0; index < count; ++index) {
            QImage image(640, 640, QImage::Format_RGB32);
            QPainter painter(&image);
            QLinearGradient gradient(0, 0, 640, 640);
            gradient.setColorAt(0, QColor::fromHsv(int(random.bounded(360)), 180, 220));
            gradient.setColorAt(1, QColor::fromHsv(int(random.bounded(360)), 200, 90));
            painter.fillRect(image.rect(), gradient);
            for (int shape = 0; shape < 40; ++shape) {
                painter.setBrush(QColor::fromHsv(int(random.bounded(360)), 160, 200, 120));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(random.bounded(640.0), random.bounded(640.0)), random.bounded(90.0),
                                    random.bounded(90.0));
            }
            painter.end();
            const QString path = directory + QStringLiteral("/cover%1.jpg").arg(index);
            image.save(path, "JPG", 85);
            m_covers.append(QUrl::fromLocalFile(path));
        }
    }

    void fillLibrary(ViewRig &rig)
    {
        makeCovers(rig.directory.path(), 40);
        QList<Playlist> playlists;
        for (int index = 0; index < 30; ++index) {
            Playlist playlist;
            playlist.id = QStringLiteral("p%1").arg(index);
            playlist.name = QStringLiteral("Playlist number %1").arg(index);
            playlist.ownerName = QStringLiteral("someone");
            playlist.songCount = 40 + index;
            playlist.artworkUrl = m_covers.at(index);
            playlist.uri = QStringLiteral("spotify:playlist:p%1").arg(index);
            playlists.append(playlist);
        }
        rig.library.playlists()->setItems(playlists);
        rig.library.pinnedPlaylists()->setItems(playlists.mid(0, 6));

        QList<Song> recent;
        for (int index = 0; index < 20; ++index) {
            recent.append(song(index));
        }
        rig.library.recentlyPlayed()->setSongs(recent);

        QList<Artist> artists;
        QList<Album> albums;
        for (int index = 0; index < 10; ++index) {
            Artist artist;
            artist.id = QStringLiteral("a%1").arg(index);
            artist.name = QStringLiteral("Artist %1").arg(index);
            artist.artworkUrl = m_covers.at(20 + index);
            artists.append(artist);
            Album album;
            album.id = QStringLiteral("al%1").arg(index);
            album.name = QStringLiteral("Album %1").arg(index);
            album.artists = QStringLiteral("Artist %1").arg(index);
            album.artworkUrl = m_covers.at(30 + index);
            albums.append(album);
        }
        rig.library.recommendedArtists()->setItems(artists);
        rig.library.recommendedAlbums()->setItems(albums);

        QList<Song> saved;
        for (int index = 0; index < 200; ++index) {
            saved.append(song(index));
        }
        rig.library.savedSongs()->setSongs(saved);
    }

    Song song(int index) const
    {
        Song item;
        item.instanceId = quint64(1000 + index);
        item.name = QStringLiteral("A song with a fairly long title %1").arg(index);
        item.artists = QStringLiteral("Some Artist, Another One");
        item.albumName = QStringLiteral("The Album %1").arg(index % 12);
        item.artworkUrl = m_covers.at(index % m_covers.size());
        item.uri = QStringLiteral("spotify:track:%1").arg(index, 22, 10, QLatin1Char('0'));
        item.durationMs = 180'000 + index * 1000;
        return item;
    }

    void play(ViewRig &rig)
    {
        QList<Song> queue;
        for (int index = 0; index < 4; ++index) {
            queue.append(song(index));
        }
        rig.connectPlayback();
        rig.player.play(queue[0], queue);
        ViewRig::settle();
        if (const auto call = rig.core.take(QStringLiteral("playTrack"))) {
            call->succeed();
        }
        // synced lyrics beside the first, a line every three seconds
        if (const auto call = rig.core.take(QStringLiteral("lyrics"))) {
            QJsonArray lines;
            for (int line = 0; line < 60; ++line) {
                lines.append(QJsonObject {{QStringLiteral("startTimeMs"), line * 3000},
                                          {QStringLiteral("words"), QStringLiteral("A line of the song, number %1").arg(line)}});
            }
            call->succeed(QString::fromUtf8(QJsonDocument(QJsonObject {{QStringLiteral("syncType"), QStringLiteral("lineSynced")},
                                                                       {QStringLiteral("lines"), lines}})
                                                .toJson(QJsonDocument::Compact)));
        }
        ViewRig::settle();
        rig.core.duration = 180'000;
        rig.core.musicLevels = true;
        ViewRig::settle();
        qInfo().noquote() << "playing:" << rig.player.isPlaying();
    }

    struct Cost {
        double syncMs = 0;
        double renderMs = 0;
    };

    Cost frames(ViewRig &rig, StepDriver &driver, int count)
    {
        QQuickRenderControl *control = rig.control.get();
        Cost cost;
        for (int frame = 0; frame < count; ++frame) {
#ifdef __APPLE__
            void *pool = objc_autoreleasePoolPush();
            const auto drained = qScopeGuard([pool] { objc_autoreleasePoolPop(pool); });
#endif
            driver.step();
            QCoreApplication::processEvents();
            // as the app's event loop would, which deletes what was let go of
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            const double start = threadMs();
            control->polishItems();
            control->beginFrame();
            control->sync();
            const double synced = threadMs();
            control->render();
            control->endFrame();
            const double rendered = threadMs();
            cost.syncMs += synced - start;
            cost.renderMs += rendered - synced;
        }
        cost.syncMs /= count;
        cost.renderMs /= count;
        return cost;
    }

    /// how many frames a second the window would draw on a 60 hz display,
    /// with real time passing and the song moving on: a frame is drawn at a
    /// display tick only if something asked for one since the last
    double frameRate(ViewRig &rig, StepDriver &driver, double seconds)
    {
        bool wanted = true;
        const auto mark = [&wanted] { wanted = true; };
        const auto a = connect(rig.control.get(), &QQuickRenderControl::renderRequested, this, mark);
        const auto b = connect(rig.control.get(), &QQuickRenderControl::sceneChanged, this, mark);
        driver.uninstall();
        QElapsedTimer clock;
        clock.start();
        qint64 next = 0;
        int drawn = 0;
        const quint32 start = rig.core.position;
        while (clock.elapsed() < seconds * 1000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            rig.core.position = start + quint32(clock.elapsed());
            if (clock.elapsed() < next) {
                QThread::usleep(500);
                continue;
            }
            next += 1000 / 60;
            if (wanted) {
                wanted = false;
#ifdef __APPLE__
                void *pool = objc_autoreleasePoolPush();
#endif
                rig.control->polishItems();
                rig.control->beginFrame();
                rig.control->sync();
                rig.control->render();
                rig.control->endFrame();
#ifdef __APPLE__
                objc_autoreleasePoolPop(pool);
#endif
                ++drawn;
            }
        }
        driver.install();
        disconnect(a);
        disconnect(b);
        return drawn / seconds;
    }

    void report(const char *name, ViewRig &rig, StepDriver &driver)
    {
        // covers decode off the thread; give them time to arrive and fade in
        QElapsedTimer settling;
        settling.start();
        while (settling.elapsed() < 2500) {
            frames(rig, driver, 1);
            QThread::msleep(4);
        }

        QStringList lines;
        capturedRenderLines = &lines;
        frames(rig, driver, 1);
        capturedRenderLines = nullptr;

        std::map<QString, int> byMaterial;
        int total = 0;
        QSGRenderer *renderer = QQuickWindowPrivate::get(rig.window.get())->renderer;
        if (renderer && renderer->rootNode()) {
            countNodes(renderer->rootNode(), byMaterial, total);
        }

        std::map<QString, int> byOwner;
        countItems(rig.window->contentItem(), QStringLiteral("(window)"), QRectF(QPointF(), QSizeF(ViewRig::kSize)), byOwner);

        // the least of a few runs: the scheduler moves the thread between
        // fast and slow cores, and the slow ones only add noise
        const int count = qEnvironmentVariableIsSet("BENCH_FRAMES") ? qEnvironmentVariable("BENCH_FRAMES").toInt() : 400;
        Cost cost = frames(rig, driver, count);
        for (int run = 0; run < 4; ++run) {
            const Cost next = frames(rig, driver, count);
            cost.syncMs = std::min(cost.syncMs, next.syncMs);
            cost.renderMs = std::min(cost.renderMs, next.renderMs);
        }
        if (const QByteArray path = qgetenv("BENCH_FOOTPRINT_EACH"); !path.isEmpty()) {
            const QByteArray command = "footprint -v -p " + QByteArray::number(QCoreApplication::applicationPid()) + " > " + path + "_"
                + QByteArray(name).replace(" ", "_").replace(",", "") + ".txt";
            std::system(command.constData());
        }
        if (const QByteArray path = qgetenv("BENCH_HEAP");
            !path.isEmpty() && QLatin1StringView(name).startsWith(qEnvironmentVariable("BENCH_HEAP_AT", QStringLiteral("home")))) {
            const QByteArray pid = QByteArray::number(QCoreApplication::applicationPid());
            const QByteArray command = qEnvironmentVariableIsSet("MallocStackLogging")
                ? "malloc_history " + pid + " -callTree -consolidateAllBySymbol > " + path + " 2>/dev/null"
                : "heap -s " + pid + " > " + path;
            std::system(command.constData());
        }
        const double rate = qEnvironmentVariableIsSet("BENCH_RATE") ? frameRate(rig, driver, 4) : -1;
        qInfo().noquote() << QStringLiteral("== %1: sync+polish %2 ms, render %3 ms a frame; %4%5")
                                 .arg(QLatin1StringView(name))
                                 .arg(cost.syncMs, 0, 'f', 3)
                                 .arg(cost.renderMs, 0, 'f', 3)
                                 .arg(memory())
                                 .arg(rate >= 0 ? QStringLiteral("; %1 frames a second at 60 hz").arg(rate, 0, 'f', 1) : QString());
        const qsizetype passes = std::count_if(lines.cbegin(), lines.cend(), [](const QString &line) {
            return line.startsWith(QLatin1StringView("PASS "));
        });
        qInfo().noquote() << QStringLiteral("  %1 renderers drew in the frame (the window and each layer redrawn)").arg(passes);
        int merged = 0;
        int unmerged = 0;
        int mergedNodes = 0;
        static const QRegularExpression kNodes(QStringLiteral("Nodes:\\s+(\\d+)"));
        for (const QString &line : std::as_const(lines)) {
            if (line.startsWith(QLatin1StringView("PASS "))) {
                continue;
            }
            const bool isMerged = !line.contains(QLatin1StringView("unmerged"));
            (isMerged ? merged : unmerged) += 1;
            if (isMerged) {
                mergedNodes += kNodes.match(line).captured(1).toInt();
            }
        }
        if (qEnvironmentVariableIsSet("BENCH_SEQUENCE")) {
            QStringList sizes;
            for (const QString &line : std::as_const(lines)) {
                if (line.startsWith(QLatin1StringView("PASS "))) {
                    continue;
                }
                sizes << kNodes.match(line).captured(1) + (line.contains(QLatin1StringView("unmerged")) ? u"u"_s : QString());
            }
            qInfo().noquote() << "  nodes in each batch:" << sizes.join(u' ');
        }
        if (qEnvironmentVariableIsSet("BENCH_BATCH_LINES")) {
            for (const QString &line : std::as_const(lines)) {
                qInfo().noquote() << "   " << line;
            }
        }
        qInfo().noquote() << QStringLiteral("  %1 batches: %2 merged (of %3 nodes), %4 unmerged")
                                 .arg(merged + unmerged)
                                 .arg(merged)
                                 .arg(mergedNodes)
                                 .arg(unmerged);
        qInfo().noquote() << QStringLiteral("  %1 geometry nodes drawn").arg(total);
        std::vector<std::pair<int, QString>> sorted;
        for (const auto &[material, count] : byMaterial) {
            sorted.emplace_back(count, material);
        }
        std::sort(sorted.rbegin(), sorted.rend());
        for (const auto &[count, material] : sorted) {
            qInfo().noquote() << QStringLiteral("  %1  %2").arg(count, 4).arg(material.left(90));
        }
        if (qEnvironmentVariableIsSet("BENCH_SEQUENCE") && renderer && renderer->rootNode()) {
            QString order;
            sequence(renderer->rootNode(), order);
            qInfo().noquote() << "  drawn in order:" << order;
        }
        if (qEnvironmentVariableIsSet("BENCH_ITEMS")) {
            std::vector<std::pair<int, QString>> items;
            for (const auto &[owner, count] : byOwner) {
                items.emplace_back(count, owner);
            }
            std::sort(items.rbegin(), items.rend());
            qInfo().noquote() << "  items drawing, by the component they are part of:";
            for (const auto &[count, owner] : items) {
                qInfo().noquote() << QStringLiteral("    %1  %2").arg(count, 4).arg(owner);
            }
        }
    }

private slots:
    void initTestCase()
    {
#ifdef __APPLE__
        // scheduled as the app's render thread is, on the fast cores
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
        // a line for every batch of every frame, formatted even when thrown
        // away: on only for counting, as it costs more than some frames
        if (qEnvironmentVariableIsSet("BENCH_BATCHES")) {
            qputenv("QSG_RENDERER_DEBUG", "render");
        }
        qputenv("QSG_ATLAS_WIDTH", "64");
        qputenv("QSG_ATLAS_HEIGHT", "64");
        qputenv("QSG_ATLAS_SIZE_LIMIT", "1");
        qputenv("QSG_TRANSIENT_IMAGES", "1");
        qputenv("QT_MAX_CACHED_GLYPH_SIZE", "512");
        previousHandler = qInstallMessageHandler(captureRenderer);
#ifdef __APPLE__
        // the default the cocoa platform gives; the offscreen one names a
        // "Sans Serif" no mac has, and looking for it loads every font there
        // is to find out what else it might be called
        QGuiApplication::setFont(QFont(QStringLiteral(".AppleSystemUIFont"), 13));
#endif
        swiftify::fonts::loadIconFont();
        swiftify::fonts::loadInterfaceFont();
        qputenv("QT_QUICK_BACKEND", "rhi");
#ifdef __APPLE__
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#endif
    }

    void mainWindow()
    {
        // before anything installs a driver of its own
        StepDriver driver;
        driver.install();
        ViewRig rig;
        QVERIFY(rig.gpu);
        rig.engine.addImageProvider(QStringLiteral("art"), new ArtworkImageProvider);
        rig.settings.setVisualizersEnabled(true);
        rig.settings.setReduceMotion(qEnvironmentVariableIsSet("BENCH_REDUCE_MOTION"));
        // the settings the app ran with when it was measured
        if (qEnvironmentVariableIsSet("BENCH_ROUND")) {
            rig.settings.setRoundArtwork(true);
            rig.settings.setCircularVisualizer(true);
        }
        fillLibrary(rig);
        // as the app does: the window first, the music after
        const bool playLater = qEnvironmentVariableIsSet("BENCH_PLAY_LATER");
        if (!playLater) {
            play(rig);
        }

        qInfo().noquote() << "before the window:" << memory();
        QQmlComponent component(&rig.engine);
        // a scene of its own instead of the window, to try something alone
        if (const QString scene = qEnvironmentVariable("BENCH_SCENE"); !scene.isEmpty()) {
            component.loadUrl(QUrl::fromLocalFile(scene));
        } else {
            component.setData(kShell, QUrl(QStringLiteral("qrc:/bench/Shell.qml")));
        }
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *shell = qobject_cast<QQuickItem *>(object.get());
        shell->setParentItem(rig.window->contentItem());
        if (playLater) {
            frames(rig, driver, 30);
            play(rig);
        }

        if (qEnvironmentVariableIsSet("BENCH_IMMERSIVE_ONLY")) {
            shell->setProperty("page", 3);
            report("immersive player", rig, driver);
            driver.uninstall();
            return;
        }
        report("home", rig, driver);
        if (qEnvironmentVariableIsSet("BENCH_TOGGLE")) {
            const auto rate = [&](const char *what) {
                qInfo().noquote() << QStringLiteral("  %1: %2 frames a second").arg(QLatin1StringView(what)).arg(frameRate(rig, driver, 3), 0, 'f', 1);
            };
            rate("as built");
            rig.settings.setReduceMotion(!rig.settings.reduceMotion());
            rate("reduce motion flipped");
            rig.settings.setReduceMotion(!rig.settings.reduceMotion());
            rate("and back");
            rig.settings.setVisualizersEnabled(false);
            rate("visualizers off");
            rig.settings.setVisualizersEnabled(true);
            rate("visualizers on again");
            rig.player.togglePlayback();
            rate("paused");
            rig.player.togglePlayback();
            ViewRig::settle();
            if (const auto call = rig.core.take(QStringLiteral("resume"))) {
                call->succeed();
            }
            rate("resumed");
        }
        if (qEnvironmentVariableIsSet("BENCH_LYRICS")) {
            shell->setProperty("lyrics", true);
            report("home with lyrics", rig, driver);
            shell->setProperty("lyrics", false);
        }
        if (qEnvironmentVariableIsSet("BENCH_HOME_ONLY")) {
            return;
        }

        shell->setProperty("page", 1);
        report("saved songs, 200 songs", rig, driver);

        shell->setProperty("page", 2);
        report("playlists", rig, driver);

        shell->setProperty("page", 3);
        report("immersive player", rig, driver);

        // round again: what a page left behind is either kept for the next
        // time, and the same, or grows every time round
        if (qEnvironmentVariableIsSet("BENCH_CYCLE")) {
            for (int round = 0; round < 3; ++round) {
                for (int page = 0; page < 3; ++page) {
                    shell->setProperty("page", page);
                    frames(rig, driver, 60);
                }
                rig.engine.collectGarbage();
                // outside an event loop's exec(), objects let go of with
                // deleteLater wait for one; the app's loop deletes them
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                frames(rig, driver, 2);
                qInfo().noquote() << "round" << round << "after a collection:" << memory();
            }
        }

        if (const QByteArray path = qgetenv("BENCH_FOOTPRINT"); !path.isEmpty()) {
            const QByteArray command = "footprint -p " + QByteArray::number(QCoreApplication::applicationPid()) + " > " + path;
            std::system(command.constData());
        }
        driver.uninstall();
    }
};

QTEST_MAIN(FrameBench)
#include "bench_frames.moc"
