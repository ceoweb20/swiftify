use std::{collections::HashMap, io::Write, time::Duration};

use byteorder::{BigEndian, ByteOrder, WriteBytesExt};
use bytes::Bytes;
use thiserror::Error;
use tokio::sync::oneshot;

use crate::{Error, FileId, SpotifyId, packet::PacketType, util::SeqGenerator};

#[derive(Debug, Hash, PartialEq, Eq, Copy, Clone)]
pub struct AudioKey(pub [u8; 16]);

#[derive(Debug, Error)]
pub enum AudioKeyError {
    #[error("audio key error")]
    AesKey,
    #[error("other end of channel disconnected")]
    Channel,
    #[error("unexpected packet type {0}")]
    Packet(u8),
    #[error("sequence {0} not pending")]
    Sequence(u32),
    #[error("audio key response timeout")]
    Timeout,
}

impl From<AudioKeyError> for Error {
    fn from(err: AudioKeyError) -> Self {
        match err {
            AudioKeyError::AesKey => Error::unavailable(err),
            AudioKeyError::Channel => Error::aborted(err),
            AudioKeyError::Sequence(_) => Error::aborted(err),
            AudioKeyError::Packet(_) => Error::unimplemented(err),
            AudioKeyError::Timeout => Error::aborted(err),
        }
    }
}

// inner state notes:
// - cache: keys already fetched this session, so a replayed track costs no
//   round trip on either path. bounded like the official client's cache
//   (its product info says key-caching-max-count: 10000).
// - ap_refused: set once the access-point channel has refused a key. the
//   service is account-wide, so one refusal means it will not serve any
//   track this session (observed by every project that walked this path);
//   after it, requests go straight to the license endpoint.
component! {
    AudioKeyManager : AudioKeyManagerInner {
        sequence: SeqGenerator<u32> = SeqGenerator::new(0),
        pending: HashMap<u32, oneshot::Sender<Result<AudioKey, Error>>> = HashMap::new(),
        cache: HashMap<FileId, AudioKey> = HashMap::new(),
        ap_refused: bool = false,
        refusals: u64 = 0,
    }
}

impl AudioKeyManager {
    /// how many key requests spotify has refused since this manager was made.
    /// an account spotify has locked out of the aes keys (a new one) gets
    /// every request refused, and a rate-limited one a run of them
    pub fn refusals(&self) -> u64 {
        self.lock(|inner| inner.refusals)
    }

    pub(crate) fn dispatch(&self, cmd: PacketType, mut data: Bytes) -> Result<(), Error> {
        let seq = BigEndian::read_u32(data.split_to(4).as_ref());

        let sender = self
            .lock(|inner| inner.pending.remove(&seq))
            .ok_or(AudioKeyError::Sequence(seq))?;

        match cmd {
            PacketType::AesKey => {
                let mut key = [0u8; 16];
                key.copy_from_slice(data.as_ref());
                sender
                    .send(Ok(AudioKey(key)))
                    .map_err(|_| AudioKeyError::Channel)?
            }
            PacketType::AesKeyError => {
                error!(
                    "error audio key {:x} {:x}",
                    data.as_ref()[0],
                    data.as_ref()[1]
                );
                self.lock(|inner| inner.refusals += 1);
                sender
                    .send(Err(AudioKeyError::AesKey.into()))
                    .map_err(|_| AudioKeyError::Channel)?
            }
            _ => {
                trace!("Did not expect {cmd:?} AES key packet with data {data:#?}");
                return Err(AudioKeyError::Packet(cmd as u8).into());
            }
        }

        Ok(())
    }

    pub async fn request(&self, track: SpotifyId, file: FileId) -> Result<AudioKey, Error> {
        // a key is immutable per file: serve replays from the session cache
        if let Some(key) = self.lock(|inner| inner.cache.get(&file).copied()) {
            return Ok(key);
        }

        // once the access-point channel has refused, it refuses every file
        // for the session — skip straight to the license endpoint
        let ap_latched = self.lock(|inner| inner.ap_refused);
        if !ap_latched {
            match self.request_access_point(track, file).await {
                Ok(key) => {
                    self.cache_key(file, key);
                    return Ok(key);
                }
                Err(error) => {
                    // latch only on an outright refusal (AesKey error), not a
                    // timeout: a refusal is the account being moved off the
                    // channel, a timeout is one unlucky request
                    if matches!(error.kind, crate::error::ErrorKind::Unavailable) {
                        self.lock(|inner| inner.ap_refused = true);
                        debug!("AP audio key refused — latched off for this session, going to the playplay license");
                    } else {
                        debug!("AP audio key failed ({error:?}), trying the playplay license");
                    }
                    self.request_playplay(track, file, error).await
                }
            }
        } else {
            // latched: no AP attempt, no error to preserve
            self.request_playplay(
                track,
                file,
                crate::error::Error::aborted("ap key channel latched off"),
            )
            .await
        }
    }

    async fn request_playplay(
        &self,
        track: SpotifyId,
        file: FileId,
        fallback_error: Error,
    ) -> Result<AudioKey, Error> {
        let session = self.session();
        match crate::playplay::request_playplay_key(session.spclient(), track, file).await {
            Ok(key) => {
                let key = AudioKey(key);
                self.cache_key(file, key);
                Ok(key)
            }
            Err(playplay_error) => {
                warn!("playplay license also failed: {playplay_error}");
                Err(fallback_error)
            }
        }
    }

    /// remembers a key for the session. the cache is bounded to the official
    /// client's own limit (key-caching-max-count: 10000); past it the oldest
    /// entry is dropped, which only costs a re-fetch if that track replays.
    fn cache_key(&self, file: FileId, key: AudioKey) {
        const MAX_KEYS: usize = 10_000;
        self.lock(|inner| {
            if inner.cache.len() >= MAX_KEYS && !inner.cache.contains_key(&file) {
                if let Some(oldest) = inner.cache.keys().next().copied() {
                    inner.cache.remove(&oldest);
                }
            }
            inner.cache.insert(file, key);
        });
    }

    /// The access-point key channel alone, without the PlayPlay fallback.
    /// Public so the key paths can be compared against each other in tests.
    pub async fn request_access_point(&self, track: SpotifyId, file: FileId) -> Result<AudioKey, Error> {
        let (tx, rx) = oneshot::channel();

        let seq = self.lock(move |inner| {
            let seq = inner.sequence.get();
            inner.pending.insert(seq, tx);
            seq
        });

        self.send_key_request(seq, track, file)?;
        const KEY_RESPONSE_TIMEOUT: Duration = Duration::from_millis(1500);
        match tokio::time::timeout(KEY_RESPONSE_TIMEOUT, rx).await {
            Err(_) => {
                error!("Audio key response timeout");
                Err(AudioKeyError::Timeout.into())
            }
            Ok(k) => k?,
        }
    }

    fn send_key_request(&self, seq: u32, track: SpotifyId, file: FileId) -> Result<(), Error> {
        let mut data: Vec<u8> = Vec::new();
        data.write_all(&file.0)?;
        data.write_all(&track.to_raw())?;
        data.write_u32::<BigEndian>(seq)?;
        data.write_u16::<BigEndian>(0x0000)?;

        self.session().send_packet(PacketType::RequestKey, data)
    }
}
