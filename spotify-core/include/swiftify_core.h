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

/*
 * the c interface to spotify-core, built with `--features capi`. it mirrors
 * the swift-bridge interface the apple apps use (src/swift_ffi.rs).
 *
 * strings are nul-terminated utf-8. a string returned from here is owned by the
 * caller and released with swiftify_string_free. functions returning an error
 * string return null on success.
 *
 * async calls return immediately. their result is queued as a completion and
 * the descriptor from swiftify_core_completion_fd becomes readable: drain it,
 * then call swiftify_core_take_completion until it returns false.
 */
#ifndef SWIFTIFY_CORE_H
#define SWIFTIFY_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SwiftifyCore SwiftifyCore;

SwiftifyCore *swiftify_core_new(void);
void swiftify_core_free(SwiftifyCore *core);
void swiftify_string_free(char *value);

/* Opaque password-derived key, kept in memory only; no OS credential store. */
typedef struct SwiftifyVault SwiftifyVault;
/* Open an encrypted vault (empty encoded means create). Derivation is expensive:
 * call from a worker thread. plaintext/error are owned strings, initially null. */
SwiftifyVault *swiftify_vault_open(const char *password, const char *encoded,
                                  char **plaintext, char **error);
char *swiftify_vault_seal(const SwiftifyVault *vault, const char *plaintext, char **error);
void swiftify_vault_free(SwiftifyVault *vault);
/* Wipe and free a decrypted string instead of using swiftify_string_free. */
void swiftify_secret_free(char *value);

int swiftify_core_completion_fd(const SwiftifyCore *core);
/* where there is no descriptor (windows: it is -1), callback(context) is
 * called from the finishing thread instead each time a completion is queued;
 * null takes it back. context lives until then. */
void swiftify_core_set_completion_callback(const SwiftifyCore *core, void (*callback)(void *),
                                           void *context);
bool swiftify_core_take_completion(const SwiftifyCore *core, uint64_t *request_id, bool *ok,
                                   char **payload);

/* async: each completes with `request_id`. lyrics and playlists complete
 * with json, as do both connects (the login to reconnect with), the rest
 * with an empty payload on success.
 *
 * a play call made while an earlier one is still running replaces it, as do
 * an output restart and a new connection: the earlier one then fails with
 * SWIFTIFY_CORE_SUPERSEDED, having started no sound. */
#define SWIFTIFY_CORE_SUPERSEDED "the request was replaced by a newer one"
/* a play call for a track spotify will not give this account fails with this */
#define SWIFTIFY_CORE_UNAVAILABLE "Spotify could not load this track."
/* a play call for an account spotify refuses the audio key for fails with
 * this: new accounts only play through spotify's own clients */
#define SWIFTIFY_CORE_AUDIO_KEY_REFUSED "Spotify refused the audio key for this account."

void swiftify_core_connect(const SwiftifyCore *core, uint64_t request_id, const char *access_token,
                           const char *client_id);
/* a login swiftify_core_connect (or this) completed with: librespot's reusable
 * credentials, which spend no oauth refresh token */
void swiftify_core_connect_stored(const SwiftifyCore *core, uint64_t request_id, const char *login,
                                  const char *client_id);
void swiftify_core_play_track(const SwiftifyCore *core, uint64_t request_id,
                              const char *spotify_uri);
/* prepares the next Spotify track on the standby player without starting it. */
char *swiftify_core_preload_track(const SwiftifyCore *core, const char *spotify_uri,
                                  int preload_standby);
/* starts the preloaded track and overlaps it with the current one for duration_ms. */
void swiftify_core_crossfade_track(const SwiftifyCore *core, uint64_t request_id,
                                   const char *spotify_uri, uint32_t duration_ms);
void swiftify_core_restart_audio_output(const SwiftifyCore *core, uint64_t request_id);
void swiftify_core_lyrics_json(const SwiftifyCore *core, uint64_t request_id,
                               const char *spotify_uri);
/* a playlist's songs read through the session, which still has them for a
 * playlist someone else made. completes with json shaped like the web api's
 * /v1/playlists/{id}/items page. */
void swiftify_core_playlist_json(const SwiftifyCore *core, uint64_t request_id,
                                 const char *playlist_uri);
