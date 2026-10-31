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

#include "theme/Themes.h"

#include "theme/ColorMath.h"

#include <cmath>

namespace swiftify::themes {

namespace {

// in-between shades a palette has no name for are blended from its own
// neighbours, and noted where they are

// latte, the light half of every catppuccin flavour
constexpr Variant kLatte {
    0xffeff1f5, // base
    0xffe6e9ef, // mantle
    0xffeff1f5, // base
    0xffdce0e8, // crust
    0xffccd0da, // surface0
    0xffbcc0cc, // surface1
    0xff4c4f69, // text
    0xff6c6f85, // subtext0
    0xff8c8fa1, // overlay1
    0xff8839ef, // mauve
    0xffea76cb, // pink
    0xff179299, // teal
    0xffd20f39, // red
};

const QList<Family> kFamilies {
    // nothing special: white or near-black grounds, greys, and the text's
    // own colour for an accent
    {QStringLiteral("default"), QStringLiteral("Default"),
     {0xffffffff, 0xfff2f2f3, 0xffffffff, 0xffebebec, 0xffe1e1e3, 0xffd0d0d3, 0xff1b1b1d, 0xff5c5c61, 0xff8a8a90,
      0xff1b1b1d, 0xff5c5c61, 0xff6f6f75, 0xffc62828},
     {0xff0c0c0d, 0xff111113, 0xff19191b, 0xff232326, 0xff2e2e31, 0xff3b3b3f, 0xffececee, 0xffa9a9ae, 0xff7c7c82,
      0xffececee, 0xffb4b4b9, 0xff9a9aa0, 0xffef5350}},
    // spotify's own: black and its greys in the dark, white in the light,
    // and its green for the highlights. the light green is a shade deeper,
    // so it still reads on white
    {QStringLiteral("spotify"), QStringLiteral("Spotify"),
     {0xffffffff, 0xfff0f0f0, 0xffffffff, 0xfff6f6f6, 0xffededed, 0xffdedede, 0xff121212, 0xff6a6a6a, 0xff8f8f8f,
      0xff1aa34a, 0xff6a6a6a, 0xff169c46, 0xffe91429},
     {0xff000000, 0xff000000, 0xff121212, 0xff1f1f1f, 0xff2a2a2a, 0xff3e3e3e, 0xffffffff, 0xffb3b3b3, 0xff7c7c7c,
      0xff1ed760, 0xffb3b3b3, 0xff1db954, 0xfff3727f}},
    {QStringLiteral("catppuccin-mocha"), QStringLiteral("Catppuccin Mocha"), kLatte,
     {0xff11111b, 0xff181825, 0xff1e1e2e, 0xff313244, 0xff45475a, 0xff585b70, 0xffcdd6f4, 0xffa6adc8, 0xff7f849c,
      0xffcba6f7, 0xfff5c2e7, 0xff94e2d5, 0xfff38ba8}},
    {QStringLiteral("catppuccin-macchiato"), QStringLiteral("Catppuccin Macchiato"), kLatte,
     {0xff181926, 0xff1e2030, 0xff24273a, 0xff363a4f, 0xff494d64, 0xff5b6078, 0xffcad3f5, 0xffa5adcb, 0xff8087a2,
      0xffc6a0f6, 0xfff5bde6, 0xff8bd5ca, 0xffed8796}},
    {QStringLiteral("catppuccin-frappe"), QStringLiteral("Catppuccin Frappé"), kLatte,
     {0xff232634, 0xff292c3c, 0xff303446, 0xff414559, 0xff51576d, 0xff626880, 0xffc6d0f5, 0xffa5adce, 0xff838ba7,
      0xffca9ee6, 0xfff4b8e4, 0xff81c8be, 0xffe78284}},
    // rosé pine: base, surface, overlay, highlight med and high; iris, rose,
    // foam (or pine on dawn) and love
    {QStringLiteral("rosepine"), QStringLiteral("Rosé Pine"),
     {0xfffffaf3, 0xfffaf4ed, 0xfffffaf3, 0xfff2e9e1, 0xffdfdad9, 0xffcecacd, 0xff575279, 0xff797593, 0xff9893a5,
      0xff907aa9, 0xffd7827e, 0xff286983, 0xffb4637a},
     {0xff191724, 0xff191724, 0xff1f1d2e, 0xff26233a, 0xff403d52, 0xff524f67, 0xffe0def4, 0xff908caa, 0xff6e6a86,
      0xffc4a7e7, 0xffebbcba, 0xff9ccfd8, 0xffeb6f92}},
    {QStringLiteral("rosepine-moon"), QStringLiteral("Rosé Pine Moon"),
     {0xfffffaf3, 0xfffaf4ed, 0xfffffaf3, 0xfff2e9e1, 0xffdfdad9, 0xffcecacd, 0xff575279, 0xff797593, 0xff9893a5,
      0xff907aa9, 0xffd7827e, 0xff286983, 0xffb4637a},
     {0xff232136, 0xff232136, 0xff2a273f, 0xff393552, 0xff44415a, 0xff56526e, 0xffe0def4, 0xff908caa, 0xff6e6a86,
      0xffc4a7e7, 0xffea9a97, 0xff9ccfd8, 0xffeb6f92}},
    // gruvbox: bg0_h, bg0_s/bg0_h, bg0, bg1, bg2, bg3; fg1, fg4, gray; orange,
    // yellow, aqua and red (faded on light, bright on dark)
    {QStringLiteral("gruvbox"), QStringLiteral("Gruvbox"),
     {0xfff9f5d7, 0xfff2e5bc, 0xfffbf1c7, 0xffebdbb2, 0xffd5c4a1, 0xffbdae93, 0xff3c3836, 0xff7c6f64, 0xff928374,
      0xffaf3a03, 0xffb57614, 0xff427b58, 0xff9d0006},
     {0xff1d2021, 0xff1d2021, 0xff282828, 0xff3c3836, 0xff504945, 0xff665c54, 0xffebdbb2, 0xffa89984, 0xff928374,
      0xfffe8019, 0xfffabd2f, 0xff8ec07c, 0xfffb4934}},
    // nord: snow storm for the light grounds, polar night for the dark; frost
    // and aurora for the accents. nord names no mid greys, so subtext, muted
    // and the most raised light ground are blended between its own.
    {QStringLiteral("nord"), QStringLiteral("Nord"),
     {0xffeceff4, 0xffe5e9f0, 0xffeceff4, 0xffd8dee9, 0xffc8d0dc, 0xffb9c2d0, 0xff2e3440, 0xff4c566a, 0xff7b8394,
      0xff5e81ac, 0xffb48ead, 0xff8fbcbb, 0xffbf616a},
     {0xff2e3440, 0xff2e3440, 0xff3b4252, 0xff434c5e, 0xff4c566a, 0xff4c566a, 0xffeceff4, 0xffd8dee9, 0xff929aaa,
      0xff88c0d0, 0xff81a1c1, 0xffa3be8c, 0xffbf616a}},
    // tokyo night: day for light; bg_dark, bg, bg_highlight, fg_gutter,
    // terminal black; fg, fg_dark, comment; blue, magenta, cyan, red. day's
    // fg reads under 4.5:1 on its own grounds, so its text is a step deeper.
    {QStringLiteral("tokyonight"), QStringLiteral("Tokyo Night"),
     {0xffe9e9ed, 0xffd0d5e3, 0xffe1e2e7, 0xffc4c8da, 0xffb7bcd2, 0xffa8aecb, 0xff2f51a2, 0xff6172b0, 0xff848cb5,
      0xff2e7de9, 0xff9854f1, 0xff007197, 0xfff52a65},
     {0xff16161e, 0xff16161e, 0xff1a1b26, 0xff292e42, 0xff3b4261, 0xff414868, 0xffc0caf5, 0xffa9b1d6, 0xff565f89,
      0xff7aa2f7, 0xffbb9af7, 0xff7dcfff, 0xfff7768e}},
    {QStringLiteral("tokyonight-storm"), QStringLiteral("Tokyo Night Storm"),
     {0xffe9e9ed, 0xffd0d5e3, 0xffe1e2e7, 0xffc4c8da, 0xffb7bcd2, 0xffa8aecb, 0xff2f51a2, 0xff6172b0, 0xff848cb5,
      0xff2e7de9, 0xff9854f1, 0xff007197, 0xfff52a65},
     {0xff1f2335, 0xff1f2335, 0xff24283b, 0xff292e42, 0xff3b4261, 0xff414868, 0xffc0caf5, 0xffa9b1d6, 0xff565f89,
      0xff7aa2f7, 0xffbb9af7, 0xff7dcfff, 0xfff7768e}},
    // dracula, and alucard, its light counterpart; the raised greys and the
    // subtext between foreground and comment are blended
    {QStringLiteral("dracula"), QStringLiteral("Dracula"),
     {0xfffffbeb, 0xfff2eedc, 0xfffffbeb, 0xffe8e4d0, 0xffcfcfde, 0xffcfcfde, 0xff1f1f1f, 0xff45423a, 0xff6c664b,
      0xff644ac9, 0xffa3144d, 0xff036a96, 0xffcb3a2a},
     {0xff191a21, 0xff21222c, 0xff282a36, 0xff343746, 0xff44475a, 0xff44475a, 0xfff8f8f2, 0xffc3c6d7, 0xff6272a4,
      0xffbd93f9, 0xffff79c6, 0xff8be9fd, 0xffff5555}},
    // everforest, medium contrast: bg_dim, bg0, bg2, bg3, bg4 (bg5 on light);
    // fg, grey2, grey1; green, aqua, yellow, red
    {QStringLiteral("everforest"), QStringLiteral("Everforest"),
     {0xfffdf6e3, 0xffefebd4, 0xfffdf6e3, 0xffe6e2cc, 0xffe0dcc7, 0xffbdc3af, 0xff5c6a72, 0xff829181, 0xff939f91,
      0xff8da101, 0xff35a77c, 0xffdfa000, 0xfff85552},
     {0xff232a2e, 0xff232a2e, 0xff2d353b, 0xff3d484d, 0xff475258, 0xff4f585e, 0xffd3c6aa, 0xff9da9a0, 0xff859289,
      0xffa7c080, 0xff83c092, 0xffdbbc7f, 0xffe67e80}},
    // kanagawa: lotus for light, wave for dark; sumi ink and lotus white for
    // the grounds; crystal blue, oni violet, spring green and wave red
    {QStringLiteral("kanagawa"), QStringLiteral("Kanagawa"),
     {0xfff2ecbc, 0xffe5ddb0, 0xfff2ecbc, 0xffe7dba0, 0xffe4d794, 0xffc9c19a, 0xff545464, 0xff716e61, 0xff8a8980,
      0xff4d699b, 0xff624c83, 0xff6f894e, 0xffc84053},
     {0xff16161d, 0xff16161d, 0xff1f1f28, 0xff2a2a37, 0xff363646, 0xff54546d, 0xffdcd7ba, 0xffc8c093, 0xff727169,
      0xff7e9cd8, 0xff957fb8, 0xff98bb6c, 0xffe46876}},
    // solarized: base3 and base2 for light, base03 and base02 for dark, with
    // the raised steps blended beyond them; blue, cyan, yellow, red. light
    // text is base02, since base01 reads under 4.5:1 on base2.
    {QStringLiteral("solarized"), QStringLiteral("Solarized"),
     {0xfffdf6e3, 0xffeee8d5, 0xfffdf6e3, 0xffe6dfcb, 0xffddd6c1, 0xff93a1a1, 0xff073642, 0xff586e75, 0xff93a1a1,
      0xff268bd2, 0xff2aa198, 0xffb58900, 0xffdc322f},
     {0xff00212b, 0xff00212b, 0xff002b36, 0xff073642, 0xff0d4452, 0xff586e75, 0xff93a1a1, 0xff839496, 0xff586e75,
      0xff268bd2, 0xff2aa198, 0xffb58900, 0xffdc322f}},
    {QStringLiteral("aurora"), QStringLiteral("Aurora"),
     {0xfff8fcff, 0xffeaf2f8, 0xfff4f9ff, 0xffdeebf5, 0xffcfdeee, 0xffb7cadc, 0xff1e2d46, 0xff52657d, 0xff788ca4,
      0xff5b52bc, 0xff236f80, 0xff9c3874, 0xffb3261e},
     {0xff101321, 0xff141a2c, 0xff1a2338, 0xff25324c, 0xff344361, 0xff485674, 0xffdce7ff, 0xffa9bad9, 0xff7e90af,
      0xffb4a4ff, 0xff74d9c8, 0xfff1a0c6, 0xffffb4ab}},
    {QStringLiteral("desert-bloom"), QStringLiteral("Desert Bloom"),
     {0xfffffaf3, 0xfff5e9dc, 0xfffff4e7, 0xffefddcc, 0xffe2c9b4, 0xffc7aa91, 0xff422c26, 0xff785c4e, 0xff997c69,
      0xff9a4b2e, 0xff6b6641, 0xff8d4666, 0xffb3261e},
     {0xff1d1512, 0xff251b17, 0xff2c211c, 0xff3c2d24, 0xff503c30, 0xff6c5040, 0xfff6e3cc, 0xffcdb49c, 0xffaa8e76,
      0xfff2b081, 0xffd0c18a, 0xffe9a6bd, 0xffffb4ab}},
    // one, from atom: one light and one dark
    {QStringLiteral("one"), QStringLiteral("One"),
     {0xffffffff, 0xffeaeaeb, 0xfffafafa, 0xffe5e5e6, 0xffdbdbdc, 0xffd4d4d4, 0xff383a42, 0xff696c77, 0xffa0a1a7,
      0xff4078f2, 0xffa626a4, 0xff50a14f, 0xffe45649},
     {0xff1b1d23, 0xff21252b, 0xff282c34, 0xff2c313a, 0xff3e4451, 0xff4b5263, 0xffabb2bf, 0xff9da5b4, 0xff5c6370,
      0xff61afef, 0xffc678dd, 0xff98c379, 0xffe06c75}},
};

qreal linear(qreal channel)
{
    return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

qreal relativeLuminance(const QColor &color)
{
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

} // namespace

const QList<Family> &families()
{
    return kFamilies;
}

const Family *find(const QString &id)
{
    for (const Family &family : kFamilies) {
        if (family.id == id) {
            return &family;
        }
    }
    return nullptr;
}

qreal contrast(const QColor &a, const QColor &b)
{
    const qreal one = relativeLuminance(a);
    const qreal two = relativeLuminance(b);
    return (std::max(one, two) + 0.05) / (std::min(one, two) + 0.05);
}

MaterialScheme::Roles roles(const Variant &variant, bool dark)
{
    using Role = MaterialScheme::Role;
    using color::mix;
    MaterialScheme::Roles roles;
    const auto set = [&roles](Role role, const QColor &value) { roles[static_cast<std::size_t>(role)] = value; };

    const QColor lowest = QColor::fromRgba(variant.lowest);
    const QColor window = QColor::fromRgba(variant.window);
    const QColor page = QColor::fromRgba(variant.page);
    const QColor raised = QColor::fromRgba(variant.raised);
    const QColor raisedMore = QColor::fromRgba(variant.raisedMore);
    const QColor text = QColor::fromRgba(variant.text);
    const QColor black(Qt::black);
    const QColor white(Qt::white);

    set(Role::Background, window);
    set(Role::OnBackground, text);
    set(Role::Surface, page);
    set(Role::SurfaceDim, dark ? lowest : mix(page, raisedMore, 0.4));
    set(Role::SurfaceBright, dark ? raisedMore : lowest);
    set(Role::SurfaceContainerLowest, lowest);
    set(Role::SurfaceContainerLow, page);
    set(Role::SurfaceContainer, mix(page, raised, 0.5));
    set(Role::SurfaceContainerHigh, raised);
    set(Role::SurfaceContainerHighest, raisedMore);
    set(Role::OnSurface, text);
    set(Role::SurfaceVariant, raised);
    set(Role::OnSurfaceVariant, QColor::fromRgba(variant.subtext));
    set(Role::InverseSurface, text);
    set(Role::InverseOnSurface, page);
    set(Role::Outline, QColor::fromRgba(variant.muted));
    set(Role::OutlineVariant, QColor::fromRgba(variant.line));
    set(Role::Shadow, black);
    set(Role::Scrim, black);
    set(Role::SurfaceTint, QColor::fromRgba(variant.primary));

    // an accent, its container (a wash of it over the page) and what goes on
    // each. on the accent: the theme's darkest or lightest, whichever reads
    // better, or plain black or white where neither reaches 4.5:1. on the
    // container: the accent drawn toward the text until it reads.
    const QColor darkInk = dark ? lowest : text;
    const QColor lightInk = dark ? text : lowest;
    const auto onto = [&](const QColor &ground) {
        const QColor themed = contrast(ground, darkInk) >= contrast(ground, lightInk) ? darkInk : lightInk;
        if (contrast(ground, themed) >= 4.5) {
            return themed;
        }
        return contrast(ground, black) >= contrast(ground, white) ? black : white;
    };
    const auto inkOn = [&](const QColor &tone, const QColor &ground) {
        for (qreal amount = 0.45; amount > 0.001; amount -= 0.05) {
            const QColor candidate = mix(tone, text, amount);
            if (contrast(candidate, ground) >= 4.5) {
                return candidate;
            }
        }
        return text;
    };
    const auto accent = [&](QRgb rgb, Role role, Role on, Role container, Role onContainer) {
        const QColor tone = QColor::fromRgba(rgb);
        const QColor wash = mix(tone, page, dark ? 0.3 : 0.22);
        set(role, tone);
        set(on, onto(tone));
        set(container, wash);
        set(onContainer, inkOn(tone, wash));
    };
    accent(variant.primary, Role::Primary, Role::OnPrimary, Role::PrimaryContainer, Role::OnPrimaryContainer);
    accent(variant.secondary, Role::Secondary, Role::OnSecondary, Role::SecondaryContainer, Role::OnSecondaryContainer);
    accent(variant.tertiary, Role::Tertiary, Role::OnTertiary, Role::TertiaryContainer, Role::OnTertiaryContainer);
    accent(variant.error, Role::Error, Role::OnError, Role::ErrorContainer, Role::OnErrorContainer);
    set(Role::InversePrimary, mix(QColor::fromRgba(variant.primary), dark ? black : white, 0.6));

    // the fixed roles are the same in light and dark: a pale and a deeper
    // tint of the accent, and dark ink for each
    const auto fixed = [&](QRgb rgb, Role pale, Role deeper, Role on, Role onVariant) {
        const QColor tone = QColor::fromRgba(rgb);
        set(pale, mix(tone, white, 0.3));
        set(deeper, mix(tone, white, 0.6));
        set(on, mix(tone, black, 0.2));
        set(onVariant, mix(tone, black, 0.5));
    };
    fixed(variant.primary, Role::PrimaryFixed, Role::PrimaryFixedDim, Role::OnPrimaryFixed, Role::OnPrimaryFixedVariant);
    fixed(variant.secondary, Role::SecondaryFixed, Role::SecondaryFixedDim, Role::OnSecondaryFixed,
          Role::OnSecondaryFixedVariant);
    fixed(variant.tertiary, Role::TertiaryFixed, Role::TertiaryFixedDim, Role::OnTertiaryFixed,
          Role::OnTertiaryFixedVariant);
    return roles;
}

} // namespace swiftify::themes
