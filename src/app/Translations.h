#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringView>
#include <QTranslator>

namespace swiftify {

class Settings;

///set to false to disable russian translations. this skips tests and disables the option inside of the settings
inline constexpr bool kRussianTranslationEnabled = false;

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
    Settings *m_settings;
    QJsonObject m_catalog;
    bool m_russian = false;
};

} // namespace swiftify
