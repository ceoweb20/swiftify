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

// stats.fm replies for tst_stats, kept out of its .cpp: moc cannot read
// raw string literals

inline constexpr const char kTrackJson[] = R"({
            "position": 1, "streams": 42, "playedMs": 7560000,
            "track": {
                "name": "Song", "durationMs": 180000,
                "externalIds": {"spotify": ["4uLU6hMCjMI75M1A2tKUQC"]},
                "albums": [{"name": "Record", "image": "https://i.scdn.co/image/abc",
                            "externalIds": {"spotify": ["alb1"]}}],
                "artists": [{"name": "Singer", "externalIds": {"spotify": ["art1"]}},
                            {"name": "Guest", "externalIds": {"spotify": ["art2"]}}]
            }
        })";

inline constexpr const char kEstimatedTrackJson[] = R"({
            "streams": 3, "track": {"name": "S", "durationMs": 1000, "externalIds": {"spotify": ["t"]}}
        })";

inline constexpr const char kArtistJson[] = R"({
            "streams": 9, "artist": {"name": "Singer", "image": "https://x/y.jpg", "genres": ["indie", "pop", "rock"],
                                     "externalIds": {"spotify": ["art1"]}}
        })";

inline constexpr const char kGenreJson[] = R"({"streams": 5, "genre": {"tag": "shoegaze"}})";

inline constexpr const char kDatesJson[] = R"({
            "hours": {"0": {"count": 2}, "21": {"count": 9}, "30": {"count": 1}},
            "weekDays": {"1": {"count": 4}, "7": {"count": 6}}
        })";

inline constexpr const char kDaysJson[] = R"({"days": {"2026-10-16": {"count": 3}, "2026-10-17": {"count": 0}}})";

// a free account's top song, as stats.fm sends it: the count left null, and
// the artists and album with stats.fm's own ids, not spotify's
inline constexpr const char kFreeTrackJson[] = R"({
    "position": 1, "streams": null, "indicator": "UP",
    "track": {
        "id": 42997036, "name": "when sunday comes around", "durationMs": 319333,
        "externalIds": {"spotify": ["6TxR6ScvxDzMZBj9UgxYx4"], "appleMusic": ["1869227640"]},
        "albums": [{"id": 7756776, "image": "https://i.scdn.co/image/ab67", "name": "Monterey Village"}],
        "artists": [{"id": 966483, "name": "Marlon Funaki", "image": "https://i.scdn.co/image/ab6761"}]
    }
})";

inline constexpr const char kFreeArtistJson[] = R"({
    "position": 1, "streams": null,
    "artist": {"id": 19967, "name": "Tame Impala", "genres": ["neo-psychedelic", "indie"],
               "externalIds": {"spotify": ["5INjqkS1o8h1imAzPqGZBb"]}}
})";

// what /streams/tracks/<id>/stats answers, plus or not
inline constexpr const char kItemStatsJson[] = R"({
    "durationMs": 4112663, "count": 13,
    "cardinality": {"tracks": 1, "artists": 1, "albums": 1}
})";
