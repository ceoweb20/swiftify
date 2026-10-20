#include "FakeCore.h"

#include "app/ErrorCenter.h"
#include "app/Settings.h"
#include "motion/AmbientClock.h"
#include "motion/FrameTicker.h"
#include "playback/ImmersiveLayout.h"
#include "playback/PlaybackController.h"
#include "render/BackdropClock.h"
#include "render/SpectrumItem.h"
#include "render/SpectrumShaping.h"
#include "render/SpectrumSource.h"
#include "spotify/CredentialStore.h"
#include "spotify/SpotifySession.h"

#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace swiftify;

/// SpectrumRenderingTests.swift: peaks survive reduction, the layout budget
/// holds at every window size, bars ease and rest, and paused bars stop.
class SpectrumTest : public QObject {
    Q_OBJECT

private slots:
    void aNarrowPeakSurvivesReduction()
    {
        for (int count : {24, 32}) {
            for (int source = 0; source < 48; ++source) {
                QList<float> levels(48, 0.0f);
                levels[source] = 1;
                const QList<float> mapped = spectrum::bassExpanded(levels, count);
                QCOMPARE(mapped.size(), count);
                QVERIFY2(*std::max_element(mapped.begin(), mapped.end()) >= 0.5f,
                         qPrintable(QStringLiteral("band %1 vanished at %2 bars").arg(source).arg(count)));
            }
        }
    }

    void theCompactMeterMixesRmsAndPeak()
    {
        QList<float> levels(48, 0.0f);
        for (int band = 0; band < 8; ++band) {
            levels[band] = band == 0 ? 1.0f : 0.0f;
        }
        const QList<float> meter = spectrum::compact(levels, 6);
        QCOMPARE(meter.size(), 6);
        const float rms = std::sqrt(1.0f / 8.0f);
        QVERIFY(std::abs(meter[0] - (rms * 0.72f + 0.28f)) < 1e-5f);
        QCOMPARE(meter[1], 0.0f);
    }

    void theLoadingPulseSweepsAcross()
    {
        const QList<float> start = spectrum::loadingPulse(48, 0.1f);
        const QList<float> end = spectrum::loadingPulse(48, 0.9f);
        QVERIFY(start.first() > start.last());
        QVERIFY(end.last() > end.first());
        QVERIFY(*std::min_element(start.begin(), start.end()) >= 0.06f - 1e-6f);

        // and then waits, every bar at its floor, before the next pass
        const QList<float> resting = spectrum::loadingPulse(48, 0.9f, 0.22f);
        QVERIFY(*std::max_element(resting.begin(), resting.end()) < 0.07f);
        const QList<float> sweeping = spectrum::loadingPulse(48, 0.4f, 0.22f);
        QVERIFY(*std::max_element(sweeping.begin(), sweeping.end()) > 0.8f);
    }

    void aRingPulsesDownBothSidesThenWaits()
    {
        constexpr int bars = 72;
        // early on: near the top on both sides, mirror images of each other
        const QList<float> early = spectrum::ringLoadingPulse(bars, 0.1f, 0.45f);
        QCOMPARE(early.size(), bars);
        for (int index = 0; index < bars / 2; ++index) {
            QVERIFY(std::abs(early[index] - early[bars - 1 - index]) < 1e-6f);
        }
        const auto peak = [](const QList<float> &levels, int from, int to) {
            return int(std::max_element(levels.begin() + from, levels.begin() + to) - levels.begin());
        };
        const int earlyPeak = peak(early, 0, bars / 2);
        // later: further down, towards the bottom
        const QList<float> later = spectrum::ringLoadingPulse(bars, 0.4f, 0.45f);
        QVERIFY(peak(later, 0, bars / 2) > earlyPeak);
        QVERIFY(*std::max_element(later.begin(), later.end()) > 0.8f);
        // and the rest: nothing moves
        const QList<float> resting = spectrum::ringLoadingPulse(bars, 0.8f, 0.45f);
        QVERIFY(*std::max_element(resting.begin(), resting.end()) < 0.07f);
    }

    void theLayoutBudgetHoldsAtEverySize()
    {
        for (qreal height : {560.0, 600.0, 680.0, 720.0, 900.0, 1440.0}) {
            for (qreal width : {1000.0, 1280.0, 1920.0}) {
                for (bool spectrumShown : {false, true}) {
                    ImmersiveLayout normal;
                    normal.set(width, height, spectrumShown, false, false);
                    ImmersiveLayout panel;
                    panel.set(width, height, spectrumShown, true, false);
                    QCOMPARE(normal.contentHeight(), panel.contentHeight());
                    QCOMPARE(normal.spectrumHeight(), panel.spectrumHeight());
                    for (const ImmersiveLayout *layout : {&normal, &panel}) {
                        const qreal used = layout->topBarHeight() + layout->contentHeight() + layout->spectrumSpacing()
                            + layout->spectrumHeight() + layout->bottomPadding();
                        QVERIFY(std::abs(used - height) < 0.01);
                        QVERIFY(layout->artworkSize() + layout->chromeHeight() <= layout->contentHeight());
                        QVERIFY2(layout->artworkSize() >= 80, "artwork stays usable at the minimum window size");
                    }
                }
            }
        }
    }

