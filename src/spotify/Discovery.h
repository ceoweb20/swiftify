#pragma once

#include "spotify/SpotifyTypes.h"
#include <QVariantList>

namespace swiftify::discovery {

struct Snapshot {
    QList<Playlist> saved;
    QList<Playlist> personalized;
    QVariantList sections;
    QString userId;
    bool personalizedAvailable = false;
    bool libraryAvailable = false;
    bool partial = false;
};

Snapshot parse(const QString &payload);

} // namespace swiftify::discovery
