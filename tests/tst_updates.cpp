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
    void oldRepositoryNameMigratesWithoutChangingOtherForks()
    {
        QCOMPARE(Updates::canonicalRepository(QStringLiteral("ceoweb20/swiftify")), QStringLiteral("ceoweb20/swiftifyplusplus"));
        QCOMPARE(Updates::canonicalRepository(QStringLiteral("CEOWEB20/Swiftify")), QStringLiteral("ceoweb20/swiftifyplusplus"));
        QCOMPARE(Updates::canonicalRepository(QStringLiteral("someone-else/swiftify")), QStringLiteral("someone-else/swiftify"));
    }

    void onlyThePinnedNumericRepositoryApiIsTrusted()
    {
        const QString id = Updates::repositoryId();
        if (id.isEmpty()) QSKIP("This build uses name-based repository lookup.");
        const QUrl api(QStringLiteral("https://api.github.com/repositories/%1/releases/latest").arg(id));
        QCOMPARE(Updates::latestReleaseApi(), api);
        QVERIFY(Updates::trustedDownload(api));
        QVERIFY(!Updates::trustedDownload(QUrl(QStringLiteral("https://api.github.com/repositories/%1/releases/latest").arg(id + u'1'))));
        QVERIFY(!Updates::trustedDownload(QUrl(QStringLiteral("https://api.github.com/repositories/%1/issues").arg(id))));
    }

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
            QStringLiteral("https://github.com/%1/releases/download/v1.0.0/../test.zip").arg(repository),
            QStringLiteral("https://github.com/%1/releases/download/v1.0.0/%2e%2e/test.zip").arg(repository),
            QStringLiteral("https://github.com/%1/releases/download/v1.0.0/test.zip?download=1").arg(repository),
            QStringLiteral("https://github.com/%1/releases/download/v1.0.0/test.zip#fragment").arg(repository),
            QStringLiteral("file:///tmp/package.zip")};
        for (const QString &url : rejected)
            QVERIFY2(!Updates::trustedDownload(QUrl(url)), qPrintable(url));
    }

    void assetsMustMatchTheExactRepositoryTagAndName()
    {
        const QString name = QStringLiteral("swiftify-update-macos-aarch64.json");
        const QString url = QStringLiteral("https://github.com/") + Updates::repository() + QStringLiteral("/releases/download/v1.2.3/") + name;
        const QJsonObject asset {{QStringLiteral("name"), name}, {QStringLiteral("browser_download_url"), url}};
        QJsonObject release {{QStringLiteral("assets"), QJsonArray {asset}},
                             {QStringLiteral("html_url"), QStringLiteral("https://github.com/%1/releases/tag/v1.2.3").arg(Updates::repository())}};
        QCOMPARE(Updates::releaseAsset(release, name, QStringLiteral("1.2.3")), QUrl(url));
        QVERIFY(Updates::releaseAsset(release, name, QStringLiteral("1.2.4")).isEmpty());
        QVERIFY(Updates::releaseAsset(release, QStringLiteral("other.json"), QStringLiteral("1.2.3")).isEmpty());
        release.insert(QStringLiteral("assets"), QJsonArray {asset, asset});
        QVERIFY(Updates::releaseAsset(release, name, QStringLiteral("1.2.3")).isEmpty());
    }

    void renamedRepositoryAssetsStayBoundToPinnedApiMetadata()
    {
        if (Updates::repositoryId().isEmpty()) QSKIP("Renames require a pinned repository ID.");
        const QString renamed = QStringLiteral("test-owner/renamed-repository");
        const QString filename = QStringLiteral("swiftify-update-macos-aarch64.json");
        const QString assetUrl = QStringLiteral("https://github.com/%1/releases/download/v1.2.3/%2").arg(renamed, filename);
        QJsonObject release{{QStringLiteral("html_url"), QStringLiteral("https://github.com/%1/releases/tag/v1.2.3").arg(renamed)},
                            {QStringLiteral("assets"), QJsonArray{QJsonObject{{QStringLiteral("name"), filename},
                                                                           {QStringLiteral("browser_download_url"), assetUrl}}}}};
        QCOMPARE(Updates::releaseRepository(release, QStringLiteral("1.2.3")), renamed);
        QCOMPARE(Updates::releaseAsset(release, filename, QStringLiteral("1.2.3")), QUrl(assetUrl));
        QVERIFY(!Updates::trustedDownload(QUrl(assetUrl)));
        QVERIFY(Updates::trustedDownload(QUrl(assetUrl), renamed));
        QVERIFY(!Updates::trustedDownload(QUrl(assetUrl), QStringLiteral("another-owner/another-repo")));
        release.insert(QStringLiteral("assets"), QJsonArray{QJsonObject{{QStringLiteral("name"), filename},
            {QStringLiteral("browser_download_url"), QStringLiteral("https://github.com/other/repo/releases/download/v1.2.3/") + filename}}});
        QVERIFY(Updates::releaseAsset(release, filename, QStringLiteral("1.2.3")).isEmpty());
        release.insert(QStringLiteral("html_url"), QStringLiteral("https://github.com.evil.test/test/repo/releases/tag/v1.2.3"));
        QVERIFY(Updates::releaseRepository(release, QStringLiteral("1.2.3")).isEmpty());
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

    void invalidReleasePagesCannotAuthorizeAssets()
    {
        const QString prefix = QStringLiteral("https://github.com/") + Updates::repository();
        const QStringList invalid{
            prefix + QStringLiteral("/releases/tag/v1.2.4"),
            prefix + QStringLiteral("/releases/tag/v1.2.3?other=repo"),
            prefix + QStringLiteral("/releases/tag/v1.2.3#fragment"),
            prefix + QStringLiteral("/releases/tag/v1.2.3/extra"),
            QStringLiteral("http://github.com/") + Updates::repository() + QStringLiteral("/releases/tag/v1.2.3"),
            QStringLiteral("https://user@github.com/") + Updates::repository() + QStringLiteral("/releases/tag/v1.2.3"),
            QStringLiteral("https://github.com:444/") + Updates::repository() + QStringLiteral("/releases/tag/v1.2.3")};
        for (const auto &page : invalid) {
            const QJsonObject release{{QStringLiteral("html_url"), page}};
            QVERIFY2(Updates::releaseRepository(release, QStringLiteral("1.2.3")).isEmpty(), qPrintable(page));
        }
    }
};

QTEST_GUILESS_MAIN(UpdatesTest)
#include "tst_updates.moc"
