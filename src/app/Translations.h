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

#include <QJsonObject>
#include <QList>
#include <QLocale>
#include <QRegularExpression>
#include <QString>
#include <QStringView>
#include <QTranslator>

namespace swiftify {

class Settings;

///set to false to disable russian translations. this skips tests and disables the option inside of the settings
inline constexpr bool kRussianTranslationEnabled = true;

/// Uses Qt's normal qsTr/tr bindings; the small UTF-8 catalog is embedded so
/// building translations doesn't require an additional Linguist installation.
class Translations : public QTranslator {
    Q_OBJECT
public:
    explicit Translations(Settings *settings, QObject *parent = nullptr);
    ~Translations() override;
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;
    bool isEmpty() const override { return m_catalog.isEmpty(); }

    static bool isLanguageEnabled(QStringView language);
    static void setLanguageEnabled(QStringView language, bool enabled);
    static void resetLanguageEnabled();
    static QString disabledLanguageMessage();
    static QString disabledLanguageErrorMessage();

signals:
    void languageChanged();

private:
    void apply();
    QString translatedMessage(const QString &source, int depth = 0) const;
    struct MessagePattern {
        QRegularExpression expression;
        QList<int> arguments;
        QString translation;
    };
    Settings *m_settings;
    QJsonObject m_catalog;
    QList<MessagePattern> m_patterns;
    bool m_russian = false;
    QLocale m_previousLocale;
};

} // namespace swiftify
