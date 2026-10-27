#pragma once

#include <QList>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class PlaybackCore;
class Settings;
class SpotifySession;

/// a named equalizer curve, built in or saved (EqualizerPreset.swift)
struct EqualizerPreset {
    QString name;
    QList<double> gains;

    bool matches(const QList<double> &other) const;
    static const QList<EqualizerPreset> &builtIn();
};

/// the equalizer, and tempo and pitch (AudioShaping.swift). the panel binds
/// here; the core is only ever handed the result.
class AudioShaping : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Sound)
    QML_SINGLETON

    Q_PROPERTY(QList<qreal> frequencies READ frequencies CONSTANT FINAL)
    Q_PROPERTY(QStringList bandLabels READ bandLabels CONSTANT FINAL)
    Q_PROPERTY(QList<qreal> gains READ gains NOTIFY gainsChanged FINAL)
    Q_PROPERTY(double preampGain READ preampGain WRITE setPreampGain NOTIFY preampGainChanged FINAL)
    Q_PROPERTY(bool equalizerEnabled READ isEqualizerEnabled WRITE setEqualizerEnabled NOTIFY equalizerEnabledChanged FINAL)
    Q_PROPERTY(double tempo READ tempo WRITE setTempo NOTIFY shapingChanged FINAL)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY shapingChanged FINAL)
    Q_PROPERTY(bool preservesPitch READ preservesPitch WRITE setPreservesPitch NOTIFY shapingChanged FINAL)
    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY presetsChanged FINAL)
    Q_PROPERTY(QString activePresetName READ activePresetName NOTIFY gainsChanged FINAL)
    Q_PROPERTY(bool flat READ isFlat NOTIFY gainsChanged FINAL)
    Q_PROPERTY(bool neutral READ isNeutral NOTIFY neutralChanged FINAL)

public:
    static constexpr double kMaximumGain = 12.0;

    AudioShaping(PlaybackCore *core, Settings *settings, SpotifySession *session, QObject *parent = nullptr);
    ~AudioShaping() override;

    static AudioShaping *instance();
    static AudioShaping *create(QQmlEngine *, QJSEngine *);

    QList<qreal> frequencies() const { return m_frequencies; }
    QStringList bandLabels() const;
    QList<qreal> gains() const { return m_gains; }
    double preampGain() const { return m_preampGain; }
    void setPreampGain(double gain);
    bool isEqualizerEnabled() const { return m_equalizerEnabled; }
    void setEqualizerEnabled(bool enabled);
    double tempo() const { return m_tempo; }
    void setTempo(double tempo);
    double pitch() const { return m_pitch; }
    void setPitch(double pitch);
    bool preservesPitch() const { return m_preservesPitch; }
    void setPreservesPitch(bool preserves);
    QStringList presetNames() const;
    QString activePresetName() const;
    bool isFlat() const;
    bool isNeutral() const;

    Q_INVOKABLE void setGain(int band, double gain);
    Q_INVOKABLE void applyPreset(const QString &name);
    /// saving over a name replaces it, the way saving a file does
    Q_INVOKABLE void savePreset(const QString &name);
    Q_INVOKABLE void deletePreset(const QString &name);
    /// built-ins cannot be deleted
    Q_INVOKABLE bool isSavedPreset(const QString &name) const;
    Q_INVOKABLE void reset();
    /// hands everything to the core again, for a fresh one
    void apply();

signals:
    void gainsChanged();
    void preampGainChanged();
    void equalizerEnabledChanged();
    void shapingChanged();
    void presetsChanged();
    void neutralChanged();

private:
    void setGains(const QList<double> &gains);
    void applyPreamp();
    void applyEqualizer();
    void applyShaping();
    void storePresets();
    QList<EqualizerPreset> allPresets() const;

    PlaybackCore *m_core;
    Settings *m_settings;
    QList<qreal> m_frequencies;
    QList<double> m_gains;
    double m_preampGain = 0.0;
    bool m_equalizerEnabled = true;
    double m_tempo = 1;
    double m_pitch = 0;
    bool m_preservesPitch = true;
    QList<EqualizerPreset> m_savedPresets;
};

} // namespace swiftify