    void barsEaseUpThenRestWhenPaused()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        ErrorCenter errors {nullptr};
        MemoryCredentialStore credentials;
        QNetworkAccessManager network;
        testing::FakeCore core;
        SpotifySession session(&core, &credentials, &network, &errors);
        PlaybackController player(&core, &session, &settings, &errors);
        SpectrumSource source(&core, &player);

        SpectrumItem item;
        item.setKind(SpectrumItem::Kind::Immersive);
        item.setSize(QSizeF(900, 150));
        item.setActive(true);
        QCOMPARE(item.levels().size(), 48);

        const QList<float> full(48, 1.0f);
        source.publish(full.constData(), 48);
        QVERIFY(item.isMoving());
        for (int frame = 0; frame < 12; ++frame) {
            item.advance(1.0 / 60);
        }
        // full scale everywhere, the bass reaching the top
        QVERIFY(std::abs(item.levels().first() - 1.0f) < 0.01f);

        // mirrored bars match their partners
        QList<float> ramp(48);
        for (int band = 0; band < 48; ++band) {
            ramp[band] = float(band) / 47;
        }
        source.publish(ramp.constData(), 48);
        for (int frame = 0; frame < 40; ++frame) {
            item.advance(1.0 / 60);
        }
        const QList<float> &levels = item.levels();
        for (qsizetype index = 0; index < levels.size(); ++index) {
            QVERIFY(std::abs(levels[index] - levels[levels.size() - 1 - index]) < 0.001f);
        }

        // a continuous band keeps its bars, bass on the left
        item.setContinuous(true);
        QCOMPARE(item.levels().size(), 48);
        QCOMPARE(item.style().layout, SpectrumStyle::Layout::Ascending);
        for (int frame = 0; frame < 40; ++frame) {
            item.advance(1.0 / 60);
        }
        QVERIFY(item.levels().first() < item.levels().last());
        item.setContinuous(false);
        QCOMPARE(item.style().layout, SpectrumStyle::Layout::Mirrored);

        // garbage stays finite
        const float garbage[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1, 2};
        source.publish(garbage, 4);
        item.advance(1.0 / 60);
        for (float level : item.levels()) {
            QVERIFY(std::isfinite(level));
        }

        // paused bars fall, come to rest within two seconds, and stay there
        item.setActive(false);
        QVERIFY2(item.isMoving(), "paused bars fall rather than freeze");
        int frames = 0;
        while (item.isMoving() && frames < 120) {
            item.advance(1.0 / 60);
            ++frames;
        }
        QVERIFY2(!item.isMoving(), "paused bars come to rest within two seconds");
        source.publish(full.constData(), 48);
        QVERIFY(!item.isMoving());
        item.setActive(true);
        QVERIFY(item.isMoving());
    }

    void theBackdropFlowsOnlyWhilePlayingAndHoldsOnPause()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        ErrorCenter errors {nullptr};
        MemoryCredentialStore credentials;
        QNetworkAccessManager network;
        testing::FakeCore core;
        SpotifySession session(&core, &credentials, &network, &errors);
        PlaybackController player(&core, &session, &settings, &errors);
        SpectrumSource source(&core, &player);
        const QList<float> full(48, 0.5f);
        source.publish(full.constData(), 48);

        BackdropClock clock;
        clock.setReactivity(2);
        clock.setPlaying(true);
        QCOMPARE(clock.time(), 0.0);

        // shown and playing: time runs and the bands arrive scaled
        clock.setRunning(true);
        QTRY_VERIFY(clock.time() > 0.05);
        QVERIFY(std::abs(clock.energy().x() - 1.0f) < 0.001f);
        QVERIFY(std::abs(clock.lead() - 1.0) < 0.001);

        // paused: the field holds where it was, with no energy
        clock.setPlaying(false);
        const qreal held = clock.time();
        QCOMPARE(clock.energy(), QVector4D());
        QTest::qWait(80);
        QCOMPARE(clock.time(), held);

        // playing again resumes from the held moment rather than jumping on
        clock.setPlaying(true);
        QVERIFY(clock.time() >= held);
        QVERIFY(clock.time() < held + 0.05);
    }

    // docs/perf.md: what turns slowly while music plays moves 30 times a
    // second, not with every frame, and stops dead with the music
    void theAmbientClockTicksAtThirtyHertz()
    {
        AmbientClock clock;
        QSignalSpy ticks(&clock, &AmbientClock::valueChanged);
        QVERIFY(!FrameTicker::instance()->isRunning());

        clock.setSpeed(2);
        QVERIFY(FrameTicker::instance()->isRunning());
        // easing a speed changes it every frame; the value still waits for
        // the tick
        clock.setSpeed(2.5);
        clock.setSpeed(3);
        QCOMPARE(ticks.count(), 0);

        QTest::qWait(500);
        QVERIFY2(ticks.count() >= 10 && ticks.count() <= 18, qPrintable(QString::number(ticks.count())));
        // three units a second for half a second, give or take a tick
        QVERIFY2(clock.value() > 1.2 && clock.value() < 1.7, qPrintable(QString::number(clock.value())));

        clock.setSpeed(0);
        QVERIFY(!FrameTicker::instance()->isRunning());
        const qreal held = clock.value();
        ticks.clear();
        QTest::qWait(100);
        QCOMPARE(ticks.count(), 0);
        QCOMPARE(clock.value(), held);
    }
};

QTEST_MAIN(SpectrumTest)
#include "tst_spectrum.moc"
