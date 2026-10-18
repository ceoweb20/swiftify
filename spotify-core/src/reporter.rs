//! tells spotify what this app is playing, as a spotify connect device does,
//! so that anything watching the account (a scrobbling bot, other spotify
//! clients) sees the song, its position and whether it is paused.
//!
//! it only reports. the app's own players do the playing, which is why crossfade
//! keeps working; spotify's other clients cannot control it, and a command sent
//! to it is answered with a failure. the registration follows what librespot's
//! `Spirc` does, with the player left out.

use std::{
    collections::HashMap,
    hash::{Hash, Hasher},
    sync::Arc,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use futures_util::StreamExt;
use librespot::{
    core::{
        Error, Session,
        config::DeviceType,
        dealer::{
            manager::{BoxedStream, BoxedStreamResult, Reply, RequestReply},
            protocol::Message,
        },
        version,
    },
    protocol::{
        connect::{Capabilities, Device, DeviceInfo, MemberType, PutStateReason, PutStateRequest},
        media::AudioQuality,
        player::{ContextIndex, ContextPlayerOptions, PlayOrigin, PlayerState, ProvidedTrack, Suppressions},
    },
};
use protobuf::{EnumOrUnknown, MessageField};
use tokio::sync::watch;

/// the context a song played from a list of songs belongs to, as spotify's
/// own clients name it
const CONTEXT: &str = "spotify:web-api";
/// updates within this long of each other go out as one
const COALESCE: Duration = Duration::from_millis(150);

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
enum Status {
    /// nothing has played yet, or the device was just registered
    #[default]
    Idle,
    Playing,
    Paused,
}

/// what the account is to be told
#[derive(Clone, Debug, Default)]
struct Desired {
    uri: Option<String>,
    status: Status,
    position_ms: u32,
    duration_ms: u32,
    /// the wall clock (ms, adjusted to spotify's) when `position_ms` was true
    stamp_ms: i64,
    volume: u16,
}

pub(crate) struct Reporter {
    desired: watch::Sender<Desired>,
    session: Session,
}

/// the dealer's listeners, which have to exist before the session connects
pub(crate) struct Prepared {
    connection_ids: BoxedStreamResult<String>,
    commands: BoxedStream<RequestReply>,
}

fn now_ms(session: &Session) -> i64 {
    let wall = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|elapsed| elapsed.as_millis() as i64)
        .unwrap_or(0);
    wall + 1000 * session.time_delta()
}

impl Prepared {
    /// registers what the dealer is to hand over, then connects nothing: the
    /// caller connects the session next, and `launch`es once it has
    pub(crate) async fn new(session: &Session) -> Result<Self, Error> {
        fn connection_id(message: Message) -> Result<String, Error> {
            message
                .headers
                .get("Spotify-Connection-Id")
                .cloned()
                .ok_or_else(|| Error::invalid_argument(format!("no connection id in {}", message.uri)))
        }
        let dealer = session.dealer();
        let connection_ids = dealer.listen_for("hm://pusher/v1/connections/", connection_id)?;
        let commands = dealer.handle_for("hm://connect-state/v1/player/command")?;
        // the one request that needs no login, made early as librespot does
        let _ = session.spclient().client_token().await?;
        Ok(Self {
            connection_ids,
            commands,
        })
    }

    /// starts reporting; the session must be connected
    pub(crate) async fn launch(
        self,
        session: Session,
        name: String,
        volume: u16,
    ) -> Result<Arc<Reporter>, Error> {
        session.dealer().start().await?;
        let (desired, updates) = watch::channel(Desired {
            volume,
            ..Desired::default()
        });
        tokio::spawn(run(session.clone(), name, self, updates));
        Ok(Arc::new(Reporter { desired, session }))
    }
}

impl Reporter {
    fn update(&self, change: impl FnOnce(&mut Desired)) {
        self.desired.send_modify(change);
    }

    fn stamp(&self) -> i64 {
        now_ms(&self.session)
    }

