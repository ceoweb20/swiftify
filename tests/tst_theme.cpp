#include "app/Settings.h"
#include "theme/MaterialScheme.h"
#include "theme/Theme.h"
#include "theme/Themes.h"

#include <QFile>
#include <QDir>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace swiftify;

namespace {

QByteArray matugenJson(const MaterialScheme::Roles &roles)
{
    QJsonObject object;
    for (std::size_t index = 0; index < roles.size(); ++index) {
        QString snake;
        for (const QChar character : MaterialScheme::roleKey(MaterialScheme::Role(index))) {
            if (character.isUpper()) {
                snake += u'_';
            }
            snake += character.toLower();
        }
        object.insert(snake, roles[index].name());
    }
    return QJsonDocument(object).toJson();
}

// Discovery tests own their XDG folders and restore the process environment.
struct MatugenPaths {
    QTemporaryDir directory;
    QList<std::pair<QByteArray, std::optional<QByteArray>>> previous;
    MatugenPaths()
    {
        for (const auto &[name, subdir] : {std::pair{"XDG_CONFIG_HOME", "config"},
                                         std::pair{"XDG_CACHE_HOME", "cache"},
                                         std::pair{"XDG_STATE_HOME", "state"}}) {
            previous.append({name, qEnvironmentVariableIsSet(name) ? std::optional(qgetenv(name)) : std::nullopt});
            const QString path = directory.filePath(QLatin1StringView(subdir));
            QDir().mkpath(path);
            qputenv(name, path.toUtf8());
        }
    }
    ~MatugenPaths()
    {
        for (const auto &[name, value] : previous) {
            if (value) qputenv(name.constData(), *value);
            else qunsetenv(name.constData());
        }
    }
};

bool writeJson(const QString &path, const QByteArray &json)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(json) == json.size() && file.commit();
}

QString formattedColor(const QColor &color, const QString &format)
{
    if (format == QLatin1StringView("hex")) return color.name();
    if (format == QLatin1StringView("strip")) return color.name().mid(1);
    if (format == QLatin1StringView("rgb")) return QStringLiteral("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
    if (format == QLatin1StringView("rgba")) return QStringLiteral("rgba(%1, %2, %3, 1)").arg(color.red()).arg(color.green()).arg(color.blue());
    return QStringLiteral("%1(%2, %3%, %4%%5)")
        .arg(format).arg(std::max(0.0f, color.hslHueF()) * 360, 0, 'f', 8)
        .arg(color.hslSaturationF() * 100, 0, 'f', 8).arg(color.lightnessF() * 100, 0, 'f', 8)
        .arg(format == QLatin1StringView("hsla") ? QStringLiteral(", 1") : QString());
}

} // namespace

