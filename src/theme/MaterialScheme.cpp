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

#include "theme/MaterialScheme.h"

#include "cpp/dynamiccolor/dynamic_scheme.h"
#include "cpp/cam/hct.h"
#include "cpp/scheme/scheme_tonal_spot.h"
#include "theme/ColorMath.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <cmath>

namespace swiftify {

namespace {

constexpr std::array<const char *, static_cast<std::size_t>(MaterialScheme::Role::Count)> kRoleKeys {
    "background",
    "onBackground",
    "surface",
    "surfaceDim",
    "surfaceBright",
    "surfaceContainerLowest",
    "surfaceContainerLow",
    "surfaceContainer",
    "surfaceContainerHigh",
    "surfaceContainerHighest",
    "onSurface",
    "surfaceVariant",
    "onSurfaceVariant",
    "inverseSurface",
    "inverseOnSurface",
    "outline",
    "outlineVariant",
    "shadow",
    "scrim",
    "surfaceTint",
    "primary",
    "onPrimary",
    "primaryContainer",
    "onPrimaryContainer",
    "inversePrimary",
    "secondary",
    "onSecondary",
    "secondaryContainer",
    "onSecondaryContainer",
    "tertiary",
    "onTertiary",
    "tertiaryContainer",
    "onTertiaryContainer",
    "error",
    "onError",
    "errorContainer",
    "onErrorContainer",
    "primaryFixed",
    "primaryFixedDim",
    "onPrimaryFixed",
    "onPrimaryFixedVariant",
    "secondaryFixed",
    "secondaryFixedDim",
    "onSecondaryFixed",
    "onSecondaryFixedVariant",
    "tertiaryFixed",
    "tertiaryFixedDim",
    "onTertiaryFixed",
    "onTertiaryFixedVariant",
};

QColor matugenColor(QJsonValue value, bool dark)
{
    for (int depth = 0; value.isObject() && depth < 6; ++depth) {
        const QJsonObject object = value.toObject();
        bool found = false;
        for (const QString &key : {dark ? QStringLiteral("dark") : QStringLiteral("light"),
                                  QStringLiteral("default"), QStringLiteral("color"),
                                  QStringLiteral("hex"), QStringLiteral("value"),
                                  dark ? QStringLiteral("light") : QStringLiteral("dark")}) {
            if (object.contains(key)) {
                value = object.value(key);
                found = true;
                break;
            }
        }
        if (!found) return {};
    }
    const QString text = value.toString().trimmed();
    QColor color = QColor::fromString(text);
    if (color.isValid()) return color;
    static const QRegularExpression strippedHex(QStringLiteral("^[0-9a-fA-F]{6}$"));
    if (strippedHex.match(text).hasMatch()) return QColor::fromString(u'#' + text);

    // Matugen --json also exports CSS rgb(a) and hsl(a) strings.
    static const QRegularExpression css(QStringLiteral("^(rgb|rgba|hsl|hsla)\\(([^()]+)\\)$"),
                                        QRegularExpression::CaseInsensitiveOption);
    const auto match = css.match(text);
    if (!match.hasMatch()) return {};
    const QString format = match.captured(1).toLower();
    const QStringList parts = match.captured(2).split(QRegularExpression(QStringLiteral("[,\\s/]+")), Qt::SkipEmptyParts);
    if (parts.size() != (format.endsWith(u'a') ? 4 : 3)) return {};
    const auto unit = [](QString part, qreal maximum) -> std::optional<qreal> {
        if (part.endsWith(u'%')) {
            part.chop(1);
            maximum = 100;
        }
        bool ok = false;
        const qreal value = part.toDouble(&ok);
        if (!ok || !std::isfinite(value) || value < 0 || value > maximum) return std::nullopt;
        return value / maximum;
    };
    const auto alpha = parts.size() == 4 ? unit(parts[3], 1) : std::optional<qreal>(1);
    if (!alpha) return {};
    if (format.startsWith(QLatin1StringView("rgb"))) {
        const auto r = unit(parts[0], 255), g = unit(parts[1], 255), b = unit(parts[2], 255);
        return r && g && b ? QColor::fromRgbF(*r, *g, *b, *alpha) : QColor();
    }
    bool ok = false;
    QString hue = parts[0];
    if (hue.endsWith(QLatin1StringView("deg"))) hue.chop(3);
    const qreal degrees = hue.toDouble(&ok);
    const auto saturation = unit(parts[1], 1), lightness = unit(parts[2], 1);
    if (!ok || !std::isfinite(degrees) || !saturation || !lightness) return {};
    return QColor::fromHslF(std::fmod(std::fmod(degrees, 360) + 360, 360) / 360,
                            *saturation, *lightness, *alpha);
}

} // namespace

MaterialScheme::MaterialScheme(QObject *parent)
    : QObject(parent)
{
}

QString MaterialScheme::roleKey(Role role)
{
    return QString::fromLatin1(kRoleKeys[static_cast<std::size_t>(role)]);
}

void MaterialScheme::setRoles(const Roles &roles, bool dark)
{
    if (roles == m_roles && dark == m_dark && m_derived[0].isValid()) {
        return;
    }
    m_roles = roles;
    m_dark = dark;
    derive();
    emit changed();
}

void MaterialScheme::derive()
{
    const auto r = [this](Role role) { return m_roles[static_cast<std::size_t>(role)]; };
    const auto d = [this](Derived derived) { return m_derived[static_cast<std::size_t>(derived)]; };
    using color::mix;
    using color::withAlpha;
    // in order: later entries read earlier ones through d().
    m_derived[0] = r(Role::Outline);
    m_derived[1] = r(Role::Background);
    m_derived[2] = r(Role::OnBackground);
    m_derived[3] = mix(r(Role::Background), r(Role::OnBackground), 0.9);
    m_derived[4] = mix(r(Role::Background), r(Role::OnBackground), 0.8);
    m_derived[5] = mix(r(Role::OutlineVariant), r(Role::Background), 0.4);
    m_derived[6] = r(Role::SurfaceContainerLow);
    m_derived[7] = r(Role::OnSurfaceVariant);
    m_derived[8] = mix(r(Role::OnSurfaceVariant), r(Role::SurfaceContainerLow), 0.45);
    m_derived[9] = mix(r(Role::SurfaceContainerLow), r(Role::OnSurfaceVariant), 0.92);
    m_derived[10] = mix(r(Role::SurfaceContainerLow), r(Role::OnSurfaceVariant), 0.85);
    m_derived[11] = r(Role::SurfaceContainer);
    m_derived[12] = r(Role::OnSurface);
    m_derived[13] = mix(r(Role::SurfaceContainer), r(Role::OnSurface), 0.90);
    m_derived[14] = mix(r(Role::SurfaceContainer), r(Role::OnSurface), 0.80);
    m_derived[15] = mix(r(Role::SurfaceContainer), r(Role::Background), 0.8);
    m_derived[16] = mix(r(Role::OnSurface), r(Role::Background), 0.4);
    m_derived[17] = r(Role::SurfaceContainerHigh);
    m_derived[18] = r(Role::OnSurface);
    m_derived[19] = mix(r(Role::SurfaceContainerHigh), r(Role::OnSurface), 0.90);
    m_derived[20] = mix(r(Role::SurfaceContainerHigh), r(Role::OnSurface), 0.80);
    m_derived[21] = r(Role::SurfaceContainerHighest);
    m_derived[22] = r(Role::OnSurface);
    m_derived[23] = mix(r(Role::SurfaceContainerHighest), r(Role::OnSurface), 0.90);
    m_derived[24] = mix(r(Role::SurfaceContainerHighest), r(Role::OnSurface), 0.80);
    m_derived[25] = mix(r(Role::Primary), d(Derived::Layer1Hover), 0.87);
    m_derived[26] = mix(r(Role::Primary), d(Derived::Layer1Active), 0.7);
    m_derived[27] = mix(r(Role::PrimaryContainer), r(Role::OnPrimaryContainer), 0.9);
    m_derived[28] = mix(r(Role::PrimaryContainer), r(Role::OnPrimaryContainer), 0.8);
    m_derived[29] = mix(r(Role::Secondary), d(Derived::Layer1Hover), 0.85);
    m_derived[30] = mix(r(Role::Secondary), d(Derived::Layer1Active), 0.4);
    m_derived[31] = mix(r(Role::SecondaryContainer), r(Role::OnSecondaryContainer), 0.90);
    m_derived[32] = mix(r(Role::SecondaryContainer), r(Role::OnSecondaryContainer), 0.54);
    m_derived[33] = mix(r(Role::Tertiary), d(Derived::Layer1Hover), 0.85);
    m_derived[34] = mix(r(Role::Tertiary), d(Derived::Layer1Active), 0.4);
    m_derived[35] = mix(r(Role::TertiaryContainer), r(Role::OnTertiaryContainer), 0.90);
    m_derived[36] = mix(r(Role::TertiaryContainer), d(Derived::Layer1Active), 0.54);
    m_derived[37] = mix(r(Role::SurfaceContainerHighest), r(Role::OnSurface), 0.95);
    m_derived[38] = mix(r(Role::SurfaceContainerHighest), r(Role::OnSurface), 0.85);
    m_derived[39] = r(Role::InverseSurface);
    m_derived[40] = r(Role::InverseOnSurface);
    m_derived[41] = withAlpha(r(Role::Scrim), 0.5);
    m_derived[42] = withAlpha(r(Role::Shadow), 0.3);
    m_derived[43] = mix(r(Role::Error), d(Derived::Layer1Hover), 0.85);
    m_derived[44] = mix(r(Role::Error), d(Derived::Layer1Active), 0.7);
    m_derived[45] = mix(r(Role::ErrorContainer), r(Role::OnErrorContainer), 0.90);
    m_derived[46] = mix(r(Role::ErrorContainer), r(Role::OnErrorContainer), 0.70);
}

MaterialScheme::Roles MaterialScheme::fromDynamicScheme(
    const material_color_utilities::DynamicScheme &scheme)
{
    const auto argb = [](material_color_utilities::Argb value) { return QColor::fromRgba(value); };
    return {
        argb(scheme.GetBackground()),
        argb(scheme.GetOnBackground()),
        argb(scheme.GetSurface()),
        argb(scheme.GetSurfaceDim()),
        argb(scheme.GetSurfaceBright()),
        argb(scheme.GetSurfaceContainerLowest()),
        argb(scheme.GetSurfaceContainerLow()),
        argb(scheme.GetSurfaceContainer()),
        argb(scheme.GetSurfaceContainerHigh()),
        argb(scheme.GetSurfaceContainerHighest()),
        argb(scheme.GetOnSurface()),
        argb(scheme.GetSurfaceVariant()),
        argb(scheme.GetOnSurfaceVariant()),
        argb(scheme.GetInverseSurface()),
        argb(scheme.GetInverseOnSurface()),
        argb(scheme.GetOutline()),
        argb(scheme.GetOutlineVariant()),
        argb(scheme.GetShadow()),
        argb(scheme.GetScrim()),
        argb(scheme.GetSurfaceTint()),
        argb(scheme.GetPrimary()),
        argb(scheme.GetOnPrimary()),
        argb(scheme.GetPrimaryContainer()),
        argb(scheme.GetOnPrimaryContainer()),
        argb(scheme.GetInversePrimary()),
        argb(scheme.GetSecondary()),
        argb(scheme.GetOnSecondary()),
        argb(scheme.GetSecondaryContainer()),
        argb(scheme.GetOnSecondaryContainer()),
        argb(scheme.GetTertiary()),
        argb(scheme.GetOnTertiary()),
        argb(scheme.GetTertiaryContainer()),
        argb(scheme.GetOnTertiaryContainer()),
        argb(scheme.GetError()),
        argb(scheme.GetOnError()),
        argb(scheme.GetErrorContainer()),
        argb(scheme.GetOnErrorContainer()),
        argb(scheme.GetPrimaryFixed()),
        argb(scheme.GetPrimaryFixedDim()),
        argb(scheme.GetOnPrimaryFixed()),
        argb(scheme.GetOnPrimaryFixedVariant()),
        argb(scheme.GetSecondaryFixed()),
        argb(scheme.GetSecondaryFixedDim()),
        argb(scheme.GetOnSecondaryFixed()),
        argb(scheme.GetOnSecondaryFixedVariant()),
        argb(scheme.GetTertiaryFixed()),
        argb(scheme.GetTertiaryFixedDim()),
        argb(scheme.GetOnTertiaryFixed()),
        argb(scheme.GetOnTertiaryFixedVariant()),
    };
}

std::optional<MaterialScheme::Roles> MaterialScheme::fromMatugenJson(const QByteArray &json, bool dark)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    QJsonObject object = document.object();
    // Both the CLI export and custom templates can wrap their schemes.
    for (int depth = 0; depth < 4; ++depth) {
        bool found = false;
        for (const QString &key : {QStringLiteral("colors"), QStringLiteral("schemes"),
                                  dark ? QStringLiteral("dark") : QStringLiteral("light"),
                                  QStringLiteral("default"),
                                  dark ? QStringLiteral("light") : QStringLiteral("dark")}) {
            if (object.value(key).isObject()) {
                object = object.value(key).toObject();
                found = true;
                break;
            }
        }
        if (!found) break;
    }

