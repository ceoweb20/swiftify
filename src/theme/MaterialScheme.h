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

// written out from a role list once. a new role needs its Q_PROPERTY, its enum
// entry, its getter and its key in MaterialScheme.cpp, all in the same order.
#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <array>
#include <cstddef>
#include <optional>

namespace material_color_utilities {
struct DynamicScheme;
}

namespace swiftify {

/// one material 3 colour scheme: the 49 roles material defines, plus the
/// layer, hover and pressed colours end4 derives from them. the app chrome, the
/// player surfaces and anything else with its own colours each hold one.
class MaterialScheme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Schemes come from Theme and PlayerScheme.")

    Q_PROPERTY(bool dark READ dark NOTIFY changed FINAL)
    Q_PROPERTY(QColor background READ background NOTIFY changed FINAL)
    Q_PROPERTY(QColor onBackground READ onBackground NOTIFY changed FINAL)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceDim READ surfaceDim NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceBright READ surfaceBright NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceContainerLowest READ surfaceContainerLowest NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceContainerLow READ surfaceContainerLow NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceContainer READ surfaceContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceContainerHigh READ surfaceContainerHigh NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceContainerHighest READ surfaceContainerHighest NOTIFY changed FINAL)
    Q_PROPERTY(QColor onSurface READ onSurface NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceVariant READ surfaceVariant NOTIFY changed FINAL)
    Q_PROPERTY(QColor onSurfaceVariant READ onSurfaceVariant NOTIFY changed FINAL)
    Q_PROPERTY(QColor inverseSurface READ inverseSurface NOTIFY changed FINAL)
    Q_PROPERTY(QColor inverseOnSurface READ inverseOnSurface NOTIFY changed FINAL)
    Q_PROPERTY(QColor outline READ outline NOTIFY changed FINAL)
    Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY changed FINAL)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY changed FINAL)
    Q_PROPERTY(QColor scrim READ scrim NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceTint READ surfaceTint NOTIFY changed FINAL)
    Q_PROPERTY(QColor primary READ primary NOTIFY changed FINAL)
    Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY changed FINAL)
    Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor onPrimaryContainer READ onPrimaryContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor inversePrimary READ inversePrimary NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY changed FINAL)
    Q_PROPERTY(QColor onSecondary READ onSecondary NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondaryContainer READ secondaryContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor onSecondaryContainer READ onSecondaryContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiary READ tertiary NOTIFY changed FINAL)
    Q_PROPERTY(QColor onTertiary READ onTertiary NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiaryContainer READ tertiaryContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor onTertiaryContainer READ onTertiaryContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor error READ error NOTIFY changed FINAL)
    Q_PROPERTY(QColor onError READ onError NOTIFY changed FINAL)
    Q_PROPERTY(QColor errorContainer READ errorContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor onErrorContainer READ onErrorContainer NOTIFY changed FINAL)
    Q_PROPERTY(QColor primaryFixed READ primaryFixed NOTIFY changed FINAL)
    Q_PROPERTY(QColor primaryFixedDim READ primaryFixedDim NOTIFY changed FINAL)
    Q_PROPERTY(QColor onPrimaryFixed READ onPrimaryFixed NOTIFY changed FINAL)
    Q_PROPERTY(QColor onPrimaryFixedVariant READ onPrimaryFixedVariant NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondaryFixed READ secondaryFixed NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondaryFixedDim READ secondaryFixedDim NOTIFY changed FINAL)
    Q_PROPERTY(QColor onSecondaryFixed READ onSecondaryFixed NOTIFY changed FINAL)
    Q_PROPERTY(QColor onSecondaryFixedVariant READ onSecondaryFixedVariant NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiaryFixed READ tertiaryFixed NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiaryFixedDim READ tertiaryFixedDim NOTIFY changed FINAL)
    Q_PROPERTY(QColor onTertiaryFixed READ onTertiaryFixed NOTIFY changed FINAL)
    Q_PROPERTY(QColor onTertiaryFixedVariant READ onTertiaryFixedVariant NOTIFY changed FINAL)
    Q_PROPERTY(QColor subtext READ subtext NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer0 READ layer0 NOTIFY changed FINAL)
    Q_PROPERTY(QColor onLayer0 READ onLayer0 NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer0Hover READ layer0Hover NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer0Active READ layer0Active NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer0Border READ layer0Border NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer1 READ layer1 NOTIFY changed FINAL)
    Q_PROPERTY(QColor onLayer1 READ onLayer1 NOTIFY changed FINAL)
    Q_PROPERTY(QColor onLayer1Inactive READ onLayer1Inactive NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer1Hover READ layer1Hover NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer1Active READ layer1Active NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer2 READ layer2 NOTIFY changed FINAL)
    Q_PROPERTY(QColor onLayer2 READ onLayer2 NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer2Hover READ layer2Hover NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer2Active READ layer2Active NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer2Disabled READ layer2Disabled NOTIFY changed FINAL)
    Q_PROPERTY(QColor onLayer2Disabled READ onLayer2Disabled NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer3 READ layer3 NOTIFY changed FINAL)
    Q_PROPERTY(QColor onLayer3 READ onLayer3 NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer3Hover READ layer3Hover NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer3Active READ layer3Active NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer4 READ layer4 NOTIFY changed FINAL)
    Q_PROPERTY(QColor onLayer4 READ onLayer4 NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer4Hover READ layer4Hover NOTIFY changed FINAL)
    Q_PROPERTY(QColor layer4Active READ layer4Active NOTIFY changed FINAL)
    Q_PROPERTY(QColor primaryHover READ primaryHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor primaryActive READ primaryActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor primaryContainerHover READ primaryContainerHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor primaryContainerActive READ primaryContainerActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondaryHover READ secondaryHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondaryActive READ secondaryActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondaryContainerHover READ secondaryContainerHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor secondaryContainerActive READ secondaryContainerActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiaryHover READ tertiaryHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiaryActive READ tertiaryActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiaryContainerHover READ tertiaryContainerHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor tertiaryContainerActive READ tertiaryContainerActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceContainerHighestHover READ surfaceContainerHighestHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor surfaceContainerHighestActive READ surfaceContainerHighestActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor tooltip READ tooltip NOTIFY changed FINAL)
    Q_PROPERTY(QColor onTooltip READ onTooltip NOTIFY changed FINAL)
    Q_PROPERTY(QColor scrimOverlay READ scrimOverlay NOTIFY changed FINAL)
    Q_PROPERTY(QColor shadowColor READ shadowColor NOTIFY changed FINAL)
    Q_PROPERTY(QColor errorHover READ errorHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor errorActive READ errorActive NOTIFY changed FINAL)
    Q_PROPERTY(QColor errorContainerHover READ errorContainerHover NOTIFY changed FINAL)
    Q_PROPERTY(QColor errorContainerActive READ errorContainerActive NOTIFY changed FINAL)