/* an artist's top songs, read through the session, since the web api no
 * longer has them. completes with json shaped like the web api's
 * /v1/artists/{id}/top-tracks response. */
void swiftify_core_artist_top_tracks_json(const SwiftifyCore *core, uint64_t request_id,
                                          const char *artist_uri);
void swiftify_core_disconnect(const SwiftifyCore *core);
/* Personalized Spotify shelves and saved playlists, using the playback session. */
void swiftify_core_discovery_json(const SwiftifyCore *core, uint64_t request_id, const char *locale);

char *swiftify_core_play(const SwiftifyCore *core);
char *swiftify_core_pause(const SwiftifyCore *core);
char *swiftify_core_seek(const SwiftifyCore *core, uint32_t position_ms);
char *swiftify_core_set_volume(const SwiftifyCore *core, float volume);
char *swiftify_core_set_stream_quality(const SwiftifyCore *core, uint32_t kbps);
char *swiftify_core_set_volume_normalization(const SwiftifyCore *core, bool enabled);
/* keeps streamed songs in path (empty: none), up to limit_bytes; applies the
 * next time playback connects */
char *swiftify_core_set_audio_cache(const SwiftifyCore *core, const char *path,
                                    uint64_t limit_bytes);
/* whether the app is to show as a spotify connect device called name and
 * report what it plays, which anything watching the account (a scrobbling bot)
 * sees; it cannot be controlled from elsewhere. applies the next time playback
 * connects. returns null, or an error to free. */
char *swiftify_core_set_connect(const SwiftifyCore *core, bool enabled, const char *name);
char *swiftify_core_set_playback_shaping(const SwiftifyCore *core, float tempo,
                                         float pitch_semitones, bool preserves_pitch);
char *swiftify_core_set_preamp_gain(const SwiftifyCore *core, float gain_db);
char *swiftify_core_set_equalizer(const SwiftifyCore *core, const float *gains_db, size_t count);

bool swiftify_core_session_is_valid(const SwiftifyCore *core);
bool swiftify_core_audio_output_stalled(const SwiftifyCore *core);
uint32_t swiftify_core_playback_position_ms(const SwiftifyCore *core);
uint32_t swiftify_core_playback_duration_ms(const SwiftifyCore *core);
bool swiftify_core_take_end_of_track(const SwiftifyCore *core);
float swiftify_core_volume(const SwiftifyCore *core);
/* the rate the output for files and live audio runs at (opening it); 0 if none */
uint32_t swiftify_core_output_rate(const SwiftifyCore *core);

void swiftify_core_set_spectrum_enabled(const SwiftifyCore *core, bool enabled);
/* 48 bands in 0...1. allocates nothing; returns how many were written. */
size_t swiftify_core_spectrum_levels(const SwiftifyCore *core, float *out, size_t capacity);

/* the equalizer's centre frequencies in hz; returns how many were written. */
size_t swiftify_equalizer_frequencies(float *out, size_t capacity);

/* audio another program plays, handed over as it plays: interleaved f32 at
 * the rate and channels it was made for. it outlives any core it plays
 * through. push may be called from any thread. */
typedef struct SwiftifyLiveBuffer SwiftifyLiveBuffer;
SwiftifyLiveBuffer *swiftify_live_buffer_new(uint32_t rate, uint32_t channels);
void swiftify_live_buffer_free(SwiftifyLiveBuffer *buffer);
void swiftify_live_buffer_push(const SwiftifyLiveBuffer *buffer, const float *samples, size_t count);
/* plays the buffer through the equalizer and the spectrum in place of
 * whatever played; one already playing carries on */
char *swiftify_core_play_live(const SwiftifyCore *core, const SwiftifyLiveBuffer *buffer);
void swiftify_core_stop_live(const SwiftifyCore *core);
/* hands interleaved stereo audio, heard at output_gain of its full level, to
 * the spectrum alone, for audio another program plays to the speakers
 * itself. may be called from any thread. */
void swiftify_core_analyse_live(const SwiftifyCore *core, const float *samples, size_t count,
                                float output_gain);

#ifdef __cplusplus
}
#endif

#endif