    Roles roles;
    for (std::size_t index = 0; index < roles.size(); ++index) {
        const QString camel = QString::fromLatin1(kRoleKeys[index]);
        QString snake;
        for (const QChar character : camel) {
            if (character.isUpper()) {
                snake += u'_';
            }
            snake += character.toLower();
        }
        QString prefixed = camel;
        prefixed[0] = prefixed[0].toUpper();
        prefixed.prepend(QStringLiteral("m3"));

        QJsonValue value;
        for (const QString &key : {snake, camel, prefixed}) {
            value = object.value(key);
            if (!value.isUndefined()) {
                break;
            }
        }
        if (value.isUndefined()) continue;
        const QColor color = matugenColor(value, dark);
        if (!color.isValid()) {
            return std::nullopt;
        }
        roles[index] = color;
    }
    // Validate a real Material scheme rather than treating an unrelated JSON
    // file or a template containing unresolved placeholders as a theme.
    for (const Role role : {Role::Background, Role::OnBackground, Role::Surface, Role::OnSurface,
                           Role::Primary, Role::OnPrimary, Role::Secondary, Role::OnSecondary,
                           Role::Tertiary, Role::OnTertiary, Role::Error, Role::OnError, Role::Outline}) {
        if (!roles[static_cast<std::size_t>(role)].isValid()) return std::nullopt;
    }
    const bool isDark = color::luminance709(roles[static_cast<std::size_t>(Role::Background)]) < 0.5;
    using namespace material_color_utilities;
    const auto fallback = fromDynamicScheme(SchemeTonalSpot(Hct(roles[static_cast<std::size_t>(Role::Primary)].rgba() | 0xff000000u), isDark, 0));
    for (std::size_t index = 0; index < roles.size(); ++index) {
        if (!roles[index].isValid()) roles[index] = fallback[index];
    }
    return roles;
}

MaterialScheme::Roles MaterialScheme::interpolate(const Roles &from, const Roles &to, qreal progress)
{
    Roles roles;
    for (std::size_t index = 0; index < roles.size(); ++index) {
        roles[index] = color::mix(to[index], from[index], progress);
    }
    return roles;
}

} // namespace swiftify
