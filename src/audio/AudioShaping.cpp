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

#include "audio/AudioShaping.h"

#include "app/Settings.h"
#include "core/RustCore.h"
#include "spotify/SpotifyError.h"
#include "spotify/SpotifySession.h"

#include <QCollator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>

#include <algorithm>
#include <cmath>

namespace swiftify {

namespace {

AudioShaping *s_instance = nullptr;

const QString kGainsKey = QStringLiteral("audioEqualizerGains");
const QString kPreampKey = QStringLiteral("audioEqualizerPreamp");
const QString kEnabledKey = QStringLiteral("audioEqualizerEnabled");
const QString kPresetsKey = QStringLiteral("audioEqualizerPresets");
const QString kTempoKey = QStringLiteral("audioPlaybackTempo");
const QString kPitchKey = QStringLiteral("audioPlaybackPitch");
const QString kPreservesPitchKey = QStringLiteral("audioPreservesPitch");

QVariantList toVariantList(const QList<double> &values)
{
    QVariantList list;
    for (double value : values) {
        list.append(value);
    }
    return list;
}

} // namespace

bool EqualizerPreset::matches(const QList<double> &other) const
{
    // close enough to count as the same curve. half a decibel is the step the
    // faders move in.
    if (other.size() != gains.size()) {
        return false;
    }
    for (qsizetype band = 0; band < gains.size(); ++band) {
        if (std::abs(gains.at(band) - other.at(band)) >= 0.25) {
            return false;
        }
    }
    return true;
}

const QList<EqualizerPreset> &EqualizerPreset::builtIn()
{
    // presets use the same names as spotify
    static const QList<EqualizerPreset> presets {
        {QStringLiteral("Flat"), {0, 0, 0, 0, 0, 0}},
        {QStringLiteral("Bass Booster"), {7, 5.5, 2.5, 0, 0, 0}},
        {QStringLiteral("Bass Reducer"), {-7, -5.5, -2.5, 0, 0, 0}},
        {QStringLiteral("Treble Booster"), {0, 0, 0, 2.5, 5, 7}},
        {QStringLiteral("Treble Reducer"), {0, 0, 0, -2.5, -5, -7}},
        {QStringLiteral("Vocal Booster"), {-2.5, -3, 2.5, 5, 3.5, -1.5}},
        {QStringLiteral("Acoustic"), {4.5, 3.5, 1.5, 2, 3, 3.5}},
        {QStringLiteral("Classical"), {4.5, 3.5, 2, 0, -1.5, 3}},
        {QStringLiteral("Dance"), {6, 4.5, 1, -1, 2, 3.5}},
        {QStringLiteral("Deep"), {6, 4, 1.5, 1, -2, -4.5}},
        {QStringLiteral("Electronic"), {5, 3.5, -0.5, 1, 2, 5}},
        {QStringLiteral("Hip-Hop"), {6, 4.5, 1.5, 1.5, -1, 2}},
        {QStringLiteral("Jazz"), {4, 3, 1, 2, 2, 3}},
        {QStringLiteral("Latin"), {5, 2.5, 0, 0, 2.5, 5}},
        {QStringLiteral("Loudness"), {6.5, 4, 0, 1, -2.5, 5}},
        {QStringLiteral("Lounge"), {-3, 0.5, 2.5, 3, 1, -1.5}},
        {QStringLiteral("Piano"), {3, 1.5, 0, 2, 3, 3}},
        {QStringLiteral("Pop"), {-1.5, 1, 3.5, 3.5, 1, -1.5}},
        {QStringLiteral("R&B"), {5, 5.5, 2, -1.5, 2, 3.5}},
        {QStringLiteral("Rock"), {5, 3.5, 1, -1, 2.5, 4.5}},
        {QStringLiteral("Small Speakers"), {5.5, 4, 2, 0, -1, -3}},
        {QStringLiteral("Spoken Word"), {-3, -1, 3, 4.5, 3, 0}},
    };
    return presets;
}

AudioShaping::AudioShaping(PlaybackCore *core, Settings *settings, SpotifySession *session, QObject *parent)
    : QObject(parent)
    , m_core(core)
    , m_settings(settings)
{
    if (!s_instance) {
        s_instance = this;
    }
    for (float frequency : RustCore::equalizerFrequencies()) {
        m_frequencies.append(frequency);
    }

    const QVariantList stored = m_settings->value(kGainsKey).toList();
    m_gains = QList<double>(m_frequencies.size(), 0.0);
    if (stored.size() == m_frequencies.size()) {
        for (qsizetype band = 0; band < stored.size(); ++band) {
            m_gains[band] = std::clamp(stored.at(band).toDouble(), -kMaximumGain, kMaximumGain);
        }
    }
    m_preampGain = std::clamp(m_settings->value(kPreampKey, 0.0).toDouble(), -kMaximumGain, kMaximumGain);
    m_equalizerEnabled = m_settings->value(kEnabledKey, true).toBool();
    m_tempo = m_settings->value(kTempoKey, 1.0).toDouble();
    m_pitch = m_settings->value(kPitchKey, 0.0).toDouble();
    m_preservesPitch = m_settings->value(kPreservesPitchKey, true).toBool();

    for (const QJsonValue &value : QJsonDocument::fromJson(m_settings->value(kPresetsKey).toByteArray()).array()) {
        const QJsonObject object = value.toObject();
        EqualizerPreset preset {object.value(QStringLiteral("name")).toString(), {}};
        for (const QJsonValue &gain : object.value(QStringLiteral("gains")).toArray()) {
            preset.gains.append(gain.toDouble());
        }
        // a preset from a build with a different band count is ignored rather
        // than stretched across the wrong frequencies.
        if (!preset.name.isEmpty() && preset.gains.size() == m_frequencies.size()) {
            m_savedPresets.append(preset);
        }
    }

    apply();
    // a logout replaces the core, and a new one starts flat.
    connect(session, &SpotifySession::loggedOut, this, &AudioShaping::apply);
}

AudioShaping::~AudioShaping()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

AudioShaping *AudioShaping::instance()
{
    return s_instance;
}

AudioShaping *AudioShaping::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QStringList AudioShaping::bandLabels() const
{
    QStringList labels;
    for (qreal frequency : m_frequencies) {
        labels.append(frequency >= 1000 ? QStringLiteral("%1k").arg(qRound(frequency / 1000)) : QString::number(qRound(frequency)));
    }
    return labels;
}

QList<EqualizerPreset> AudioShaping::allPresets() const
{
    QList<EqualizerPreset> presets;
    for (const EqualizerPreset &preset : EqualizerPreset::builtIn()) {
        if (preset.gains.size() == m_frequencies.size()) {
            presets.append(preset);
        }
    }
    return presets + m_savedPresets;
}

QStringList AudioShaping::presetNames() const
{
    QStringList names;
    for (const EqualizerPreset &preset : allPresets()) {
        names.append(preset.name);
    }
    return names;
}

QString AudioShaping::activePresetName() const
{
    for (const EqualizerPreset &preset : allPresets()) {
        if (preset.matches(m_gains)) {
            return preset.name;
        }
    }
    return {};
}

bool AudioShaping::isFlat() const
{
    return std::all_of(m_gains.begin(), m_gains.end(), [](double gain) { return std::abs(gain) < 0.05; });
}

bool AudioShaping::isNeutral() const
{
    return isFlat() && std::abs(m_preampGain) < 0.01 && std::abs(m_tempo - 1) < 0.001 && std::abs(m_pitch) < 0.001;
}

void AudioShaping::setGains(const QList<double> &gains)
{
    if (gains == m_gains) {
        return;
    }
    m_gains = gains;
    m_settings->setValue(kGainsKey, toVariantList(m_gains));
    applyEqualizer();
    emit gainsChanged();
    emit neutralChanged();
}

void AudioShaping::setGain(int band, double gain)
{
    if (band < 0 || band >= m_gains.size() || !std::isfinite(gain)) {
        return;
    }
    QList<double> gains = m_gains;
    gains[band] = std::clamp(gain, -kMaximumGain, kMaximumGain);
    setGains(gains);
}

void AudioShaping::setPreampGain(double gain)
{
    gain = std::clamp(gain, -kMaximumGain, kMaximumGain);
    if (!std::isfinite(gain) || std::abs(gain - m_preampGain) < 0.001) {
        return;
    }
    m_preampGain = gain;
    m_settings->setValue(kPreampKey, m_preampGain);
    applyPreamp();
    emit preampGainChanged();
    emit neutralChanged();
}

void AudioShaping::setEqualizerEnabled(bool enabled)
{
    if (m_equalizerEnabled == enabled) {
        return;
    }
    m_equalizerEnabled = enabled;
    m_settings->setValue(kEnabledKey, enabled);
    applyEqualizer();
    emit equalizerEnabledChanged();
}

void AudioShaping::setTempo(double tempo)
{
    tempo = std::clamp(tempo, 0.5, 2.0);
    if (tempo == m_tempo) {
        return;
    }
    m_tempo = tempo;
    m_settings->setValue(kTempoKey, tempo);
    applyShaping();
    emit shapingChanged();
    emit neutralChanged();
}

void AudioShaping::setPitch(double pitch)
{
    pitch = std::clamp(pitch, -12.0, 12.0);
    if (pitch == m_pitch) {
        return;
    }
    m_pitch = pitch;
    m_settings->setValue(kPitchKey, pitch);
    applyShaping();
    emit shapingChanged();
    emit neutralChanged();
}

void AudioShaping::setPreservesPitch(bool preserves)
{
    if (m_preservesPitch == preserves) {
        return;
    }
    m_preservesPitch = preserves;
    m_settings->setValue(kPreservesPitchKey, preserves);
    applyShaping();
    emit shapingChanged();
}

void AudioShaping::applyPreset(const QString &name)
{
    for (const EqualizerPreset &preset : allPresets()) {
        if (preset.name == name) {
            setGains(preset.gains);
            return;
        }
    }
}

void AudioShaping::savePreset(const QString &rawName)
{
    const QString name = rawName.trimmed();
    if (name.isEmpty()) {
        return;
    }
    m_savedPresets.removeIf([&](const EqualizerPreset &preset) { return preset.name.compare(name, Qt::CaseInsensitive) == 0; });
    m_savedPresets.append({name, m_gains});
    QCollator collator;
    std::sort(m_savedPresets.begin(), m_savedPresets.end(),
              [&](const EqualizerPreset &a, const EqualizerPreset &b) { return collator.compare(a.name, b.name) < 0; });
    storePresets();
    emit presetsChanged();
    emit gainsChanged();
}

void AudioShaping::deletePreset(const QString &name)
{
    m_savedPresets.removeIf([&](const EqualizerPreset &preset) { return preset.name == name; });
    storePresets();
    emit presetsChanged();
    emit gainsChanged();
}

bool AudioShaping::isSavedPreset(const QString &name) const
{
    return std::any_of(m_savedPresets.begin(), m_savedPresets.end(),
                       [&](const EqualizerPreset &preset) { return preset.name == name; });
}

void AudioShaping::storePresets()
{
    QJsonArray array;
    for (const EqualizerPreset &preset : m_savedPresets) {
        QJsonArray gains;
        for (double gain : preset.gains) {
            gains.append(gain);
        }
        array.append(QJsonObject {{QStringLiteral("name"), preset.name}, {QStringLiteral("gains"), gains}});
    }
    m_settings->setValue(kPresetsKey, QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void AudioShaping::reset()
{
    setPreampGain(0.0);
    setGains(QList<double>(m_frequencies.size(), 0.0));
    setTempo(1);
    setPitch(0);
}

void AudioShaping::apply()
{
    applyPreamp();
    applyEqualizer();
    applyShaping();
}

void AudioShaping::applyPreamp()
{
    try {
        m_core->setPreampGain(float(m_preampGain));
    } catch (const SpotifyError &) {
    }
}

void AudioShaping::applyEqualizer()
{
    // bypassed rather than zeroed: the faders keep their positions, and the
    // core is told there is nothing to do.
    QList<float> gains;
    for (double gain : m_gains) {
        gains.append(m_equalizerEnabled ? float(gain) : 0.0f);
    }
    try {
        m_core->setEqualizer(gains);
    } catch (const SpotifyError &) {
    }
}

void AudioShaping::applyShaping()
{
    // pitch is not an independent control while speed is carrying it.
    try {
        m_core->setPlaybackShaping(float(m_tempo), m_preservesPitch ? float(m_pitch) : 0.0f, m_preservesPitch);
    } catch (const SpotifyError &) {
    }
}

} // namespace swiftify
