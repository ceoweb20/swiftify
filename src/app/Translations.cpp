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

#include "app/Translations.h"
#include "app/ErrorCenter.h"
#include "app/Settings.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocale>

#include <algorithm>

namespace swiftify {

namespace {
bool s_russianEnabled = kRussianTranslationEnabled;
} // namespace

bool Translations::isLanguageEnabled(QStringView language)
{
    if (language == QLatin1StringView("ru")) {
        return s_russianEnabled;
    }
    return true;
}

void Translations::setLanguageEnabled(QStringView language, bool enabled)
{
    if (language == QLatin1StringView("ru")) {
        if (s_russianEnabled == enabled) {
            return;
        }
        s_russianEnabled = enabled;
        if (auto *settings = Settings::instance()) {
            settings->enforceEnabledLanguage();
        }
    }
}

void Translations::resetLanguageEnabled()
{
    setLanguageEnabled(QStringLiteral("ru"), kRussianTranslationEnabled);
}

QString Translations::disabledLanguageMessage()
{
    return QStringLiteral("this language is currently disabled due to missing language maintainers");
}

QString Translations::disabledLanguageErrorMessage()
{
    return QStringLiteral("this language is currently disabled due to missing language maintainers. your app language has been switched back to english.");
}

Translations::Translations(Settings *settings, QObject *parent)
    : QTranslator(parent), m_settings(settings)
{
    QFile file(QStringLiteral(":/translations/ru.json"));
    if (file.open(QIODevice::ReadOnly)) {
        m_catalog = QJsonDocument::fromJson(file.readAll()).object();
    }
    // Status/error strings can already contain paths, codes or other details
    // when QML receives them. Match catalog templates without translating metadata.
    const QRegularExpression placeholder(QStringLiteral("%([1-9]\\d*)"));
    for (auto it = m_catalog.begin(); it != m_catalog.end(); ++it) {
        if (!it.value().isString() || !it.key().contains(placeholder)) continue;
        QString pattern = QStringLiteral("\\A");
        qsizetype position = 0;
        QList<int> arguments;
        auto matches = placeholder.globalMatch(it.key());
        while (matches.hasNext()) {
            const auto match = matches.next();
            pattern += QRegularExpression::escape(it.key().mid(position, match.capturedStart() - position));
            pattern += QStringLiteral("(.*?)");
            arguments.append(match.captured(1).toInt());
            position = match.capturedEnd();
        }
        pattern += QRegularExpression::escape(it.key().mid(position)) + QStringLiteral("\\z");
        m_patterns.append({QRegularExpression(pattern, QRegularExpression::DotMatchesEverythingOption),
                           arguments, it.value().toString()});
    }
    std::sort(m_patterns.begin(), m_patterns.end(), [](const auto &a, const auto &b) {
        return a.expression.pattern().size() > b.expression.pattern().size();
    });
    connect(settings, &Settings::languageChanged, this, &Translations::apply);
    connect(settings, &Settings::languageAvailabilityChanged, this, &Translations::apply);
    apply();
}

Translations::~Translations()
{
    QCoreApplication::removeTranslator(this);
    QLocale::setDefault(m_previousLocale);
}

void Translations::apply()
{
    QCoreApplication::removeTranslator(this);
    if (!isLanguageEnabled(m_settings->language())) {
        m_settings->enforceEnabledLanguage();
    }
    if (m_settings->takeRevertedDisabledLanguage()) {
        if (auto *errors = ErrorCenter::instance()) {
            errors->show(disabledLanguageErrorMessage());
        }
    }
    const QString language = m_settings->language();
    m_russian = isLanguageEnabled(QStringLiteral("ru"))
        && (language == QLatin1StringView("ru")
            || (language == QLatin1StringView("system") && QLocale::system().language() == QLocale::Russian));
    QLocale::setDefault(m_russian ? QLocale(QLocale::Russian, QLocale::Russia)
                                 : language == QLatin1StringView("en") ? QLocale(QLocale::English, QLocale::UnitedStates)
                                                                      : QLocale::system());
    if (m_russian) QCoreApplication::installTranslator(this);
    emit languageChanged();
}

QString Translations::translatedMessage(const QString &source, int depth) const
{
    const auto value = m_catalog.value(source);
    if (value.isString()) return value.toString();
    if (depth >= 4) return {};
    const QRegularExpression placeholder(QStringLiteral("%([1-9]\\d*)"));
    for (const auto &pattern : m_patterns) {
        const auto match = pattern.expression.match(source);
        if (!match.hasMatch()) continue;
        QString result;
        qsizetype position = 0;
        auto placeholders = placeholder.globalMatch(pattern.translation);
        while (placeholders.hasNext()) {
            const auto token = placeholders.next();
            result += pattern.translation.mid(position, token.capturedStart() - position);
            const int index = pattern.arguments.indexOf(token.captured(1).toInt());
            const QString argument = match.captured(index + 1);
            const QString translated = translatedMessage(argument, depth + 1);
            result += translated.isEmpty() ? argument : translated;
            position = token.capturedEnd();
        }
        result += pattern.translation.mid(position);
        return result;
    }
    return {};
}

QString Translations::translate(const char *context, const char *sourceText, const char *, int n) const
{
    if (!m_russian || !sourceText) return {};
    const auto value = m_catalog.value(QString::fromUtf8(sourceText));
    if (value.isArray() && n >= 0) {
        const int form = n % 10 == 1 && n % 100 != 11 ? 0
            : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14) ? 1 : 2;
        return value.toArray().at(form).toString();
    }
    if (value.isString()) return value.toString();
    const QByteArray scope(context ? context : "");
    if (scope == "Errors" || scope == "Soloist" || scope == "SpotifySession" || scope == "Stats" || scope == "Discovery" || scope == "DJMix") {
        return translatedMessage(QString::fromUtf8(sourceText));
    }
    return {};
}

} // namespace swiftify
