#include "spotify/Discovery.h"
#include "spotify/SpotifyError.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace swiftify;

class DiscoveryTest : public QObject {
    Q_OBJECT
private:
    static QJsonObject playlist(const QString &id, const QString &name)
    {
        return {{QStringLiteral("id"), id}, {QStringLiteral("name"), name},
            {QStringLiteral("owner"), QJsonObject {{QStringLiteral("id"), QStringLiteral("owner")}}},
            {QStringLiteral("collaborative"), true}};
    }

private slots:
    void keepsPersonalizedPlaylistsWithoutDuplicatesOrEditRights()
    {
        const auto first = playlist(QStringLiteral("37i9dQZF1DXcBWIGoYBM5M"), QStringLiteral("Синтез 🎧"));
        const auto second = playlist(QStringLiteral("37i9dQZF1DX4JAvHpjipBk"), QStringLiteral("Daily Mix"));
        const auto unavailable = playlist(QStringLiteral("bad/../../uri"), QStringLiteral("Unavailable"));
        QJsonObject payload {{QStringLiteral("userId"), QStringLiteral("user")},
            {QStringLiteral("personalizedAvailable"), true}, {QStringLiteral("libraryAvailable"), true},
            {QStringLiteral("library"), QJsonArray {first, first}},
            {QStringLiteral("sections"), QJsonArray {
                QJsonObject {{QStringLiteral("title"), QStringLiteral("Для тебя")}, {QStringLiteral("items"), QJsonArray {first, first, unavailable}}},
                QJsonObject {{QStringLiteral("title"), QStringLiteral("Mixes")}, {QStringLiteral("items"), QJsonArray {first, second}}}}}};
        const auto snapshot = discovery::parse(QString::fromUtf8(QJsonDocument(payload).toJson()));
        QCOMPARE(snapshot.saved.size(), 1);
        QCOMPARE(snapshot.personalized.size(), 2);
        QCOMPARE(snapshot.sections.size(), 2);
        QCOMPARE(snapshot.personalized.first().name, QStringLiteral("Синтез 🎧"));
        QVERIFY(snapshot.personalized.first().ownerId.isEmpty());
        QVERIFY(!snapshot.personalized.first().isCollaborative);
        QVERIFY(!snapshot.personalized.first().canModify(QStringLiteral("owner")));
        QCOMPARE(snapshot.personalized.first().uri, QStringLiteral("spotify:playlist:37i9dQZF1DXcBWIGoYBM5M"));
        // Saved rootlist permissions are separate from read-only recommendations.
        QCOMPARE(snapshot.saved.first().ownerId, QStringLiteral("owner"));
    }

    void rejectsInvalidSnapshotsRatherThanClearingExistingCollections()
    {
        QVERIFY_THROWS_EXCEPTION(SpotifyError, discovery::parse(QStringLiteral("not JSON")));
        QVERIFY_THROWS_EXCEPTION(SpotifyError, discovery::parse(QStringLiteral("{}")));
        QVERIFY_THROWS_EXCEPTION(SpotifyError, discovery::parse(QStringLiteral("[]")));
    }
};

QTEST_GUILESS_MAIN(DiscoveryTest)
#include "tst_discovery.moc"