public:
    enum class Role : std::size_t {
        Background,
        OnBackground,
        Surface,
        SurfaceDim,
        SurfaceBright,
        SurfaceContainerLowest,
        SurfaceContainerLow,
        SurfaceContainer,
        SurfaceContainerHigh,
        SurfaceContainerHighest,
        OnSurface,
        SurfaceVariant,
        OnSurfaceVariant,
        InverseSurface,
        InverseOnSurface,
        Outline,
        OutlineVariant,
        Shadow,
        Scrim,
        SurfaceTint,
        Primary,
        OnPrimary,
        PrimaryContainer,
        OnPrimaryContainer,
        InversePrimary,
        Secondary,
        OnSecondary,
        SecondaryContainer,
        OnSecondaryContainer,
        Tertiary,
        OnTertiary,
        TertiaryContainer,
        OnTertiaryContainer,
        Error,
        OnError,
        ErrorContainer,
        OnErrorContainer,
        PrimaryFixed,
        PrimaryFixedDim,
        OnPrimaryFixed,
        OnPrimaryFixedVariant,
        SecondaryFixed,
        SecondaryFixedDim,
        OnSecondaryFixed,
        OnSecondaryFixedVariant,
        TertiaryFixed,
        TertiaryFixedDim,
        OnTertiaryFixed,
        OnTertiaryFixedVariant,
        Count,
    };
    enum class Derived : std::size_t {
        Subtext,
        Layer0,
        OnLayer0,
        Layer0Hover,
        Layer0Active,
        Layer0Border,
        Layer1,
        OnLayer1,
        OnLayer1Inactive,
        Layer1Hover,
        Layer1Active,
        Layer2,
        OnLayer2,
        Layer2Hover,
        Layer2Active,
        Layer2Disabled,
        OnLayer2Disabled,
        Layer3,
        OnLayer3,
        Layer3Hover,
        Layer3Active,
        Layer4,
        OnLayer4,
        Layer4Hover,
        Layer4Active,
        PrimaryHover,
        PrimaryActive,
        PrimaryContainerHover,
        PrimaryContainerActive,
        SecondaryHover,
        SecondaryActive,
        SecondaryContainerHover,
        SecondaryContainerActive,
        TertiaryHover,
        TertiaryActive,
        TertiaryContainerHover,
        TertiaryContainerActive,
        SurfaceContainerHighestHover,
        SurfaceContainerHighestActive,
        Tooltip,
        OnTooltip,
        ScrimOverlay,
        ShadowColor,
        ErrorHover,
        ErrorActive,
        ErrorContainerHover,
        ErrorContainerActive,
        Count,
    };
    using Roles = std::array<QColor, static_cast<std::size_t>(Role::Count)>;

    explicit MaterialScheme(QObject *parent = nullptr);

    /// replaces every colour, recomputes the derived ones and notifies once if
    /// anything changed.
    void setRoles(const Roles &roles, bool dark);
    const Roles &roles() const { return m_roles; }
    bool dark() const { return m_dark; }

    static Roles fromDynamicScheme(const material_color_utilities::DynamicScheme &scheme);
    /// Matugen exports and templates: flat roles, nested schemes, or a mode
    /// per role. Older schemes may omit newer Material roles.
    static std::optional<Roles> fromMatugenJson(const QByteArray &json, bool dark = true);
    /// roles interpolated between two schemes, for animated palette changes.
    static Roles interpolate(const Roles &from, const Roles &to, qreal progress);
    static QString roleKey(Role role);

    QColor background() const { return m_roles[0]; }
    QColor onBackground() const { return m_roles[1]; }
    QColor surface() const { return m_roles[2]; }
    QColor surfaceDim() const { return m_roles[3]; }
    QColor surfaceBright() const { return m_roles[4]; }
    QColor surfaceContainerLowest() const { return m_roles[5]; }
    QColor surfaceContainerLow() const { return m_roles[6]; }
    QColor surfaceContainer() const { return m_roles[7]; }
    QColor surfaceContainerHigh() const { return m_roles[8]; }
    QColor surfaceContainerHighest() const { return m_roles[9]; }
    QColor onSurface() const { return m_roles[10]; }
    QColor surfaceVariant() const { return m_roles[11]; }
    QColor onSurfaceVariant() const { return m_roles[12]; }
    QColor inverseSurface() const { return m_roles[13]; }
    QColor inverseOnSurface() const { return m_roles[14]; }
    QColor outline() const { return m_roles[15]; }
    QColor outlineVariant() const { return m_roles[16]; }
    QColor shadow() const { return m_roles[17]; }
    QColor scrim() const { return m_roles[18]; }
    QColor surfaceTint() const { return m_roles[19]; }
    QColor primary() const { return m_roles[20]; }
    QColor onPrimary() const { return m_roles[21]; }
    QColor primaryContainer() const { return m_roles[22]; }
    QColor onPrimaryContainer() const { return m_roles[23]; }
    QColor inversePrimary() const { return m_roles[24]; }
    QColor secondary() const { return m_roles[25]; }
    QColor onSecondary() const { return m_roles[26]; }
    QColor secondaryContainer() const { return m_roles[27]; }
    QColor onSecondaryContainer() const { return m_roles[28]; }
    QColor tertiary() const { return m_roles[29]; }
    QColor onTertiary() const { return m_roles[30]; }
    QColor tertiaryContainer() const { return m_roles[31]; }
    QColor onTertiaryContainer() const { return m_roles[32]; }
    QColor error() const { return m_roles[33]; }
    QColor onError() const { return m_roles[34]; }
    QColor errorContainer() const { return m_roles[35]; }
    QColor onErrorContainer() const { return m_roles[36]; }
    QColor primaryFixed() const { return m_roles[37]; }
    QColor primaryFixedDim() const { return m_roles[38]; }
    QColor onPrimaryFixed() const { return m_roles[39]; }
    QColor onPrimaryFixedVariant() const { return m_roles[40]; }
    QColor secondaryFixed() const { return m_roles[41]; }
    QColor secondaryFixedDim() const { return m_roles[42]; }
    QColor onSecondaryFixed() const { return m_roles[43]; }
    QColor onSecondaryFixedVariant() const { return m_roles[44]; }
    QColor tertiaryFixed() const { return m_roles[45]; }
    QColor tertiaryFixedDim() const { return m_roles[46]; }
    QColor onTertiaryFixed() const { return m_roles[47]; }
    QColor onTertiaryFixedVariant() const { return m_roles[48]; }
    QColor subtext() const { return m_derived[0]; }
    QColor layer0() const { return m_derived[1]; }
    QColor onLayer0() const { return m_derived[2]; }
    QColor layer0Hover() const { return m_derived[3]; }
    QColor layer0Active() const { return m_derived[4]; }
    QColor layer0Border() const { return m_derived[5]; }
    QColor layer1() const { return m_derived[6]; }
    QColor onLayer1() const { return m_derived[7]; }
    QColor onLayer1Inactive() const { return m_derived[8]; }
    QColor layer1Hover() const { return m_derived[9]; }
    QColor layer1Active() const { return m_derived[10]; }
    QColor layer2() const { return m_derived[11]; }
    QColor onLayer2() const { return m_derived[12]; }
    QColor layer2Hover() const { return m_derived[13]; }
    QColor layer2Active() const { return m_derived[14]; }
    QColor layer2Disabled() const { return m_derived[15]; }
    QColor onLayer2Disabled() const { return m_derived[16]; }
    QColor layer3() const { return m_derived[17]; }
    QColor onLayer3() const { return m_derived[18]; }
    QColor layer3Hover() const { return m_derived[19]; }
    QColor layer3Active() const { return m_derived[20]; }
    QColor layer4() const { return m_derived[21]; }
    QColor onLayer4() const { return m_derived[22]; }
    QColor layer4Hover() const { return m_derived[23]; }
    QColor layer4Active() const { return m_derived[24]; }
    QColor primaryHover() const { return m_derived[25]; }
    QColor primaryActive() const { return m_derived[26]; }
    QColor primaryContainerHover() const { return m_derived[27]; }
    QColor primaryContainerActive() const { return m_derived[28]; }
    QColor secondaryHover() const { return m_derived[29]; }
    QColor secondaryActive() const { return m_derived[30]; }
    QColor secondaryContainerHover() const { return m_derived[31]; }
    QColor secondaryContainerActive() const { return m_derived[32]; }
    QColor tertiaryHover() const { return m_derived[33]; }
    QColor tertiaryActive() const { return m_derived[34]; }
    QColor tertiaryContainerHover() const { return m_derived[35]; }
    QColor tertiaryContainerActive() const { return m_derived[36]; }
    QColor surfaceContainerHighestHover() const { return m_derived[37]; }
    QColor surfaceContainerHighestActive() const { return m_derived[38]; }
    QColor tooltip() const { return m_derived[39]; }
    QColor onTooltip() const { return m_derived[40]; }
    QColor scrimOverlay() const { return m_derived[41]; }
    QColor shadowColor() const { return m_derived[42]; }
    QColor errorHover() const { return m_derived[43]; }
    QColor errorActive() const { return m_derived[44]; }
    QColor errorContainerHover() const { return m_derived[45]; }
    QColor errorContainerActive() const { return m_derived[46]; }

signals:
    void changed();

private:
    void derive();

    Roles m_roles {};
    std::array<QColor, static_cast<std::size_t>(Derived::Count)> m_derived {};
    bool m_dark = true;
};

} // namespace swiftify
