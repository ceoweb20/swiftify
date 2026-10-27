#include "playback/DjMix.h"

#include <QTest>

using namespace swiftify;

namespace {
Song track(int id, const QString &artist)
{
    Song song;
    song.uri = QStringLiteral("spotify:track:%1").arg(id);
    song.name = QStringLiteral("Track %1").arg(id);
    song.artists = artist;
    return song;
}
}

class DjMixTest : public QObject {
    Q_OBJECT
private slots:
    void excludesDuplicatesUnplayableAndCurrentTracks()
    {
        Song unavailable = track(3, QStringLiteral("C"));
        unavailable.isPlayable = false;
        Song invalid;
        invalid.uri = QStringLiteral("spotify:episode:test");
        const auto first = track(1, QStringLiteral("A"));
        const auto second = track(2, QStringLiteral("B"));
        QRandomGenerator random(42);
        const auto result = dj::queue({first, second, unavailable, invalid}, {first, second}, 40, {first.uri}, random);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().uri, second.uri);
        QVERIFY(result.first().instanceId != second.instanceId);
    }

    void spacesArtistsWhenThereAreAlternatives()
    {
        QList<Song> recommended, familiar;
        for (int i = 0; i < 5; ++i) {
            recommended.append(track(i, QStringLiteral("A")));
            familiar.append(track(i + 5, QStringLiteral("B")));
        }
        QRandomGenerator random(42);
        const auto result = dj::queue(recommended, familiar, 10, {}, random);
        QCOMPARE(result.size(), 10);
        for (int i = 1; i < result.size(); ++i) QVERIFY(result.at(i).artists != result.at(i - 1).artists);
    }

    void respectsLimitsAndDoesNotMutateItsSources()
    {
        QList<Song> tracks;
        for (int i = 0; i < 150; ++i) tracks.append(track(i, QString::number(i % 7)));
        QRandomGenerator first(42), second(42);
        const auto a = dj::queue(tracks, {}, 40, {}, first);
        const auto b = dj::queue(tracks, {}, 40, {}, second);
        QCOMPARE(a.size(), 40);
        QCOMPARE(tracks.size(), 150);
        QSet<QString> unique;
        for (int i = 0; i < a.size(); ++i) {
            QCOMPARE(a.at(i).uri, b.at(i).uri);
            unique.insert(a.at(i).uri);
        }
        QCOMPARE(unique.size(), 40);
        QCOMPARE(dj::queue(tracks, {}, 1000, {}, first).size(), 100);
        QVERIFY(dj::queue(tracks, {}, 0, {}, first).isEmpty());
        QVERIFY(dj::queue({}, {}, 40, {}, first).isEmpty());
    }
};

QTEST_GUILESS_MAIN(DjMixTest)
#include "tst_dj_mix.moc"
