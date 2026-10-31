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

#pragma once

#include "theme/MaterialScheme.h"

#include <QColor>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// a colour theme as the settings show it: its name and a few of its colours
/// in the current light or dark, for the swatch
struct ThemeOption {
    Q_GADGET
    QML_VALUE_TYPE(themeOption)
    Q_PROPERTY(QString id MEMBER id FINAL)
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QColor window MEMBER window FINAL)
    Q_PROPERTY(QColor page MEMBER page FINAL)
    Q_PROPERTY(QColor text MEMBER text FINAL)
    Q_PROPERTY(QColor primary MEMBER primary FINAL)
    Q_PROPERTY(QColor secondary MEMBER secondary FINAL)
    Q_PROPERTY(QColor tertiary MEMBER tertiary FINAL)

public:
    QString id;
    QString name;
    QColor window;
    QColor page;
    QColor text;
    QColor primary;
    QColor secondary;
    QColor tertiary;
};

/// the colour schemes people rice their desktops with, for when there is no
/// Matugen scheme to follow, or by choice. each comes as a light and
/// a dark variant from its own palette, and the app maps the palette onto
/// material's roles rather than reseeding it, so a theme keeps its own
/// grounds and accents.
namespace themes {

/// one variant of a theme, in the theme's own colours
struct Variant {
    /// the lowest container: darkest in a dark theme, lightest in a light one
    QRgb lowest;
    /// behind everything, around the page
    QRgb window;
    /// the page and the panels on it
    QRgb page;
    /// raised surfaces, then the most raised
    QRgb raised;
    QRgb raisedMore;
    /// dividers and quiet outlines
    QRgb line;
    QRgb text;
    QRgb subtext;
    /// outlines and placeholder text
    QRgb muted;
    QRgb primary;
    QRgb secondary;
    QRgb tertiary;
    QRgb error;
};

struct Family {
    QString id;
    QString name;
    Variant light;
    Variant dark;
};

const QList<Family> &families();
/// nullptr for "system", "builtin" or anything unknown
const Family *find(const QString &id);
MaterialScheme::Roles roles(const Variant &variant, bool dark);

/// wcag contrast between two colours, 1 to 21
qreal contrast(const QColor &a, const QColor &b);

} // namespace themes
} // namespace swiftify
