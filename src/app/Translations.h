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
