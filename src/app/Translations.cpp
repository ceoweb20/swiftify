#include "app/Translations.h"
#include "app/ErrorCenter.h"
#include "app/Settings.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocale>

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
    connect(settings, &Settings::languageChanged, this, &Translations::apply);
    connect(settings, &Settings::languageAvailabilityChanged, this, &Translations::apply);
    apply();
}

Translations::~Translations()
{
    QCoreApplication::removeTranslator(this);
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
    if (m_russian) QCoreApplication::installTranslator(this);
    emit languageChanged();
}

QString Translations::translate(const char *, const char *sourceText, const char *, int n) const
{
    if (!m_russian || !sourceText) return {};
    const auto value = m_catalog.value(QString::fromUtf8(sourceText));
    if (value.isArray() && n >= 0) {
        const int form = n % 10 == 1 && n % 100 != 11 ? 0
            : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14) ? 1 : 2;
        return value.toArray().at(form).toString();
    }
    return value.isString() ? value.toString() : QString();
}

} // namespace swiftify