class ThemeTest : public QObject {
    Q_OBJECT

private slots:
    void artworkThemeRespectsSelectionAndMode()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeMode(QStringLiteral("dark"));
        Theme theme(&settings);
        const auto original = theme.colors()->roles();
        const auto light = Theme::rolesFromSeed(QColor(QStringLiteral("#3185d2")), false, true);
        const auto dark = Theme::rolesFromSeed(QColor(QStringLiteral("#3185d2")), true, true);
        theme.setArtworkSchemes(light, dark);
        QCOMPARE(theme.colors()->roles(), original);
        settings.setThemeSource(QStringLiteral("artwork"));
        QVERIFY(theme.usingArtworkScheme());
        QTRY_COMPARE(theme.colors()->roles(), dark);
        settings.setThemeMode(QStringLiteral("light"));
        QTRY_COMPARE(theme.colors()->roles(), light);
        theme.clearArtworkScheme();
        QVERIFY(!theme.usingArtworkScheme());
        QCOMPARE(theme.colors()->roles(), Theme::rolesFromSeed(QColor::fromRgba(Theme::kAccent), false, true));
    }

    void missingMatugenDoesNotDuplicateTheBuiltinTheme()
    {
        MatugenPaths paths;
        Settings settings(paths.directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setMatugenColorsPath(paths.directory.filePath(QStringLiteral("missing.json")));
        Theme theme(&settings);
        const auto options = theme.themeOptions(true);
        QCOMPARE(std::count_if(options.begin(), options.end(), [](const auto &option) {
            return option.id == QLatin1StringView("builtin");
        }), 1);
        QVERIFY(std::none_of(options.begin(), options.end(), [](const auto &option) {
            return option.id == QLatin1StringView("system");
        }));
        QVERIFY(themes::find(QStringLiteral("aurora")));
        QVERIFY(themes::find(QStringLiteral("desert-bloom")));
    }

    void freshSettingsUseSpotifyColors_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void freshSettingsUseSpotifyColors()
    {
        QFETCH(bool, dark);
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeMode(dark ? QStringLiteral("dark") : QStringLiteral("light"));
        Theme theme(&settings);
        const themes::Family *spotify = themes::find(QStringLiteral("spotify"));
        QVERIFY(spotify);
        QVERIFY(!theme.usingSystemScheme());
        QCOMPARE(theme.dark(), dark);
        QCOMPARE(theme.colors()->roles(), themes::roles(dark ? spotify->dark : spotify->light, dark));
    }

    void seedSchemesFollowMaterialTones()
    {
        const auto dark = Theme::rolesFromSeed(QColor::fromRgba(Theme::kAccent), true, true);
        const auto light = Theme::rolesFromSeed(QColor::fromRgba(Theme::kAccent), false, true);
        const auto role = [](const MaterialScheme::Roles &roles, MaterialScheme::Role which) {
            return roles[static_cast<std::size_t>(which)];
        };
        // dark surfaces are dark and their text light, and the other way round.
        QVERIFY(role(dark, MaterialScheme::Role::Surface).lightnessF() < 0.2);
        QVERIFY(role(dark, MaterialScheme::Role::OnSurface).lightnessF() > 0.8);
        QVERIFY(role(light, MaterialScheme::Role::Surface).lightnessF() > 0.9);
        // a content scheme keeps the seed's hue for primary.
        QVERIFY(qAbs(role(light, MaterialScheme::Role::Primary).hueF()
                     - QColor::fromRgba(Theme::kAccent).hueF()) < 0.05);
    }

    void derivedColorsFollowEnd4()
    {
        MaterialScheme scheme;
        const auto roles = Theme::rolesFromSeed(QColor(Qt::blue), true, false);
        scheme.setRoles(roles, true);
        QCOMPARE(scheme.layer1(), scheme.surfaceContainerLow());
        QCOMPARE(scheme.subtext(), scheme.outline());
        QVERIFY(qAbs(scheme.shadowColor().alphaF() - 0.3) < 0.001);
        // hover sits 8% of the way from the layer towards its text.
        const QColor hover = scheme.layer1Hover();
        const qreal expected = scheme.layer1().redF() * 0.92 + scheme.onLayer1().redF() * 0.08;
        QVERIFY(qAbs(hover.redF() - expected) < 0.01);
    }

    void readsAndFollowsTheMatugenFile()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeSource(QStringLiteral("system"));
        const QString path = directory.filePath(QStringLiteral("colors.json"));
        settings.setMatugenColorsPath(path);

        const auto green = Theme::rolesFromSeed(QColor(Qt::green), false, false);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(matugenJson(green));
        file.close();

        Theme theme(&settings);
        QVERIFY(theme.usingSystemScheme());
        QVERIFY(!theme.dark());
        QCOMPARE(theme.colors()->primary(), green[static_cast<std::size_t>(MaterialScheme::Role::Primary)]);

        QSignalSpy changed(theme.colors(), &MaterialScheme::changed);
        const auto red = Theme::rolesFromSeed(QColor(Qt::red), true, false);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(matugenJson(red));
        file.close();
        QTRY_VERIFY_WITH_TIMEOUT(changed.count() > 0, 3000);
        QVERIFY(theme.dark());
        QCOMPARE(theme.colors()->primary(), red[static_cast<std::size_t>(MaterialScheme::Role::Primary)]);
    }

    void matugenExports_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<QString>("layout");
        QTest::addColumn<bool>("dark");
        for (const char *format : {"hex", "strip", "rgb", "rgba", "hsl", "hsla"}) {
            for (const char *layout : {"flat", "per-role", "schemes"}) {
                for (const bool dark : {false, true}) {
                    QTest::addRow("%s %s %s", format, layout, dark ? "dark" : "light")
                        << QString::fromLatin1(format) << QString::fromLatin1(layout) << dark;
                }
            }
        }
    }

    void matugenExports()
    {
        QFETCH(QString, format);
        QFETCH(QString, layout);
        QFETCH(bool, dark);
        const auto light = Theme::rolesFromSeed(Qt::green, false, false);
        const auto night = Theme::rolesFromSeed(Qt::blue, true, false);
        const auto &expected = dark ? night : light;
        const auto objectFor = [&](const MaterialScheme::Roles &roles) {
            QJsonObject object = QJsonDocument::fromJson(matugenJson(roles)).object();
            for (auto it = object.begin(); it != object.end(); ++it) it.value() = formattedColor(QColor::fromString(it.value().toString()), format);
            return object;
        };
        QJsonObject object;
        if (layout == QLatin1StringView("flat")) {
            object = objectFor(expected);
        } else if (layout == QLatin1StringView("schemes")) {
            object.insert(QStringLiteral("schemes"), QJsonObject{{"dark", objectFor(night)}, {"light", objectFor(light)}});
        } else {
            const auto d = objectFor(night), l = objectFor(light);
            QJsonObject colors;
            for (auto it = d.begin(); it != d.end(); ++it) {
                colors.insert(it.key(), QJsonObject{{"dark", QJsonObject{{"color", it.value()}}},
                                                   {"default", QJsonObject{{"color", it.value()}}},
                                                   {"light", QJsonObject{{"color", l.value(it.key())}}}});
            }
            object.insert(QStringLiteral("colors"), colors);
        }
        const auto parsed = MaterialScheme::fromMatugenJson(QJsonDocument(object).toJson(), dark);
        QVERIFY(parsed);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const QColor actual = (*parsed)[i].toRgb();
            QVERIFY(qAbs(actual.red() - expected[i].red()) <= 1);
            QVERIFY(qAbs(actual.green() - expected[i].green()) <= 1);
            QVERIFY(qAbs(actual.blue() - expected[i].blue()) <= 1);
        }
    }

    void discoversOtherFilenamesAndFollowsAtomicReplacement()
    {
        MatugenPaths paths;
        const auto green = Theme::rolesFromSeed(Qt::green, true, false);
        const auto red = Theme::rolesFromSeed(Qt::red, true, false);
        const QString config = paths.directory.filePath(QStringLiteral("config/matugen"));
        const QString cached = paths.directory.filePath(QStringLiteral("cache/matugen/wallpaper-theme.json"));
        QVERIFY(writeJson(cached, matugenJson(green)));
        QVERIFY(writeJson(config + QStringLiteral("/colors.json"), "{\"primary\": \"#123456\"}"));
        // Even valid-looking input templates must never override generated output.
        QVERIFY(writeJson(config + QStringLiteral("/templates/colors.json"), matugenJson(red)));
        Settings settings(paths.directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeSource(QStringLiteral("system"));
        Theme theme(&settings);
        QVERIFY(theme.usingSystemScheme());
        QCOMPARE(theme.systemSchemePath(), cached);
        QCOMPARE(theme.colors()->primary(), green[std::size_t(MaterialScheme::Role::Primary)]);
        // A new generated directory and any valid filename are discovered live.
        const QString generated = config + QStringLiteral("/generated/my-palette.json");
        QVERIFY(writeJson(generated, matugenJson(red)));
        QTRY_COMPARE_WITH_TIMEOUT(theme.systemSchemePath(), generated, 3000);
        QCOMPARE(theme.colors()->primary(), red[std::size_t(MaterialScheme::Role::Primary)]);
        QVERIFY(writeJson(generated, matugenJson(green)));
        QTRY_COMPARE_WITH_TIMEOUT(theme.colors()->primary(), green[std::size_t(MaterialScheme::Role::Primary)], 3000);
        QVERIFY(QFile::remove(generated));
        QTRY_COMPARE_WITH_TIMEOUT(theme.systemSchemePath(), cached, 3000);
    }

    void findsDankMaterialShellsScheme()
    {
        MatugenPaths paths;
        const auto green = Theme::rolesFromSeed(Qt::green, true, false);
        const auto red = Theme::rolesFromSeed(Qt::red, true, false);
        const auto blue = Theme::rolesFromSeed(Qt::blue, true, false);
        const auto modes = [](const MaterialScheme::Roles &roles) {
            // as dms writes it: the matugen roles by mode under colors,
            // beside its terminal palette
            const QJsonObject scheme = QJsonDocument::fromJson(matugenJson(roles)).object();
            return QJsonDocument(QJsonObject{{"colors", QJsonObject{{"dark", scheme}, {"light", scheme}}},
                                             {"dank16", QJsonObject{{"color0", QJsonObject{{"dark", "#000000"}}}}},
                                             {"mode", "dark"}})
                .toJson();
        };
        const QString dms = paths.directory.filePath(QStringLiteral("cache/DankMaterialShell/dms-colors.json"));
        QVERIFY(writeJson(dms, modes(green)));
        // its other caches are never taken for a scheme
        QVERIFY(writeJson(paths.directory.filePath(QStringLiteral("cache/DankMaterialShell/cache.json")), matugenJson(blue)));
        // and it comes before a scheme in matugen's own folders
        QVERIFY(writeJson(paths.directory.filePath(QStringLiteral("config/matugen/colors.json")), matugenJson(red)));
        Settings settings(paths.directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeSource(QStringLiteral("system"));
        settings.setThemeMode(QStringLiteral("dark"));
        Theme theme(&settings);
        QVERIFY(theme.usingSystemScheme());
        QCOMPARE(theme.systemSchemePath(), dms);
        QCOMPARE(theme.colors()->primary(), green[std::size_t(MaterialScheme::Role::Primary)]);
        // a new wallpaper rewrites it, and the theme follows
        QVERIFY(writeJson(dms, modes(blue)));
        QTRY_COMPARE_WITH_TIMEOUT(theme.colors()->primary(), blue[std::size_t(MaterialScheme::Role::Primary)], 3000);
    }

    void dankMaterialShellsSchemeIsFoundOnceItAppears()
    {
        MatugenPaths paths;
        const auto green = Theme::rolesFromSeed(Qt::green, true, false);
        const auto red = Theme::rolesFromSeed(Qt::red, true, false);
        QDir().mkpath(paths.directory.filePath(QStringLiteral("cache/DankMaterialShell")));
        QVERIFY(writeJson(paths.directory.filePath(QStringLiteral("config/matugen/colors.json")), matugenJson(red)));
        Settings settings(paths.directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeSource(QStringLiteral("system"));
        Theme theme(&settings);
        QCOMPARE(theme.colors()->primary(), red[std::size_t(MaterialScheme::Role::Primary)]);
        // dms writing its scheme for the first time, after swiftify started
        const QString dms = paths.directory.filePath(QStringLiteral("cache/DankMaterialShell/dms-colors.json"));
        QVERIFY(writeJson(dms, matugenJson(green)));
        QTRY_COMPARE_WITH_TIMEOUT(theme.systemSchemePath(), dms, 3000);
        QCOMPARE(theme.colors()->primary(), green[std::size_t(MaterialScheme::Role::Primary)]);
    }

    void explicitFilesOverrideDiscoveryAndModesCanChange()
    {
        MatugenPaths paths;
        const auto day = Theme::rolesFromSeed(Qt::green, false, false);
        const auto night = Theme::rolesFromSeed(Qt::blue, true, false);
        const QString detected = paths.directory.filePath(QStringLiteral("config/matugen/colors.json"));
        const QString custom = paths.directory.filePath(QStringLiteral("elsewhere/custom.json"));
        QVERIFY(writeJson(detected, matugenJson(day)));
        const QJsonObject modes{{"colors", QJsonObject{{"dark", QJsonDocument::fromJson(matugenJson(night)).object()},
                                                      {"light", QJsonDocument::fromJson(matugenJson(day)).object()}}}};
        QVERIFY(writeJson(custom, QJsonDocument(modes).toJson()));
        Settings settings(paths.directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeSource(QStringLiteral("system"));
        settings.setThemeMode(QStringLiteral("dark"));
        settings.setMatugenColorsPath(QUrl::fromLocalFile(custom).toString());
        Theme theme(&settings);
        QCOMPARE(theme.systemSchemePath(), custom);
        QVERIFY(theme.dark());
        settings.setThemeMode(QStringLiteral("light"));
        QTRY_VERIFY(!theme.dark());
        QCOMPARE(theme.colors()->primary(), day[std::size_t(MaterialScheme::Role::Primary)]);
        settings.setMatugenColorsPath(custom + QStringLiteral(".missing"));
        QTRY_VERIFY(!theme.usingSystemScheme());
        settings.setMatugenColorsPath(QString());
        QTRY_COMPARE(theme.systemSchemePath(), detected);
        QVERIFY(theme.usingSystemScheme());
        QCOMPARE(theme.themeOptions(false).first().id, QStringLiteral("system"));
    }

    void olderExportsFillMissingModernRoles()
    {
        QJsonObject object = QJsonDocument::fromJson(matugenJson(Theme::rolesFromSeed(Qt::blue, true, false))).object();
        for (const QString &key : object.keys()) {
            if (key.contains(QLatin1StringView("fixed")) || key.contains(QLatin1StringView("surface_container"))
                || key == QLatin1StringView("surface_tint") || key == QLatin1StringView("surface_dim")
                || key == QLatin1StringView("surface_bright")) object.remove(key);
        }
        const auto roles = MaterialScheme::fromMatugenJson(QJsonDocument(object).toJson());
        QVERIFY(roles);
        for (const QColor &color : *roles) QVERIFY(color.isValid());
        object.insert(QStringLiteral("primary"), QStringLiteral("{{colors.primary.default.hex}}"));
        QVERIFY(!MaterialScheme::fromMatugenJson(QJsonDocument(object).toJson()));
    }

    void fallsBackWhenTheFileIsMissingOrIncomplete()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeSource(QStringLiteral("system"));
        const QString path = directory.filePath(QStringLiteral("colors.json"));
        settings.setMatugenColorsPath(path);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"primary\": \"#ff0000\"}");
        file.close();

        Theme theme(&settings);
        QVERIFY(!theme.usingSystemScheme());
        QVERIFY(theme.colors()->primary().isValid());
    }

    void everyThemeReadsInLightAndDark_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<bool>("dark");
        for (const themes::Family &family : themes::families()) {
            QTest::addRow("%s light", qPrintable(family.id)) << family.id << false;
            QTest::addRow("%s dark", qPrintable(family.id)) << family.id << true;
        }
    }

    // text on the page, and every accent with what is drawn on it, stays
    // legible: wcag's 4.5 for body text, 3 for the rest
    void everyThemeReadsInLightAndDark()
    {
        QFETCH(QString, id);
        QFETCH(bool, dark);
        const themes::Family *family = themes::find(id);
        QVERIFY(family);
        const MaterialScheme::Roles roles = themes::roles(dark ? family->dark : family->light, dark);
        const auto role = [&roles](MaterialScheme::Role which) { return roles[static_cast<std::size_t>(which)]; };
        using Role = MaterialScheme::Role;
        const auto check = [&](Role ink, Role ground, qreal minimum) {
            const qreal ratio = themes::contrast(role(ink), role(ground));
            QVERIFY2(ratio >= minimum, qPrintable(QStringLiteral("%1 on %2: %3 (%4 and %5)")
                                                      .arg(int(ink)).arg(int(ground)).arg(ratio, 0, 'f', 2)
                                                      .arg(role(ink).name(), role(ground).name())));
        };
        check(Role::OnSurface, Role::SurfaceContainerLow, 4.5);
        check(Role::OnSurface, Role::Background, 4.5);
        check(Role::OnSurfaceVariant, Role::SurfaceContainerLow, 3);
        check(Role::OnPrimary, Role::Primary, 3);
        check(Role::OnPrimaryContainer, Role::PrimaryContainer, 3);
        check(Role::OnSecondaryContainer, Role::SecondaryContainer, 3);
        check(Role::OnTertiaryContainer, Role::TertiaryContainer, 3);
        check(Role::OnError, Role::Error, 3);
        // a dark theme is dark and a light one light
        QCOMPARE(themes::contrast(role(Role::SurfaceContainerLow), Qt::black) < themes::contrast(role(Role::SurfaceContainerLow), Qt::white), dark);
    }

    void aThemeFollowsTheMode()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeSource(QStringLiteral("catppuccin-mocha"));
        settings.setThemeMode(QStringLiteral("dark"));
        Theme theme(&settings);
        QVERIFY(!theme.usingSystemScheme());
        QVERIFY(theme.dark());
        QCOMPARE(theme.colors()->background(), QColor(0x18, 0x18, 0x25));

        QSignalSpy changed(&theme, &Theme::schemeChanged);
        settings.setThemeMode(QStringLiteral("light"));
        QTRY_VERIFY(changed.count() > 0);
        QVERIFY(!theme.dark());
        QCOMPARE(theme.colors()->background(), QColor(0xe6, 0xe9, 0xef));

        // an id nobody knows is the app's own colours rather than nothing
        settings.setThemeSource(QStringLiteral("not-a-theme"));
        QTRY_VERIFY(theme.colors()->primary().isValid());
        QCOMPARE(theme.themeOptions(false).size(), themes::families().size() + (theme.themeOptions(false).first().id == QLatin1String("system") ? 3 : 2));
    }
};

QTEST_MAIN(ThemeTest)
#include "tst_theme.moc"
