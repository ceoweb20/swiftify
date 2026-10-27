#pragma once

#include "theme/MaterialScheme.h"

#include <QCache>
#include <QColor>
#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantAnimation>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class ArtworkCache;
class PlaybackController;
class Theme;
struct WeightedPalette;

/// the playing cover's colours: its palette for the visualizers and the
/// backdrop, and material schemes seeded from it for the player surfaces.
/// changes cross-fade, and the old colours stay until the new ones are ready.
/// the palette and the schemes are worked out off the gui thread and kept
/// per cover: seeding the two schemes took up to a tenth of a second, and on
/// the gui thread every song change dropped frames.
class PlayerStyle : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QList<QColor> palette READ palette NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QList<qreal> weights READ weights NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor primary READ primary NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY paletteChanged FINAL)
    /// follows the app's light or dark
    Q_PROPERTY(swiftify::MaterialScheme *scheme READ scheme CONSTANT FINAL)
    /// always dark, for the immersive player
    Q_PROPERTY(swiftify::MaterialScheme *darkScheme READ darkScheme CONSTANT FINAL)

public:
    PlayerStyle(PlaybackController *player, Theme *theme, ArtworkCache *artwork, QObject *parent = nullptr);
    ~PlayerStyle() override;

    static PlayerStyle *instance();
    static PlayerStyle *create(QQmlEngine *, QJSEngine *);

    QList<QColor> palette() const { return m_palette; }
    QList<qreal> weights() const { return m_weights; }
    QColor primary() const { return m_palette.value(0); }
    QColor secondary() const { return m_palette.value(1); }
    MaterialScheme *scheme() const { return m_scheme; }
    MaterialScheme *darkScheme() const { return m_darkScheme; }

signals:
    void paletteChanged();

private:
    struct Target {
        QList<QColor> palette;
        QList<qreal> weights;
        MaterialScheme::Roles light;
        MaterialScheme::Roles dark;
    };

    void artworkChanged();
    /// the selected theme's colours, for a song with no cover, one that will
    /// not load, or nothing playing
    void adoptFallback();
    Target themeTarget() const;
    void retarget(Target target);
    void step(qreal progress);
    /// pure, so it may run on any thread
    static Target targetFor(const QList<QColor> &palette, const QList<qreal> &weights);
    static Target targetFor(const WeightedPalette &palette);

    PlaybackController *m_player;
    Theme *m_theme;
    ArtworkCache *m_artwork;
    MaterialScheme *m_scheme;
    MaterialScheme *m_darkScheme;
    QUrl m_artworkUrl;
    /// Each cover's palette and both scheme variants, independent of app mode.
    QCache<QUrl, Target> m_targets {128};
    Target m_fallback;
    /// showing the theme's colours rather than a cover's
    bool m_onFallback = true;

    QList<QColor> m_palette;
    QList<qreal> m_weights;
    Target m_from;
    Target m_to;
    QVariantAnimation m_transition;
};

} // namespace swiftify
