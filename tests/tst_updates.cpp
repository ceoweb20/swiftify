#include "system/Updates.h"
#include "app/Settings.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace swiftify;

class UpdatesTest : public QObject {
    Q_OBJECT
private slots:
    void onlyNewStableVersionsAreOffered()
    {
        QVERIFY(Updates::newerStable(QStringLiteral("0.14.0"), QStringLiteral("0.13.9")));
        QVERIFY(Updates::newerStable(QStringLiteral("1.10.0"), QStringLiteral("1.9.9")));
        for (const auto &candidate : {"0.13.0", "0.12.9", "0.14.0-beta", "v0.14.0", "0.14", "00.14.0", "garbage"})
            QVERIFY(!Updates::newerStable(QString::fromLatin1(candidate), QStringLiteral("0.13.0")));
    }

    void downloadsNeverGoToOtherRepositoriesOrPlaintext()
    {
        QVERIFY(Updates::trustedDownload(QUrl(QStringLiteral("https://api.github.com/repos/shredos67/swiftify/releases/latest"))));
        QVERIFY(Updates::trustedDownload(QUrl(QStringLiteral("https://github.com/shredos67/swiftify/releases/download/v1.0.0/test.zip"))));
        QVERIFY(Updates::trustedDownload(QUrl(QStringLiteral("https://release-assets.githubusercontent.com/test?token=opaque"))));
        for (const auto &url : {"http://github.com/shredos67/swiftify/releases/download/test", "https://github.com/other/swiftify/releases/download/test",
                               "https://github.com.evil.test/shredos67/swiftify/releases/download/test", "https://user@github.com/shredos67/swiftify/releases/download/test",
                               "https://github.com:444/shredos67/swiftify/releases/download/test", "https://api.github.com/repos/other/swiftify/releases/latest",
                               "file:///tmp/package.zip"})
            QVERIFY(!Updates::trustedDownload(QUrl(QString::fromLatin1(url))));
    }

    void assetsMustMatchTheExactRepositoryTagAndName()
    {
        const QString name = QStringLiteral("swiftify-update-macos-aarch64.json");
        const QString url = QStringLiteral("https://github.com/shredos67/swiftify/releases/download/v1.2.3/") + name;
        const QJsonObject asset {{QStringLiteral("name"), name}, {QStringLiteral("browser_download_url"), url}};
        QJsonObject release {{QStringLiteral("assets"), QJsonArray {asset}}};
        QCOMPARE(Updates::releaseAsset(release, name, QStringLiteral("1.2.3")), QUrl(url));
        QVERIFY(Updates::releaseAsset(release, name, QStringLiteral("1.2.4")).isEmpty());
        QVERIFY(Updates::releaseAsset(release, QStringLiteral("other.json"), QStringLiteral("1.2.3")).isEmpty());
        release.insert(QStringLiteral("assets"), QJsonArray {asset, asset});
        QVERIFY(Updates::releaseAsset(release, name, QStringLiteral("1.2.3")).isEmpty());
    }

    void checksAreOptInAndUnsupportedBuildsCannotInstall()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("test.conf")));
        Updates updates(&settings);
        QVERIFY(!updates.automatic());
        QVERIFY(!updates.supported()); // Test executable has no official portable marker.
        QSignalSpy restart(&updates, &Updates::restartRequested);
        updates.check(); updates.install();
        QVERIFY(!updates.busy()); QVERIFY(!updates.available()); QCOMPARE(restart.count(), 0);
        updates.setAutomatic(true);
        QVERIFY(updates.automatic());
        QVERIFY(settings.value(QStringLiteral("updates/automatic")).toBool());
        updates.setAutomatic(false);
        QVERIFY(!updates.automatic());
    }
};

QTEST_GUILESS_MAIN(UpdatesTest)
#include "tst_updates.moc"
