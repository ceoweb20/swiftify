#include "spotify/SpotifyLink.h"

#include <QTest>

using namespace swiftify;
using Kind = SpotifyLink::Kind;

Q_DECLARE_METATYPE(swiftify::SpotifyLink::Kind)

namespace {
const QString kId = QStringLiteral("4uLU6hMCjMI75M1A2tKUQC");
}

class SpotifyLinkTest : public QObject {
    Q_OBJECT

private:
    static void addLinkKinds()
    {
        QTest::addColumn<QString>("kindName");
        QTest::addColumn<Kind>("kind");

        QTest::newRow("track") << "track" << Kind::Track;
        QTest::newRow("album") << "album" << Kind::Album;
        QTest::newRow("playlist") << "playlist" << Kind::Playlist;
        QTest::newRow("artist") << "artist" << Kind::Artist;
    }

private slots:
    void aSpotifyUrlNamesWhatItOpens_data() { addLinkKinds(); }

    void aSpotifyUrlNamesWhatItOpens()
    {
        // Given
        QFETCH(QString, kindName);
        QFETCH(Kind, kind);
        const QString input = QStringLiteral("https://open.spotify.com/%1/%2").arg(kindName, kId);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(link);
        QCOMPARE(link->kind, kind);
        QCOMPARE(link->id, kId);
    }

    void aSpotifyUriNamesWhatItOpens_data() { addLinkKinds(); }

    void aSpotifyUriNamesWhatItOpens()
    {
        // Given
        QFETCH(QString, kindName);
        QFETCH(Kind, kind);
        const QString input = QStringLiteral("spotify:%1:%2").arg(kindName, kId);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(link);
        QCOMPARE(link->kind, kind);
        QCOMPARE(link->id, kId);
    }

    void aLocaleSegmentIsIgnored()
    {
        // Given
        const QString input = QStringLiteral("https://open.spotify.com/intl-de/album/%1").arg(kId);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(link);
        QCOMPARE(link->kind, Kind::Album);
        QCOMPARE(link->id, kId);
    }

    void aQueryStringIsIgnored()
    {
        // Given
        const QString input = QStringLiteral("https://open.spotify.com/track/%1?si=abc123").arg(kId);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(link);
        QCOMPARE(link->kind, Kind::Track);
        QCOMPARE(link->id, kId);
    }

    void aTrailingSlashAndSurroundingSpacesAreIgnored()
    {
        // Given
        const QString input = QStringLiteral(" https://open.spotify.com/artist/%1/ ").arg(kId);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(link);
        QCOMPARE(link->kind, Kind::Artist);
        QCOMPARE(link->id, kId);
    }

    void httpIsAcceptedLikeHttps()
    {
        // Given
        const QString input = QStringLiteral("http://open.spotify.com/track/%1").arg(kId);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(link);
        QCOMPARE(link->kind, Kind::Track);
        QCOMPARE(link->id, kId);
    }

    void somethingElseIsNotALink_data()
    {
        QTest::addColumn<QString>("input");

        QTest::newRow("empty") << QString();
        QTest::newRow("plain text") << QStringLiteral("radiohead");
        QTest::newRow("other host") << QStringLiteral("https://example.com/track/%1").arg(kId);
        QTest::newRow("host with spotify as prefix") << QStringLiteral("https://open.spotify.com.evil.test/track/%1").arg(kId);
        QTest::newRow("host with spotify as suffix") << QStringLiteral("https://evilopen.spotify.com/track/%1").arg(kId);
        QTest::newRow("user") << QStringLiteral("https://open.spotify.com/user/%1").arg(kId);
        QTest::newRow("show") << QStringLiteral("https://open.spotify.com/show/%1").arg(kId);
        QTest::newRow("episode") << QStringLiteral("https://open.spotify.com/episode/%1").arg(kId);
        QTest::newRow("one segment") << QStringLiteral("https://open.spotify.com/track");
        QTest::newRow("three segments") << QStringLiteral("https://open.spotify.com/track/%1/extra").arg(kId);
        QTest::newRow("uri without id") << QStringLiteral("spotify:track");
        QTest::newRow("uri with too many parts") << QStringLiteral("spotify:track:a:b");
        QTest::newRow("uri of user") << QStringLiteral("spotify:user:%1").arg(kId);
    }

    void somethingElseIsNotALink()
    {
        // Given
        QFETCH(QString, input);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(!link);
    }

    void anIdOfTheWrongShapeIsRejected_data()
    {
        QTest::addColumn<QString>("input");

        QTest::newRow("too short") << QStringLiteral("https://open.spotify.com/track/4uLU6hMCjMI75M1A2tKUQ");
        QTest::newRow("with dash") << QStringLiteral("https://open.spotify.com/track/4uLU6hMCjMI75M1A2tKU-C");
        QTest::newRow("with underscore") << QStringLiteral("https://open.spotify.com/track/4uLU6hMCjMI75M1A2tKU_C");
        QTest::newRow("empty") << QStringLiteral("https://open.spotify.com/track/");
    }

    void anIdOfTheWrongShapeIsRejected()
    {
        // Given
        QFETCH(QString, input);

        // When
        const auto link = parseSpotifyLink(input);

        // Then
        QVERIFY(!link);
    }
};

QTEST_GUILESS_MAIN(SpotifyLinkTest)
#include "tst_spotifylink.moc"
