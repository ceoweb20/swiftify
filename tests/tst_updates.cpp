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
        QVERIFY(Updates::trustedDownload(QUrl(QStringLiteral("https://api.github.com/repos/") + Updates::repository() + QStringLiteral("/releases/latest"))));
        QVERIFY(Updates::trustedDownload(QUrl(QStringLiteral("https://github.com/") + Updates::repository() + QStringLiteral("/releases/download/v1.0.0/test.zip"))));
        QVERIFY(Updates::trustedDownload(QUrl(QStringLiteral("https://release-assets.githubusercontent.com/test?token=opaque"))));
        const QString repository = Updates::repository();
        const QString foreign = QStringLiteral("someone-else/") + repository.section(QLatin1Char('/'), 1);
        const QStringList rejected {
            QStringLiteral("http://github.com/%1/releases/download/test").arg(repository),
            QStringLiteral("https://github.com/%1/releases/download/test").arg(foreign),
            QStringLiteral("https://github.com.evil.test/%1/releases/download/test").arg(repository),
            QStringLiteral("https://user@github.com/%1/releases/download/test").arg(repository),
            QStringLiteral("https://github.com:444/%1/releases/download/test").arg(repository),
            QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(foreign),
            QStringLiteral("file:///tmp/package.zip")};
        for (const QString &url : rejected)
            QVERIFY2(!Updates::trustedDownload(QUrl(url)), qPrintable(url));
    }

    void assetsMustMatchTheExactRepositoryTagAndName()
    {
        const QString name = QStringLiteral("swiftify-update-macos-aarch64.json");
        const QString url = QStringLiteral("https://github.com/") + Updates::repository() + QStringLiteral("/releases/download/v1.2.3/") + name;
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
        // both are on until turned off, but nothing happens without a portable package
        QVERIFY(updates.automatic());
        QVERIFY(updates.installAutomatically());
        QVERIFY(!updates.supported()); // Test executable has no official portable marker.
        QSignalSpy restart(&updates, &Updates::restartRequested);
        updates.check(); updates.install();
        QVERIFY(!updates.busy()); QVERIFY(!updates.available()); QCOMPARE(restart.count(), 0);
        updates.setAutomatic(false);
        QVERIFY(!updates.automatic());
        QVERIFY(!settings.value(QStringLiteral("updates/automatic"), true).toBool());
        updates.setAutomatic(true);
        QVERIFY(updates.automatic());
        updates.setInstallAutomatically(false);
        QVERIFY(!updates.installAutomatically());
        QVERIFY(!settings.value(QStringLiteral("updates/install"), true).toBool());
        updates.setInstallAutomatically(true);
        QVERIFY(updates.installAutomatically());
    }
};

QTEST_GUILESS_MAIN(UpdatesTest)
#include "tst_updates.moc"
