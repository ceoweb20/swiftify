#include "ViewRig.h"

#include "app/Fonts.h"
#include "system/windows/WindowsIntegration.h"

#include <QQuickStyle>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>

#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>

using namespace swiftify;
using swiftify::testing::ViewRig;

class WindowsTest : public QObject {
    Q_OBJECT

    void verifyTitleBar(QQuickWindow *window, const QString &stage)
    {
        window->raise();
        QTest::qWait(300);
        const HWND handle = reinterpret_cast<HWND>(window->winId());
        const qreal scale = window->devicePixelRatio();
        const QColor background = window->color();
        BOOL dark = FALSE;
        QCOMPARE(DwmGetWindowAttribute(handle, 20, &dark, sizeof(dark)), S_OK);
        QCOMPARE(bool(dark), background.lightnessF() < 0.5);
        DWORD corners = 0;
        QCOMPARE(DwmGetWindowAttribute(handle, 33, &corners, sizeof(corners)), S_OK);
        QCOMPARE(corners, DWORD(2));
        RECT frame {};
        RECT client {};
        RECT buttons {};
        RECT visible {};
        POINT origin {0, 0};
        QVERIFY(GetWindowRect(handle, &frame));
        QVERIFY(GetClientRect(handle, &client));
        QVERIFY(ClientToScreen(handle, &origin));
        QCOMPARE(DwmGetWindowAttribute(handle, 5, &buttons, sizeof(buttons)), S_OK);
        QCOMPARE(DwmGetWindowAttribute(handle, 9, &visible, sizeof(visible)), S_OK);
        QVERIFY(origin.y > frame.top);
        QVERIFY(frame.top + buttons.bottom <= origin.y);
        QVERIFY2(qAbs(client.right - qRound(window->width() * scale)) <= 1,
            qPrintable(QStringLiteral("%1: client width %2, Qt width %3 at %4x").arg(stage).arg(client.right).arg(window->width()).arg(scale)));
        QVERIFY2(qAbs(client.bottom - qRound(window->height() * scale)) <= 1,
            qPrintable(QStringLiteral("%1: client height %2, Qt height %3 at %4x").arg(stage).arg(client.bottom).arg(window->height()).arg(scale)));
        const QPoint captionCenter((visible.left + visible.right) / 2, (visible.top + origin.y) / 2);
        QCOMPARE(SendMessageW(handle, WM_NCHITTEST, 0, MAKELPARAM(captionCenter.x(), captionCenter.y())), LRESULT(HTCAPTION));
        const QImage rendered = window->grabWindow();
        QVERIFY(!rendered.isNull());
        QCOMPARE(rendered.pixelColor(rendered.width() - 2, 2).alpha(), 255);
        DwmFlush();
        QScreen *screen = window->screen();
        const QPoint screenOrigin = screen->geometry().topLeft();
        const QPoint position(qRound((visible.left - screenOrigin.x()) / scale),
                              qRound((visible.top - screenOrigin.y()) / scale));
        const QImage desktop = screen->grabWindow(0, position.x(), position.y(),
            qRound((visible.right - visible.left) / scale), qRound((visible.bottom - visible.top) / scale)).toImage();
        QVERIFY(!desktop.isNull());
        const QString preview = qEnvironmentVariable("SWIFTIFY_WINDOW_PREVIEW");
        if (!preview.isEmpty()) QVERIFY(desktop.save(preview + QStringLiteral(".%1.png").arg(stage)));
        const QColor actual = desktop.pixelColor(captionCenter - QPoint(visible.left, visible.top));
        QVERIFY(qAbs(actual.red() - background.red()) <= 3);
        QVERIFY(qAbs(actual.green() - background.green()) <= 3);
        QVERIFY(qAbs(actual.blue() - background.blue()) <= 3);
        for (int index = 0; index < 3; ++index) {
            const int buttonLeft = frame.left + buttons.left - visible.left + (buttons.right - buttons.left) * index / 3;
            const int buttonRight = frame.left + buttons.left - visible.left + (buttons.right - buttons.left) * (index + 1) / 3;
            const int buttonTop = frame.top + buttons.top - visible.top;
            const int buttonBottom = frame.top + buttons.bottom - visible.top;
            int glyphPixels = 0;
            for (int row = qMax(0, buttonTop + 2); row < qMin(desktop.height(), buttonBottom - 2); ++row) {
                for (int column = qMax(0, buttonLeft + 2); column < qMin(desktop.width(), buttonRight - 2); ++column) {
                    const int lightness = desktop.pixelColor(column, row).lightness();
                    if (qAbs(lightness - background.lightness()) > 32) ++glyphPixels;
                }
            }
            QVERIFY2(glyphPixels > 5, qPrintable(stage + QStringLiteral(": native button %1 must be visible").arg(index)));
        }
    }

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QGuiApplication::setQuitOnLastWindowClosed(false);
        fonts::loadIconFont();
        fonts::loadInterfaceFont();
    }

    void closingKeepsTheAppRunning_data()
    {
        QTest::addColumn<bool>("trayEnabled");
        QTest::addColumn<bool>("playing");
        QTest::addColumn<bool>("nativeTitleBar");
        for (bool nativeTitleBar : {true, false}) {
            const QByteArray prefix = nativeTitleBar ? "native-" : "in-app-";
            QTest::newRow(prefix + "idle-with-tray") << true << false << nativeTitleBar;
            QTest::newRow(prefix + "idle-without-tray") << false << false << nativeTitleBar;
            QTest::newRow(prefix + "playing-with-tray") << true << true << nativeTitleBar;
            QTest::newRow(prefix + "playing-without-tray") << false << true << nativeTitleBar;
        }
    }

    void closingKeepsTheAppRunning()
    {
        QFETCH(bool, trayEnabled);
        QFETCH(bool, playing);
        QFETCH(bool, nativeTitleBar);
        ViewRig rig;
        rig.settings.setTrayIcon(trayEnabled);
        rig.settings.setNativeTitleBar(nativeTitleBar);
        if (playing) rig.playSomething();
        QQmlComponent component(&rig.engine, QUrl(QStringLiteral("qrc:/qt/qml/Swiftify/qml/Main.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(window);
        QVERIFY(rig.app.isWindows());
        QCOMPARE(window->flags().testFlag(Qt::FramelessWindowHint), !nativeTitleBar);
        QTRY_VERIFY(window->isVisible());
        QSignalSpy closing(window, &QWindow::visibleChanged);
        QVERIFY(!window->close());
        QTRY_VERIFY(!window->isVisible());
        if (rig.app.isTrayAvailable()) {
            QTRY_VERIFY(window->findChild<QObject *>(QStringLiteral("swiftifyTrayIcon")));
        }
        QVERIFY(!rig.app.isQuitting());
        QCOMPARE(rig.player.isPlaying(), playing);
        QVERIFY(!closing.isEmpty());
        rig.app.handleCommand(QStringLiteral("raise"));
        QTRY_VERIFY(window->isVisible());
    }

    void contentUsesTheFullClientArea_data()
    {
        QTest::addColumn<int>("windowWidth");
        QTest::addColumn<int>("panelWidth");
        QTest::newRow("narrow") << 526 << 0;
        QTest::newRow("normal") << 1280 << 0;
        QTest::newRow("panel-open") << 1280 << 320;
        QTest::newRow("panel-moving") << 1280 << 80;
    }

    void contentUsesTheFullClientArea()
    {
        QFETCH(int, windowWidth);
        QFETCH(int, panelWidth);
        ViewRig rig;
        rig.settings.setPerformanceMode(true);
        QQmlComponent component(&rig.engine, QUrl(QStringLiteral("qrc:/qt/qml/Swiftify/qml/Main.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(window);
        window->setWidth(windowWidth);
        auto *inspector = window->findChild<QQuickItem *>(QStringLiteral("mainInspector"));
        auto *toolbar = window->findChild<QQuickItem *>(QStringLiteral("mainTopBar"));
        auto *lyrics = window->findChild<QQuickItem *>(QStringLiteral("mainLyricsPanel"));
        auto *sheet = window->findChild<QQuickItem *>(QStringLiteral("mainSheet"));
        QVERIFY(inspector && toolbar && lyrics && sheet);
        inspector->setWidth(panelWidth);
        ViewRig::settle();
        QCOMPARE(sheet->y(), 8.0);
        QCOMPARE(inspector->y(), 8.0);
        QCOMPARE(sheet->x() + sheet->width(), inspector->x() - 8);
        QCOMPARE(sheet->y() + sheet->height(), window->height() - 8);
        QCOMPARE(toolbar->width(), sheet->width());
        QCOMPARE(lyrics->y(), 0.0);
        const auto immersive = rig.show(QStringLiteral("ImmersivePlayer"));
        QVERIFY(immersive);
        immersive->setWidth(windowWidth);
        ViewRig::settle();
        auto *switcher = immersive->findChild<QQuickItem *>(QStringLiteral("immersiveSwitcher"));
        QVERIFY(switcher);
        QVERIFY(switcher->x() >= 70);
        QCOMPARE(switcher->x() + switcher->width(), windowWidth - 24.0);
    }

    void inAppControlsStayInTheTopLeft()
    {
        ViewRig rig;
        QVERIFY(!rig.settings.nativeTitleBar());
        rig.settings.setShowWindowControls(false);
        rig.settings.setPerformanceMode(true);
        QQmlComponent component(&rig.engine, QUrl(QStringLiteral("qrc:/qt/qml/Swiftify/qml/Main.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(window);
        auto *controls = window->findChild<QQuickItem *>(QStringLiteral("appWindowControls"));
        auto *sidebar = window->findChild<QQuickItem *>(QStringLiteral("mainSidebar"));
        auto *toolbar = window->findChild<QQuickItem *>(QStringLiteral("mainTopBar"));
        QVERIFY(controls && sidebar && toolbar);
        QVERIFY(controls->isVisible());
        auto *close = controls->findChild<QQuickItem *>(QStringLiteral("closeWindowButton"));
        auto *maximize = controls->findChild<QQuickItem *>(QStringLiteral("maximizeWindowButton"));
        auto *minimize = controls->findChild<QQuickItem *>(QStringLiteral("minimizeWindowButton"));
        QVERIFY(close && maximize && minimize);
        ViewRig::settle();
        QCOMPARE(close->x(), 0.0);
        QVERIFY(close->x() + close->width() < maximize->x());
        QVERIFY(maximize->x() + maximize->width() < minimize->x());
        rig.session.setInitialLibraryLoader({});
        rig.session.restoreSavedSessions();
        QTRY_VERIFY(!rig.session.isEvaluatingStoredLogin());
        for (int width : {526, 1280}) {
            window->setWidth(width);
            ViewRig::settle();
            QCOMPARE(controls->mapToScene(QPointF()), QPointF(12, 8));
            QVERIFY(controls->x() + controls->width() < sidebar->x() + sidebar->width());
            QVERIFY(controls->y() + controls->height() <= sidebar->y());
            QVERIFY(!toolbar->findChild<QObject *>(QStringLiteral("closeWindowButton")));
        }
        rig.playSomething();
        rig.app.handleCommand(QStringLiteral("immersive"));
        QTRY_COMPARE(window->property("immersiveReveal").toReal(), 1.0);
        auto *immersiveClose = window->findChild<QQuickItem *>(QStringLiteral("immersiveCloseButton"));
        QVERIFY(immersiveClose);
        QVERIFY(controls->isVisible());
        QCOMPARE(immersiveClose->mapToScene(QPointF()), QPointF(24, 42));
        QVERIFY(immersiveClose->y() >= controls->y() + controls->height() + 6);
        QVERIFY(immersiveClose->y() + immersiveClose->height() <= immersiveClose->parentItem()->height());
        const QString preview = qEnvironmentVariable("SWIFTIFY_WINDOW_PREVIEW");
        if (!preview.isEmpty() && QGuiApplication::platformName() == QLatin1StringView("windows")) {
            QTest::qWait(300);
            const qreal scale = window->devicePixelRatio();
            const QImage frame = window->grabWindow();
            QVERIFY(!frame.isNull());
            QVERIFY(frame.copy(0, 0, qRound(180 * scale), qRound(110 * scale))
                        .save(preview + QStringLiteral(".immersive-controls.png")));
        }
        rig.settings.setNativeTitleBar(true);
        QTRY_VERIFY(!controls->isVisible());
        QTRY_VERIFY(!window->flags().testFlag(Qt::FramelessWindowHint));
        QCOMPARE(immersiveClose->x(), 24.0);
        QCOMPARE(immersiveClose->y(), 18.0);
        rig.settings.setNativeTitleBar(false);
        QTRY_VERIFY(controls->isVisible());
        QCOMPARE(immersiveClose->mapToScene(QPointF()), QPointF(24, 42));
        rig.app.handleCommand(QStringLiteral("close-immersive"));
        QTRY_COMPARE(window->property("immersiveReveal").toReal(), 0.0);
        const auto settings = rig.show(QStringLiteral("SettingsPages"));
        QVERIFY(settings);
        settings->setProperty("page", 3);
        auto *selector = settings->findChild<QQuickItem *>(QStringLiteral("windowControlsMode"));
        QVERIFY(selector && selector->isVisible());
        QCOMPARE(selector->property("currentIndex").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 0)));
        QVERIFY(rig.settings.nativeTitleBar());
        QTRY_VERIFY(!controls->isVisible());
        QCOMPARE(sidebar->y(), 8.0);
    }

    void inAppFrame_data()
    {
        nativeTitleBar_data();
    }

    void inAppFrame()
    {
        if (QGuiApplication::platformName() != QLatin1StringView("windows")) QSKIP("Requires the Windows platform plugin");
        QFETCH(int, screenIndex);
        QScreen *screen = QGuiApplication::screens().at(screenIndex);
        ViewRig rig;
        rig.settings.setNativeTitleBar(false);
        rig.settings.setPerformanceMode(true);
        rig.session.setInitialLibraryLoader({});
        rig.session.restoreSavedSessions();
        QTRY_VERIFY(!rig.session.isEvaluatingStoredLogin());
        WindowsIntegration integration(&rig.player, &rig.theme);
        QQmlComponent component(&rig.engine, QUrl(QStringLiteral("qrc:/qt/qml/Swiftify/qml/Main.qml")));
        const QRect available = screen->availableGeometry();
        const QSize size = QSize(1000, 650).boundedTo(available.size() - QSize(100, 100));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("x"), available.x() + 40}, {QStringLiteral("y"), available.y() + 60},
            {QStringLiteral("width"), size.width()}, {QStringLiteral("height"), size.height()},
            {QStringLiteral("visible"), false}
        }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(window);
        integration.setWindow(window);
        window->show();
        QTRY_VERIFY(window->isExposed());
        QTRY_COMPARE(window->screen(), screen);
        HWND handle = reinterpret_cast<HWND>(window->winId());
        QTRY_VERIFY(GetWindowLongPtrW(handle, GWL_STYLE) & WS_THICKFRAME);
        QVERIFY(!(GetWindowLongPtrW(handle, GWL_STYLE) & WS_CAPTION));
        auto *minimize = window->findChild<QQuickItem *>(QStringLiteral("minimizeWindowButton"));
        auto *maximize = window->findChild<QQuickItem *>(QStringLiteral("maximizeWindowButton"));
        auto *close = window->findChild<QQuickItem *>(QStringLiteral("closeWindowButton"));
        QVERIFY(minimize && maximize && close);
        const auto checkBounds = [&] {
            RECT frame {};
            RECT client {};
            POINT origin {0, 0};
            QVERIFY(GetWindowRect(handle, &frame));
            QVERIFY(GetClientRect(handle, &client));
            QVERIFY(ClientToScreen(handle, &origin));
            QCOMPARE(origin.x, frame.left);
            QCOMPARE(origin.y, frame.top);
            QCOMPARE(client.right, frame.right - frame.left);
            QCOMPARE(client.bottom, frame.bottom - frame.top);
            QVERIFY(qAbs(client.right - qRound(window->width() * window->devicePixelRatio())) <= 1);
            QVERIFY(qAbs(client.bottom - qRound(window->height() * window->devicePixelRatio())) <= 1);
            const int centerX = (frame.left + frame.right) / 2;
            const int centerY = (frame.top + frame.bottom) / 2;
            QCOMPARE(SendMessageW(handle, WM_NCHITTEST, 0, MAKELPARAM(centerX, frame.top + 1)), LRESULT(HTTOP));
            QCOMPARE(SendMessageW(handle, WM_NCHITTEST, 0, MAKELPARAM(frame.right - 2, centerY)), LRESULT(HTRIGHT));
            QCOMPARE(SendMessageW(handle, WM_NCHITTEST, 0, MAKELPARAM(frame.left + 1, frame.bottom - 2)), LRESULT(HTBOTTOMLEFT));
            DWORD corners = 0;
            QCOMPARE(DwmGetWindowAttribute(handle, 33, &corners, sizeof(corners)), S_OK);
            QCOMPARE(corners, DWORD(2));
        };
        checkBounds();
        const QString preview = qEnvironmentVariable("SWIFTIFY_WINDOW_PREVIEW");
        if (!preview.isEmpty()) {
            window->raise();
            window->requestActivate();
            QTest::qWait(300);
            DwmFlush();
            RECT visible {};
            QCOMPARE(DwmGetWindowAttribute(handle, 9, &visible, sizeof(visible)), S_OK);
            const qreal scale = window->devicePixelRatio();
            const QPoint origin = screen->geometry().topLeft();
            const QImage desktop = screen->grabWindow(0, qRound((visible.left - origin.x()) / scale),
                qRound((visible.top - origin.y()) / scale), qRound((visible.right - visible.left) / scale),
                qRound((visible.bottom - visible.top) / scale)).toImage();
            QVERIFY(desktop.save(preview + QStringLiteral(".in-app-%1.png").arg(screenIndex)));
        }
        const auto moveToScreen = [&](QScreen *destination) {
            const QPoint offset = destination->availableGeometry().topLeft() - destination->geometry().topLeft() + QPoint(40, 40);
            const QPoint position = destination->geometry().topLeft() + (QPointF(offset) * destination->devicePixelRatio()).toPoint();
            QVERIFY(SetWindowPos(handle, nullptr, position.x(), position.y(), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
            QTRY_COMPARE(window->screen(), destination);
            checkBounds();
        };
        for (QScreen *destination : QGuiApplication::screens()) moveToScreen(destination);
        moveToScreen(screen);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, maximize->mapToScene(QPointF(maximize->width() / 2, maximize->height() / 2)).toPoint());
        QTRY_COMPARE(window->windowState(), Qt::WindowMaximized);
        MONITORINFO monitor {sizeof(MONITORINFO)};
        QVERIFY(GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor));
        RECT client {};
        POINT origin {0, 0};
        QVERIFY(GetClientRect(handle, &client));
        QVERIFY(ClientToScreen(handle, &origin));
        QCOMPARE(origin.x, monitor.rcWork.left);
        QCOMPARE(origin.y, monitor.rcWork.top);
        QCOMPARE(client.right, monitor.rcWork.right - monitor.rcWork.left);
        QCOMPARE(client.bottom, monitor.rcWork.bottom - monitor.rcWork.top);
        rig.settings.setNativeTitleBar(true);
        QTRY_VERIFY(GetWindowLongPtrW(reinterpret_cast<HWND>(window->winId()), GWL_STYLE) & WS_CAPTION);
        QCOMPARE(window->windowState(), Qt::WindowMaximized);
        verifyTitleBar(window, QStringLiteral("switched-maximized-%1").arg(screenIndex));
        rig.settings.setNativeTitleBar(false);
        QTRY_VERIFY(!(GetWindowLongPtrW(reinterpret_cast<HWND>(window->winId()), GWL_STYLE) & WS_CAPTION));
        QTRY_VERIFY(GetWindowLongPtrW(reinterpret_cast<HWND>(window->winId()), GWL_STYLE) & WS_THICKFRAME);
        QCOMPARE(window->windowState(), Qt::WindowMaximized);
        origin = {0, 0};
        QVERIFY(GetClientRect(reinterpret_cast<HWND>(window->winId()), &client));
        QVERIFY(ClientToScreen(reinterpret_cast<HWND>(window->winId()), &origin));
        QCOMPARE(origin.y, monitor.rcWork.top);
        QCOMPARE(origin.y + client.bottom, monitor.rcWork.bottom);
        QVERIFY(QMetaObject::invokeMethod(maximize, "clicked"));
        QTRY_COMPARE(window->windowState(), Qt::WindowNoState);
        handle = reinterpret_cast<HWND>(window->winId());
        checkBounds();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, minimize->mapToScene(QPointF(minimize->width() / 2, minimize->height() / 2)).toPoint());
        QTRY_COMPARE(window->windowState(), Qt::WindowMinimized);
        window->showNormal();
        QTRY_COMPARE(window->windowState(), Qt::WindowNoState);
        rig.playSomething();
        SendMessageW(handle, WM_COMMAND, MAKEWPARAM(2, THBN_CLICKED), 0);
        QTRY_VERIFY(!rig.player.isPlaying());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, close->mapToScene(QPointF(close->width() / 2, close->height() / 2)).toPoint());
        QTRY_VERIFY(!window->isVisible());
        QVERIFY(!rig.app.isQuitting());
        rig.settings.setNativeTitleBar(true);
        QVERIFY(!window->isVisible());
        rig.settings.setNativeTitleBar(false);
        QVERIFY(!window->isVisible());
        rig.app.handleCommand(QStringLiteral("raise"));
        QTRY_VERIFY(window->isVisible());
        rig.settings.setNativeTitleBar(true);
        QTRY_VERIFY(!window->flags().testFlag(Qt::FramelessWindowHint));
        verifyTitleBar(window, QStringLiteral("restored-native-%1").arg(screenIndex));
    }

    void nativeTitleBar_data()
    {
        QTest::addColumn<int>("screenIndex");
        const auto screens = QGuiApplication::screens();
        for (int index = 0; index < screens.size(); ++index) {
            QTest::newRow(qPrintable(screens[index]->name())) << index;
        }
    }

    void nativeTitleBar()
    {
        if (QGuiApplication::platformName() != QLatin1StringView("windows")) QSKIP("Requires the Windows platform plugin");
        QFETCH(int, screenIndex);
        QScreen *screen = QGuiApplication::screens().at(screenIndex);
        ViewRig rig;
        rig.settings.setNativeTitleBar(true);
        rig.settings.setPerformanceMode(true);
        rig.settings.setThemeMode(QStringLiteral("dark"));
        rig.session.setInitialLibraryLoader({});
        rig.session.restoreSavedSessions();
        QTRY_VERIFY(!rig.session.isEvaluatingStoredLogin());
        WindowsIntegration integration(&rig.player, &rig.theme);
        QQmlComponent component(&rig.engine, QUrl(QStringLiteral("qrc:/qt/qml/Swiftify/qml/Main.qml")));
        const QRect available = screen->availableGeometry();
        const QSize size = QSize(1100, 700).boundedTo(available.size() - QSize(100, 100));
        const QVariantMap initial {
            {QStringLiteral("x"), available.x() + 40}, {QStringLiteral("y"), available.y() + 60},
            {QStringLiteral("width"), size.width()}, {QStringLiteral("height"), size.height()},
            {QStringLiteral("visible"), false}
        };
        std::unique_ptr<QObject> object(component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(window);
        integration.setWindow(window);
        window->show();
        QTRY_VERIFY(window->isExposed());
        QTRY_COMPARE(window->screen(), screen);
        QCOMPARE(window->size(), size);
        QCOMPARE(window->color(), rig.theme.colors()->layer0());
        QVERIFY(!window->flags().testFlag(Qt::ExpandedClientAreaHint));
        QVERIFY(!window->flags().testFlag(Qt::NoTitleBarBackgroundHint));
        const HWND handle = reinterpret_cast<HWND>(window->winId());
        const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);
        QVERIFY(style & WS_CAPTION);
        QVERIFY(style & WS_THICKFRAME);
        QVERIFY(style & WS_MINIMIZEBOX);
        QVERIFY(style & WS_MAXIMIZEBOX);
        window->raise();
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        const QString prefix = QStringLiteral("screen-%1-").arg(screenIndex);
        verifyTitleBar(window, prefix + QStringLiteral("startup"));
        const auto moveToScreen = [&](QScreen *destination) {
            const QPoint offset = destination->availableGeometry().topLeft() - destination->geometry().topLeft() + QPoint(40, 40);
            const QPoint position = destination->geometry().topLeft() + (QPointF(offset) * destination->devicePixelRatio()).toPoint();
            QVERIFY(SetWindowPos(handle, nullptr, position.x(), position.y(), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
            QTRY_COMPARE(window->screen(), destination);
            QTRY_VERIFY(qAbs(window->width() - size.width()) <= 3);
            QTRY_VERIFY(qAbs(window->height() - size.height()) <= 3);
        };
        for (QScreen *destination : QGuiApplication::screens()) {
            if (destination == screen) continue;
            moveToScreen(destination);
            verifyTitleBar(window, prefix + QStringLiteral("moved-%1").arg(destination->name()));
            moveToScreen(screen);
            verifyTitleBar(window, prefix + QStringLiteral("returned-%1").arg(destination->name()));
        }
        SendMessageW(handle, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        QTRY_COMPARE(window->windowState(), Qt::WindowMinimized);
        SendMessageW(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
        QTRY_COMPARE(window->windowState(), Qt::WindowNoState);
        SendMessageW(handle, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        QTRY_COMPARE(window->windowState(), Qt::WindowMaximized);
        verifyTitleBar(window, prefix + QStringLiteral("maximized"));
        MONITORINFO monitor {sizeof(MONITORINFO)};
        QVERIFY(GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor));
        RECT client {};
        POINT origin {0, 0};
        GetClientRect(handle, &client);
        ClientToScreen(handle, &origin);
        QCOMPARE(origin.x, monitor.rcWork.left);
        QVERIFY(origin.y > monitor.rcWork.top);
        QCOMPARE(client.right, monitor.rcWork.right - monitor.rcWork.left);
        QCOMPARE(origin.y + client.bottom, monitor.rcWork.bottom);
        SendMessageW(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
        QTRY_COMPARE(window->windowState(), Qt::WindowNoState);
        {
            QWindow focusWindow;
            focusWindow.resize(100, 80);
            focusWindow.setPosition(window->position() + QPoint(60, 150));
            focusWindow.show();
            focusWindow.requestActivate();
            QTRY_VERIFY(!window->isActive());
            verifyTitleBar(window, prefix + QStringLiteral("inactive"));
        }
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        rig.settings.setThemeMode(QStringLiteral("light"));
        QTRY_VERIFY(!rig.theme.dark());
        QTRY_COMPARE(window->color(), rig.theme.colors()->layer0());
        verifyTitleBar(window, prefix + QStringLiteral("light"));
        rig.playSomething();
        rig.app.handleCommand(QStringLiteral("immersive"));
        QTRY_COMPARE(window->property("immersiveReveal").toReal(), 1.0);
        QTRY_COMPARE(window->color(), rig.style.darkScheme()->layer0());
        verifyTitleBar(window, prefix + QStringLiteral("immersive"));
        rig.app.handleCommand(QStringLiteral("close-immersive"));
        QTRY_COMPARE(window->property("immersiveReveal").toReal(), 0.0);
        QTRY_COMPARE(window->color(), rig.theme.colors()->layer0());
        SendMessageW(handle, WM_SYSCOMMAND, SC_CLOSE, 0);
        QTRY_VERIFY(!window->isVisible());
        QVERIFY(!rig.app.isQuitting());
        QVERIFY(rig.player.isPlaying());
        rig.app.handleCommand(QStringLiteral("raise"));
        QTRY_VERIFY(window->isVisible());
        verifyTitleBar(window, prefix + QStringLiteral("reopened"));
    }

    void taskbarPlayPauseControlsPlayback()
    {
        if (QGuiApplication::platformName() != QLatin1StringView("windows")) QSKIP("Requires the Windows platform plugin");
        ViewRig rig;
        WindowsIntegration integration(&rig.player, &rig.theme);
        QWindow window;
        integration.setWindow(&window);
        window.show();
        rig.playSomething();
        const Song first = rig.player.currentSong();
        QVERIFY(rig.player.isPlaying());
        const HWND handle = reinterpret_cast<HWND>(window.winId());
        QVERIFY(IsWindow(handle));
        SendMessageW(handle, WM_COMMAND, MAKEWPARAM(2, THBN_CLICKED), 0);
        QTRY_VERIFY(!rig.player.isPlaying());
        SendMessageW(handle, WM_COMMAND, MAKEWPARAM(2, THBN_CLICKED), 0);
        QTRY_VERIFY(rig.player.isPlaying());
        SendMessageW(handle, WM_COMMAND, MAKEWPARAM(3, THBN_CLICKED), 0);
        QTRY_VERIFY(rig.player.currentSong().uri != first.uri);
        SendMessageW(handle, WM_COMMAND, MAKEWPARAM(1, THBN_CLICKED), 0);
        QTRY_COMPARE(rig.player.currentSong().uri, first.uri);
    }

    void mediaTransportControlsControlPlayback()
    {
        if (QGuiApplication::platformName() != QLatin1StringView("windows")) QSKIP("Requires the Windows platform plugin");
        ViewRig rig;
        WindowsIntegration integration(&rig.player, &rig.theme);
        QWindow window;
        integration.setWindow(&window);
        window.show();
        rig.playSomething();
        const Song first = rig.player.currentSong();
        QVERIFY(rig.player.isPlaying());
        integration.handleMediaButton(1); // Pause
        QTRY_VERIFY(!rig.player.isPlaying());
        integration.handleMediaButton(0); // Play
        QTRY_VERIFY(rig.player.isPlaying());
        integration.handleMediaButton(6); // Next
        QTRY_VERIFY(rig.player.currentSong().uri != first.uri);
        integration.handleMediaButton(7); // Previous
        QTRY_COMPARE(rig.player.currentSong().uri, first.uri);
    }

    void explicitQuitAllowsTheWindowToClose()
    {
        ViewRig rig;
        rig.settings.setTrayIcon(false);
        QQmlComponent component(&rig.engine, QUrl(QStringLiteral("qrc:/qt/qml/Swiftify/qml/Main.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(window);
        rig.app.quit();
        QVERIFY(rig.app.isQuitting());
        QVERIFY(window->close());
        QVERIFY(!window->isVisible());
    }
};

QTEST_MAIN(WindowsTest)
#include "tst_windows.moc"