    /// `uri` is playing, `position_ms` into it
    pub(crate) fn playing(&self, uri: String, position_ms: u32, duration_ms: u32) {
        let stamp = self.stamp();
        self.update(|desired| {
            desired.uri = Some(uri);
            desired.status = Status::Playing;
            desired.position_ms = position_ms;
            desired.duration_ms = duration_ms;
            desired.stamp_ms = stamp;
        });
    }

    /// `uri` is paused at `position_ms`. a song that is no longer the one
    /// reported (its replacement is already loading) changes nothing
    pub(crate) fn paused(&self, uri: &str, position_ms: u32) {
        let stamp = self.stamp();
        self.update(|desired| {
            if desired.uri.as_deref() != Some(uri) {
                return;
            }
            desired.status = Status::Paused;
            desired.position_ms = position_ms;
            desired.stamp_ms = stamp;
        });
    }

    /// the position of `uri` changed under it (a seek), keeping it playing or paused
    pub(crate) fn seeked(&self, uri: &str, position_ms: u32) {
        let stamp = self.stamp();
        self.update(|desired| {
            if desired.uri.as_deref() != Some(uri) {
                return;
            }
            desired.position_ms = position_ms;
            desired.stamp_ms = stamp;
        });
    }

    pub(crate) fn duration(&self, uri: &str, duration_ms: u32) {
        self.update(|desired| {
            if desired.uri.as_deref() == Some(uri) {
                desired.duration_ms = duration_ms;
            }
        });
    }

    pub(crate) fn volume(&self, volume: u16) {
        self.update(|desired| desired.volume = volume);
    }
}

fn device_info(name: &str, volume: u16, session: &Session) -> DeviceInfo {
    DeviceInfo {
        can_play: true,
        volume: volume.into(),
        name: name.to_owned(),
        device_id: session.device_id().to_string(),
        device_type: EnumOrUnknown::new(DeviceType::Computer.into()),
        device_software_version: version::SEMVER.to_string(),
        spirc_version: version::SPOTIFY_SPIRC_VERSION.to_string(),
        client_id: session.client_id(),
        is_group: false,
        capabilities: MessageField::some(Capabilities {
            volume_steps: 64,
            disable_volume: false,
            gaia_eq_connect_id: true,
            can_be_player: true,
            needs_full_player_state: true,
            is_observable: true,
            is_controllable: true,
            hidden: false,
            supports_gzip_pushes: true,
            supports_logout: false,
            supported_types: vec![
                "audio/episode".into(),
                "audio/track".into(),
                "audio/local".into(),
            ],
            supports_playlist_v2: true,
            supports_transfer_command: true,
            supports_command_request: true,
            supports_set_options_command: true,
            is_voice_enabled: false,
            restrict_to_local: false,
            connect_disabled: false,
            supports_rename: false,
            supports_external_episodes: false,
            supports_set_backend_metadata: false,
            supports_hifi: MessageField::none(),
            supports_dj: false,
            supports_rooms: false,
            supported_audio_quality: EnumOrUnknown::new(AudioQuality::VERY_HIGH),
            command_acks: true,
            ..Default::default()
        }),
        ..Default::default()
    }
}

/// the same uid for the same song, so repeated reports of it agree
fn uid_of(uri: &str) -> String {
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    uri.hash(&mut hasher);
    format!("{:032x}", u128::from(hasher.finish()))
}

fn player_state(desired: &Desired, session: &Session) -> PlayerState {
    let mut state = PlayerState {
        session_id: session.session_id(),
        is_system_initiated: true,
        playback_speed: 1.0,
        play_origin: MessageField::some(PlayOrigin::new()),
        suppressions: MessageField::some(Suppressions::new()),
        options: MessageField::some(ContextPlayerOptions::new()),
        ..Default::default()
    };
    let Some(uri) = &desired.uri else {
        return state;
    };
    let paused = desired.status != Status::Playing;
    // desktop and mobile clients want every flag set while paused, or their
    // play button is grayed out
    state.is_paused = paused;
    state.is_buffering = paused;
    state.is_playing = true;
    state.playback_speed = if paused { 0.0 } else { 1.0 };
    state.duration = desired.duration_ms.into();
    state.position_as_of_timestamp = desired.position_ms.into();
    state.timestamp = desired.stamp_ms;
    state.context_uri = CONTEXT.to_owned();
    state.context_url = format!("context://{CONTEXT}");
    state.index = MessageField::some(ContextIndex::new());
    let mut metadata = HashMap::new();
    metadata.insert("context_uri".to_owned(), CONTEXT.to_owned());
    metadata.insert("entity_uri".to_owned(), CONTEXT.to_owned());
    state.track = MessageField::some(ProvidedTrack {
        uri: uri.clone(),
        uid: uid_of(uri),
        metadata,
        provider: "context".to_owned(),
        ..Default::default()
    });
    state
}

fn request(
    reason: PutStateReason,
    name: &str,
    desired: &Desired,
    active_since: Option<i64>,
    session: &Session,
) -> PutStateRequest {
    PutStateRequest {
        member_type: EnumOrUnknown::new(MemberType::CONNECT_STATE),
        put_state_reason: EnumOrUnknown::new(reason),
        is_active: active_since.is_some(),
        started_playing_at: active_since.unwrap_or(0).max(0) as u64,
        client_side_timestamp: now_ms(session).max(0) as u64,
        device: MessageField::some(Device {
            device_info: MessageField::some(device_info(name, desired.volume, session)),
            player_state: MessageField::some(player_state(desired, session)),
            ..Default::default()
        }),
        ..Default::default()
    }
}

/// the one task that talks to spotify: it registers the device when the
/// dealer says what its connection is, then keeps the latest state sent
async fn run(
    session: Session,
    name: String,
    mut prepared: Prepared,
    mut updates: watch::Receiver<Desired>,
) {
    let mut registered = false;
    let mut active_since: Option<i64> = None;
    let mut check = tokio::time::interval(Duration::from_secs(5));
    loop {
        if session.is_invalid() {
            break;
        }
        tokio::select! {
            id = prepared.connection_ids.next() => match id {
                Some(Ok(id)) => {
                    session.set_connection_id(&id);
                    let desired = updates.borrow().clone();
                    let new_device = request(PutStateReason::NEW_DEVICE, &name, &desired, None, &session);
                    match session.spclient().put_connect_state_request(&new_device).await {
                        Ok(_) => {
                            registered = true;
                            active_since = None;
                            // a reconnect forgets what was told before
                            updates.mark_changed();
                        }
                        Err(error) => eprintln!("spotify-core: could not register the connect device: {error}"),
                    }
                }
                Some(Err(error)) => eprintln!("spotify-core: bad connection id message: {error}"),
                None => break,
            },
            command = prepared.commands.next() => match command {
                // nothing can be controlled from elsewhere
                Some((_request, reply)) => { let _ = reply.send(Reply::Failure); }
                None => break,
            },
            changed = updates.changed(), if registered => {
                if changed.is_err() {
                    break;
                }
                tokio::time::sleep(COALESCE).await;
                let desired = updates.borrow_and_update().clone();
                if desired.uri.is_some() && active_since.is_none() {
                    active_since = Some(now_ms(&session));
                }
                let state = request(PutStateReason::PLAYER_STATE_CHANGED, &name, &desired, active_since, &session);
                if let Err(error) = session.spclient().put_connect_state_request(&state).await {
                    eprintln!("spotify-core: could not report playback: {error}");
                }
            },
            _ = check.tick() => {}
        }
    }
    if registered {
        let _ = session.spclient().delete_connect_state_request().await;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_song_keeps_its_uid() {
        let uri = "spotify:track:2WUy2Uywcj5cP0IXQagO3z";
        assert_eq!(uid_of(uri), uid_of(uri));
        assert_ne!(uid_of(uri), uid_of("spotify:track:4cOdK2wGLETKBW3PvgPWqT"));
        assert_eq!(uid_of(uri).len(), 32);
    }

    #[test]
    fn status_starts_idle() {
        assert_eq!(Desired::default().status, Status::Idle);
        assert!(Desired::default().uri.is_none());
    }
}
