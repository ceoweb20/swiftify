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

use std::{
    collections::VecDeque,
    f32::consts::TAU,
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, AtomicU64, Ordering},
        mpsc::{SyncSender, sync_channel},
    },
    thread,
    time::{Duration, Instant},
};

use librespot::playback::{
    NUM_CHANNELS, SAMPLE_RATE,
    audio_backend::{Sink, SinkError, SinkResult},
    convert::Converter,
    decoder::AudioPacket,
    mixer::VolumeGetter,
};
use rodio::cpal::{self, traits::StreamTrait};
use rustfft::{Fft, FftPlanner, num_complex::Complex};

use crate::{
    dsp::{DspProcessor, SharedDsp},
    output::open_stereo_stream,
};

pub(crate) const BAND_COUNT: usize = 48;

// cava's dual-window frequency analysis:
// https://github.com/karlstav/cava/blob/master/cavacore.c
const MAIN_FFT_SIZE: usize = 4_096;
const BASS_FFT_SIZE: usize = MAIN_FFT_SIZE * 2;
// a 1,024-frame hop still updates the analyzer about 43 times per second at
// 44.1 khz. core animation interpolates these targets, so running the two cava
// ffts more often would spend cpu on frames the ui does not directly present.
const HOP_SIZE: usize = 1_024;
pub(crate) const ANALYSIS_SAMPLE_COUNT: usize = HOP_SIZE * NUM_CHANNELS as usize;
const MIN_FREQUENCY_HZ: f32 = 40.0;
const MAX_FREQUENCY_HZ: f32 = 16_000.0;
const BASS_CUTOFF_HZ: f32 = 100.0;
// normalize actual band energy before applying a gentle display curve. never
// multiply db values or feed visible heights back into the next audio target:
// both flatten dynamics and turn sustained distortion into a wall of full bars.
const MIN_REFERENCE_AMPLITUDE: f32 = 0.08;
const INITIAL_REFERENCE_AMPLITUDE: f32 = 0.25;
/// knee of the soft shoulder. higher pins sooner: a band sitting at the running reference
/// lands at 1 - e^-knee, so 1.25 left sustained peaks near 71% and 1.8 puts them near 83%.
const COMPRESSION_KNEE: f32 = 1.8;
const ATTACK_SECONDS: f32 = 0.008;
const RELEASE_SECONDS: f32 = 0.065;

#[derive(Clone)]
pub(crate) struct SharedSpectrum {
    levels: Arc<Mutex<[f32; BAND_COUNT]>>,
    enabled: Arc<AtomicBool>,
    generation: Arc<AtomicU64>,
    sequence: Arc<AtomicU64>,
    analysis_sender: SyncSender<AnalysisChunk>,
}

impl SharedSpectrum {
    pub(crate) fn new() -> Self {
        let levels = Arc::new(Mutex::new([0.0; BAND_COUNT]));
        let enabled = Arc::new(AtomicBool::new(false));
        let generation = Arc::new(AtomicU64::new(0));
        let sequence = Arc::new(AtomicU64::new(0));
        let (analysis_sender, analysis_receiver) = sync_channel::<AnalysisChunk>(1);
        let worker_levels = Arc::clone(&levels);
        let worker_enabled = Arc::clone(&enabled);
        let worker_generation = Arc::clone(&generation);
        thread::Builder::new()
            .name("swiftify-spectrum".to_owned())
            .spawn(move || {
                let mut analyzer = SpectrumAnalyzer::new();
                let mut analyzer_generation = 0;
                let mut previous_sequence = None;

                while let Ok(chunk) = analysis_receiver.recv() {
                    let current_generation = worker_generation.load(Ordering::Acquire);
                    if chunk.generation != current_generation
                        || !worker_enabled.load(Ordering::Relaxed)
                    {
                        continue;
                    }

                    if analyzer_generation != current_generation {
                        analyzer.clear();
                        analyzer_generation = current_generation;
                        previous_sequence = None;
                    }

                    let frame_skip = previous_sequence
                        .map(|previous: u64| chunk.sequence.wrapping_sub(previous).max(1))
                        .unwrap_or(1);
                    previous_sequence = Some(chunk.sequence);
                    analyzer.push_interleaved_f32(&chunk.samples, chunk.output_gain, frame_skip);

                    // a clear can race an fft. never republish the old track's
                    // levels after the generation has advanced.
                    if worker_generation.load(Ordering::Acquire) == current_generation
                        && worker_enabled.load(Ordering::Relaxed)
                        && let Ok(mut published) = worker_levels.lock()
                        && worker_generation.load(Ordering::Acquire) == current_generation
                        && worker_enabled.load(Ordering::Relaxed)
                    {
                        *published = analyzer.levels();
                    }
                }
            })
            .expect("failed to start the spectrum analysis worker");

        Self {
            levels,
            enabled,
            generation,
            sequence,
            analysis_sender,
        }
    }

    pub(crate) fn set_enabled(&self, enabled: bool) {
        self.enabled.store(enabled, Ordering::Relaxed);
        if !enabled {
            self.clear();
        }
    }

    pub(crate) fn is_enabled(&self) -> bool {
        self.enabled.load(Ordering::Relaxed)
    }

    /// analysis windows queued so far
    #[cfg(test)]
    pub(crate) fn queued(&self) -> u64 {
        self.sequence.load(Ordering::Relaxed)
    }

    pub(crate) fn levels(&self) -> Vec<f32> {
        self.levels
            .lock()
            .map(|levels| levels.to_vec())
            .unwrap_or_else(|_| vec![0.0; BAND_COUNT])
    }

    /// the same levels without an allocation, for a front end that polls them
    /// every frame it draws. returns how many bands were written.
    pub(crate) fn copy_levels(&self, out: &mut [f32]) -> usize {
        let count = out.len().min(BAND_COUNT);
        match self.levels.lock() {
            Ok(levels) => out[..count].copy_from_slice(&levels[..count]),
            Err(_) => out[..count].fill(0.0),
        }
        count
    }

    pub(crate) fn clear(&self) {
        self.generation.fetch_add(1, Ordering::AcqRel);
        if let Ok(mut levels) = self.levels.lock() {
            levels.fill(0.0);
        }
    }

    /// moves on each time the levels are cleared: a window begun before
    /// then is begun again
    fn generation(&self) -> u64 {
        self.generation.load(Ordering::Acquire)
    }

    pub(crate) fn queue_interleaved_f32(
        &self,
        samples: [f32; ANALYSIS_SAMPLE_COUNT],
        output_gain: f64,
    ) {
        let generation = self.generation.load(Ordering::Acquire);
        if !self.is_enabled() {
            return;
        }
        let sequence = self.sequence.fetch_add(1, Ordering::Relaxed);
        // the single-slot channel is deliberately lossy. if analysis ever falls
        // behind, discard this frame instead of blocking the audio output thread.
        let _ = self.analysis_sender.try_send(AnalysisChunk {
            samples,
            output_gain,
            generation,
            sequence,
        });
    }
}

/// whether the audio device is still taking what it is handed.
#[derive(Clone, Default)]
pub(crate) struct OutputHealth {
    stalled: Arc<AtomicBool>,
}

impl OutputHealth {
    pub(crate) fn new() -> Self {
        Self::default()
    }

    pub(crate) fn mark_stalled(&self) {
        self.stalled.store(true, Ordering::Release);
    }

    pub(crate) fn clear(&self) {
        self.stalled.store(false, Ordering::Release);
    }

    pub(crate) fn is_stalled(&self) -> bool {
        self.stalled.load(Ordering::Acquire)
    }
}

/// a live connection to the default output device. held as an option so a
/// device that cannot be opened is an error the player can report, rather than
/// a panic on the thread librespot builds its sink on.
struct Output {
    queue: Arc<PlayQueue>,
    stream: cpal::Stream,
    /// the rate the device plays at, which librespot's packets are brought to
    /// before they are queued (RateConverter)
    rate: u32,
}

/// what the device plays, in the order it was written: whole chunks, copied
/// out a block at a time on the device's own thread.
///
/// rodio's sink and mixer handed the device one sample at a time, each
/// through a stack of wrappers (the queue, a converter that had nothing to
/// convert, pausing, stopping, skipping), and that walk cost as much as the
/// whole interface thread while music played. it also kept the device
/// running on silence while paused; this stream is stopped instead.
#[derive(Default)]
pub(crate) struct PlayQueue {
    state: Mutex<QueueState>,
}

#[derive(Default)]
struct QueueState {
    chunks: VecDeque<PlayedChunk>,
    /// the analysis window being filled, carried from chunk to chunk as the
    /// device plays them
    window: AnalysisWindow,
    /// chunks played out, for the writing thread to free: freeing them on
    /// the device's thread had it handing memory back to the system there
    spent: Vec<PlayedChunk>,
}

impl PlayQueue {
    fn state(&self) -> std::sync::MutexGuard<'_, QueueState> {
        self.state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
    }

    pub(crate) fn len(&self) -> usize {
        self.state().chunks.len()
    }

    /// queues a chunk, and takes what has been played back into `spent`,
    /// leaving the device's thread the list `spent` was (emptied, its room
    /// kept) to fill again
    fn push(&self, chunk: PlayedChunk, spent: &mut Vec<PlayedChunk>) {
        let mut state = self.state();
        state.chunks.push_back(chunk);
        std::mem::swap(&mut state.spent, spent);
    }

    fn clear(&self) {
        let chunks = std::mem::take(&mut self.state().chunks);
        drop(chunks);
    }

    /// fills interleaved stereo from the front of the queue, and silence once
    /// it runs out
    pub(crate) fn fill(&self, out: &mut [f32]) {
        let mut written = 0;
        {
            let mut state = self.state();
            let QueueState {
                chunks,
                window,
                spent,
            } = &mut *state;
            while written < out.len() {
                let Some(front) = chunks.front_mut() else {
                    break;
                };
                written += front.fill(&mut out[written..], window);
                if front.is_done()
                    && let Some(played) = chunks.pop_front()
                {
                    spent.push(played);
                }
            }
        }
        out[written..].fill(0.0);
    }
}

/// a packet as the device plays it
pub(crate) enum PlayedChunk {
    Plain {
        samples: Vec<f32>,
        position: usize,
    },
    /// boxed: it carries the analysis window it is filling
    Analyzed(Box<AnalyzedSource>),
}

impl PlayedChunk {
    /// copies as much of what is left of it as fits; how many samples
    fn fill(&mut self, out: &mut [f32], window: &mut AnalysisWindow) -> usize {
        match self {
            Self::Plain { samples, position } => {
                let count = out.len().min(samples.len() - *position);
                out[..count].copy_from_slice(&samples[*position..*position + count]);
                *position += count;
                count
            }
            Self::Analyzed(source) => source.fill(out, window),
        }
    }

    fn is_done(&self) -> bool {
        match self {
            Self::Plain { samples, position } => *position == samples.len(),
            Self::Analyzed(source) => source.is_done(),
        }
    }
}

/// librespot's 44.1 khz packets brought to the device's rate as they are
/// written, by linear interpolation that carries on from one packet into the
/// next. rodio converts each packet on its own otherwise, starting afresh at
/// its first frame and running past its last: about two frames too many a
/// packet at 48 khz, so music played 0.4% slow, with a small jump in the wave
/// at every packet's edge, some forty times a second.
struct RateConverter {
    to: u32,
    /// input frames an output frame steps over
    step: f64,
    /// where the next output frame falls, in input frames from the start of
    /// the next packet; from -1, between the last packet's final frame and
    /// the next one's first
    position: f64,
    previous: [f32; NUM_CHANNELS as usize],
    primed: bool,
}

impl RateConverter {
    const CHANNELS: usize = NUM_CHANNELS as usize;

    fn new(to: u32) -> Self {
        Self {
            to,
            step: f64::from(SAMPLE_RATE) / f64::from(to),
            position: 0.0,
            previous: [0.0; NUM_CHANNELS as usize],
            primed: false,
        }
    }

    /// starts afresh, as after a seek or a new track
    fn reset(&mut self) {
        self.position = 0.0;
        self.primed = false;
    }

    /// interleaved frames in, interleaved frames out
    fn convert(&mut self, input: &[f32]) -> Vec<f32> {
        let frames = input.len() / Self::CHANNELS;
        if frames == 0 {
            return Vec::new();
        }
        if !self.primed {
            self.previous.copy_from_slice(&input[..Self::CHANNELS]);
            self.position = 0.0;
            self.primed = true;
        }
        let frame = |index: isize, channel: usize| -> f32 {
            if index < 0 {
                self.previous[channel]
            } else {
                input[index as usize * Self::CHANNELS + channel]
            }
        };
        let mut output =
            Vec::with_capacity(((frames as f64 / self.step) as usize + 2) * Self::CHANNELS);
        // each output frame lies between two input frames it can see: the
        // last of the packet before stands in for the frame before this one's
        // first
        while self.position < (frames - 1) as f64 {
            let base = self.position.floor();
            let index = base as isize;
            let fraction = (self.position - base) as f32;
            for channel in 0..Self::CHANNELS {
                let from = frame(index, channel);
                let to = frame(index + 1, channel);
                output.push(from + (to - from) * fraction);
            }
            self.position += self.step;
        }
        self.position -= frames as f64;
        self.previous
            .copy_from_slice(&input[(frames - 1) * Self::CHANNELS..frames * Self::CHANNELS]);
        output
    }
}

pub(crate) struct SpectrumSink {
    output: Option<Output>,
    spectrum: SharedSpectrum,
    output_volume: Box<dyn VolumeGetter + Send>,
    dsp: DspProcessor,
    health: OutputHealth,
    converter: Option<RateConverter>,
    /// chunks the device has played, freed here on the writing thread
    spent: Vec<PlayedChunk>,
}

impl SpectrumSink {
    pub(crate) fn new(
        spectrum: SharedSpectrum,
        output_volume: Box<dyn VolumeGetter + Send>,
        dsp: SharedDsp,
        health: OutputHealth,
    ) -> Self {
        Self {
            output: None,
            spectrum,
            output_volume,
            dsp: DspProcessor::new(dsp),
            health,
            converter: None,
            spent: Vec::with_capacity(Self::QUEUE_HIGH_WATER * 2),
        }
    }
}

impl SpectrumSink {
    /// limits how long a stopped output can block playback
    const DRAIN_TIMEOUT: Duration = Duration::from_secs(2);
    /// the most packets queued on the device, same as librespot's rodio backend.
    /// about half a second of audio.
    const QUEUE_HIGH_WATER: usize = 26;
    /// a full queue is left to play down to here before more is written, so
    /// the player thread wakes a few times per refill instead of two or three
    /// times for every packet the device takes. a packet is about 23 ms, so
    /// this is still a third of a second ahead, or half that at double tempo.
    const QUEUE_LOW_WATER: usize = 16;
    /// how often a full queue is checked: roughly two packets' worth.
    const QUEUE_POLL: Duration = Duration::from_millis(40);

    /// opened here rather than in new because new runs on librespot's own player thread,
    /// where a failure could only ever have been a panic.
    fn ensure_output(&mut self) -> SinkResult<()> {
        if self.output.is_some() {
            return Ok(());
        }

        let queue = Arc::new(PlayQueue::default());
        let filled = Arc::clone(&queue);
        let Some((stream, rate)) = open_stereo_stream(move |out| filled.fill(out)) else {
            self.health.mark_stalled();
            return Err(SinkError::ConnectionRefused(
                "no audio output device is available".to_owned(),
            ));
        };
        self.output = Some(Output {
            queue,
            stream,
            rate,
        });
        Ok(())
    }

    /// waits for the queue to fall to limit sources, checking every interval.
    /// false if it timed out, which means the device is no longer consuming
    /// what we hand it.
    fn drain_to(queue: &PlayQueue, limit: usize, interval: Duration) -> bool {
        let deadline = Instant::now() + Self::DRAIN_TIMEOUT;
        while queue.len() > limit {
            if Instant::now() >= deadline {
                return false;
            }
            thread::sleep(interval);
        }
        true
    }

    /// the device has stopped consuming. drop the stream along with the flag:
    /// a dead audio unit stays dead, so the next start is better off opening
    /// a fresh one than resuming a queue nothing is reading.
    fn mark_stalled(&mut self) {
        self.health.mark_stalled();
        self.output = None;
    }
}

/// librespot decodes on a thread of its own at ordinary priority, a few
/// hundred milliseconds ahead of the device. a busy interface could hold it
/// up long enough to run that lead dry, so it asks to be scheduled like the
/// interface itself, once, from the thread it runs on.
fn raise_decoder_priority() {
    thread_local! {
        static RAISED: std::cell::Cell<bool> = const { std::cell::Cell::new(false) };
    }
    if RAISED.with(|raised| raised.replace(true)) {
        return;
    }
    #[cfg(target_os = "macos")]
    {
        const QOS_CLASS_USER_INTERACTIVE: u32 = 0x21;
        unsafe extern "C" {
            fn pthread_set_qos_class_self_np(qos_class: u32, relative_priority: i32) -> i32;
        }
        // SAFETY: changes only the calling thread's own scheduling class.
        unsafe {
            pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
        }
    }
}

impl Sink for SpectrumSink {
    fn start(&mut self) -> SinkResult<()> {
        raise_decoder_priority();
        self.ensure_output()?;
        if let Some(output) = &self.output
            && output.stream.play().is_err()
        {
            self.mark_stalled();
            return Err(SinkError::ConnectionRefused(
                "the audio output device could not be started".to_owned(),
            ));
        }
        Ok(())
    }

    fn stop(&mut self) -> SinkResult<()> {
        if let Some(output) = &self.output {
            // let queued audio play out, then stop waiting for it.
            if !Self::drain_to(&output.queue, 0, Duration::from_millis(10)) {
                output.queue.clear();
            }
            // the device stops asking for samples while nothing plays
            let _ = output.stream.pause();
        }
        self.spectrum.clear();
        // nothing of one track may be overlap-added onto the next, or
        // interpolated into it.
        self.dsp.reset();
        if let Some(converter) = &mut self.converter {
            converter.reset();
        }
        Ok(())
    }

    fn write(&mut self, packet: AudioPacket, converter: &mut Converter) -> SinkResult<()> {
        self.ensure_output()?;
        let samples = packet
            .samples()
            .map_err(|error| SinkError::OnWrite(error.to_string()))?;
        // before the tap, so the visualizer shows what the speakers get
        // rather than what the decoder produced.
        let samples = self.dsp.process(converter.f64_to_f32(samples));
        let Some(output) = &self.output else {
            return Err(SinkError::ConnectionRefused(
                "no audio output device is available".to_owned(),
            ));
        };

        // brought to the device's rate here, so rodio passes them through
        let converted = if output.rate == SAMPLE_RATE {
            None
        } else {
            let converter = self
                .converter
                .get_or_insert_with(|| RateConverter::new(output.rate));
            if converter.to != output.rate {
                *converter = RateConverter::new(output.rate);
            }
            Some(converter.convert(&samples))
        };

        let chunk = if self.spectrum.is_enabled() {
            PlayedChunk::Analyzed(Box::new(AnalyzedSource::played_at(
                samples,
                converted.map(|played| (played, output.rate)),
                self.spectrum.clone(),
                self.output_volume.attenuation_factor(),
            )))
        } else {
            // do not route every decoded sample through the analysis when
            // visualizers are disabled.
            let samples = converted.unwrap_or(samples);
            PlayedChunk::Plain {
                samples,
                position: 0,
            }
        };
        output.queue.push(chunk, &mut self.spent);
        // what the device has played, freed here; the list keeps its room
        self.spent.clear();

        // the same bounded queue as librespot's rodio backend, refilled in
        // batches rather than one packet per wake.
        if output.queue.len() > Self::QUEUE_HIGH_WATER
            && !Self::drain_to(&output.queue, Self::QUEUE_LOW_WATER, Self::QUEUE_POLL)
        {
            self.mark_stalled();
            return Err(SinkError::OnWrite(
                "the audio output stopped accepting samples".to_owned(),
            ));
        }

        Ok(())
    }
}

struct AnalysisChunk {
    samples: [f32; ANALYSIS_SAMPLE_COUNT],
    output_gain: f64,
    generation: u64,
    sequence: u64,
}

pub(crate) struct AnalyzedSource {
    /// what the device plays
    played: Vec<f32>,
    position: usize,
    /// the chunk as librespot decoded it, when the device plays it at another
    /// rate: the analyser works at the decoder's rate, and follows playback
    /// through these as the converted frames are played
    decoded: Option<DecodedChunk>,
    spectrum: SharedSpectrum,
    output_gain: f64,
    /// played out, and all it decoded handed to the analysis
    finished: bool,
}

struct DecodedChunk {
    samples: Vec<f32>,
    /// samples handed to the analysis so far
    fed: usize,
    /// decoded frames a played frame stands for
    step: f64,
    /// samples played so far
    played: usize,
}

/// the analysis window being filled, carried from chunk to chunk
struct AnalysisWindow {
    samples: [f32; ANALYSIS_SAMPLE_COUNT],
    index: usize,
    enabled: bool,
    /// the spectrum's generation it was begun in
    generation: u64,
}

impl Default for AnalysisWindow {
    fn default() -> Self {
        Self {
            samples: [0.0; ANALYSIS_SAMPLE_COUNT],
            index: 0,
            enabled: true,
            generation: 0,
        }
    }
}

impl AnalysisWindow {
    /// starts afresh if the levels were cleared since it was begun
    fn follow(&mut self, spectrum: &SharedSpectrum) {
        let generation = spectrum.generation();
        if generation != self.generation {
            self.generation = generation;
            self.index = 0;
        }
    }

    /// takes samples in runs, a window's room at a time, and queues each
    /// window as it fills
    fn feed(&mut self, spectrum: &SharedSpectrum, output_gain: f64, mut samples: &[f32]) {
        while !samples.is_empty() {
            if self.index == 0 {
                self.enabled = spectrum.is_enabled();
            }
            let taken = samples.len().min(ANALYSIS_SAMPLE_COUNT - self.index);
            if self.enabled {
                self.samples[self.index..self.index + taken].copy_from_slice(&samples[..taken]);
            }
            self.index += taken;
            samples = &samples[taken..];
            if self.index == ANALYSIS_SAMPLE_COUNT {
                if self.enabled {
                    spectrum.queue_interleaved_f32(self.samples, output_gain);
                }
                self.index = 0;
            }
        }
    }
}

impl AnalyzedSource {
    /// a chunk played at the decoder's own rate
    #[cfg(test)]
    fn new(samples: Vec<f32>, spectrum: SharedSpectrum, output_gain: f64) -> Self {
        Self::played_at(samples, None, spectrum, output_gain)
    }

    /// a chunk as decoded, and what the device plays of it at its own rate
    /// when that is another
    fn played_at(
        decoded: Vec<f32>,
        converted: Option<(Vec<f32>, u32)>,
        spectrum: SharedSpectrum,
        output_gain: f64,
    ) -> Self {
        let (played, decoded) = match converted {
            Some((played, rate)) => (
                played,
                Some(DecodedChunk {
                    samples: decoded,
                    fed: 0,
                    step: f64::from(SAMPLE_RATE) / f64::from(rate),
                    played: 0,
                }),
            ),
            None => (decoded, None),
        };
        Self {
            played,
            position: 0,
            decoded,
            spectrum,
            output_gain,
            finished: false,
        }
    }
}

impl AnalyzedSource {
    /// copies as much of what is left to play as fits into `out`, and hands
    /// the analysis what it covers; how many samples
    fn fill(&mut self, out: &mut [f32], window: &mut AnalysisWindow) -> usize {
        const CHANNELS: usize = NUM_CHANNELS as usize;
        window.follow(&self.spectrum);
        let start = self.position;
        let count = out.len().min(self.played.len() - start);
        out[..count].copy_from_slice(&self.played[start..start + count]);
        self.position += count;

        match &mut self.decoded {
            None => window.feed(
                &self.spectrum,
                self.output_gain,
                &self.played[start..self.position],
            ),
            Some(decoded) => {
                // the frames played so far, and the decoded frames they cover
                // go to the analysis
                decoded.played += count;
                let frames = ((decoded.played / CHANNELS) as f64 * decoded.step) as usize;
                let reached = (frames * CHANNELS).min(decoded.samples.len());
                if reached > decoded.fed {
                    window.feed(
                        &self.spectrum,
                        self.output_gain,
                        &decoded.samples[decoded.fed..reached],
                    );
                    decoded.fed = reached;
                }
            }
        }

        if self.position == self.played.len() && !self.finished {
            // whatever of the decoded chunk the played frames did not reach
            // yet is analysed with its last frame; the unfinished window goes
            // on to whichever chunk plays next
            if let Some(decoded) = &mut self.decoded {
                window.feed(
                    &self.spectrum,
                    self.output_gain,
                    &decoded.samples[decoded.fed..],
                );
                decoded.fed = decoded.samples.len();
            }
            self.finished = true;
        }
        count
    }

    fn is_done(&self) -> bool {
        self.finished
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum FftResolution {
    Bass,
    Main,
}

#[derive(Clone, Copy, Debug)]
struct BandRange {
    lower_bin: usize,
    upper_bin: usize,
    resolution: FftResolution,
}

struct SpectrumAnalyzer {
    main_fft: Arc<dyn Fft<f32>>,
    bass_fft: Arc<dyn Fft<f32>>,
    main_fft_buffer: Vec<Complex<f32>>,
    bass_fft_buffer: Vec<Complex<f32>>,
    main_fft_scratch: Vec<Complex<f32>>,
    bass_fft_scratch: Vec<Complex<f32>>,
    main_window: Vec<f32>,
    bass_window: Vec<f32>,
    main_power_scale: f32,
    bass_power_scale: f32,
    band_ranges: [BandRange; BAND_COUNT],
    input_buffer: Vec<Complex<f32>>,
    output_levels: [f32; BAND_COUNT],
    reference_amplitude: f32,
}

impl SpectrumAnalyzer {
    fn new() -> Self {
        let mut planner = FftPlanner::<f32>::new();
        let main_fft = planner.plan_fft_forward(MAIN_FFT_SIZE);
        let bass_fft = planner.plan_fft_forward(BASS_FFT_SIZE);
        let main_scratch_len = main_fft.get_inplace_scratch_len();
        let bass_scratch_len = bass_fft.get_inplace_scratch_len();
        let main_window = hann_window(MAIN_FFT_SIZE);
        let bass_window = hann_window(BASS_FFT_SIZE);

        Self {
            main_fft,
            bass_fft,
            main_fft_buffer: vec![Complex::default(); MAIN_FFT_SIZE],
            bass_fft_buffer: vec![Complex::default(); BASS_FFT_SIZE],
            main_fft_scratch: vec![Complex::default(); main_scratch_len],
            bass_fft_scratch: vec![Complex::default(); bass_scratch_len],
            main_power_scale: window_power_scale(&main_window),
            bass_power_scale: window_power_scale(&bass_window),
            main_window,
            bass_window,
            band_ranges: logarithmic_band_ranges(),
            input_buffer: vec![Complex::default(); BASS_FFT_SIZE],
            output_levels: [0.0; BAND_COUNT],
            reference_amplitude: INITIAL_REFERENCE_AMPLITUDE,
        }
    }

    #[cfg(test)]
    fn push_interleaved(&mut self, samples: &[f64], output_gain: f64) {
        let samples: Vec<_> = samples.iter().map(|sample| *sample as f32).collect();
        self.push_interleaved_f32(&samples, output_gain, 1);
    }

    fn push_interleaved_f32(&mut self, samples: &[f32], output_gain: f64, frame_skip: u64) {
        let inverse_output_gain = inverse_output_gain(output_gain);

        for (chunk_index, chunk) in samples.chunks(ANALYSIS_SAMPLE_COUNT).enumerate() {
            let frame_count = (chunk.len() / NUM_CHANNELS as usize).min(BASS_FFT_SIZE);
            if frame_count == 0 {
                continue;
            }

            let elapsed_frames = frame_count.saturating_mul(
                if chunk_index == 0 {
                    frame_skip.max(1)
                } else {
                    1
                }
                .min(BASS_FFT_SIZE as u64) as usize,
            );
            self.shift_input(elapsed_frames);
            for (index, frame) in chunk
                .as_chunks::<{ NUM_CHANNELS as usize }>()
                .0
                .iter()
                .take(frame_count)
                .enumerate()
            {
                // pack l/r into one complex fft instead of averaging them.
                // this preserves out-of-phase guitars/synths at the same fft
                // count as mono analysis. power is combined after the transform.
                self.input_buffer[frame_count - index - 1] = Complex::new(
                    analysis_sample(frame[0], inverse_output_gain),
                    analysis_sample(frame[1], inverse_output_gain),
                );
            }

            self.transform(elapsed_frames as f32 / SAMPLE_RATE as f32);
        }
    }

    fn shift_input(&mut self, frame_count: usize) {
        let frame_count = frame_count.min(BASS_FFT_SIZE);
        self.input_buffer
            .copy_within(..BASS_FFT_SIZE - frame_count, frame_count);
        // missing packets must not stretch old audio across the fft window.
        self.input_buffer[..frame_count].fill(Complex::default());
    }

    fn transform(&mut self, elapsed_seconds: f32) {
        self.run_fft();
        let amplitudes = self.band_amplitudes();
        self.update_reference_amplitude(&amplitudes, elapsed_seconds);
        self.update_levels(&amplitudes, elapsed_seconds);
    }

    fn run_fft(&mut self) {
        for index in 0..BASS_FFT_SIZE {
            self.bass_fft_buffer[index] = self.input_buffer[index] * self.bass_window[index];
        }
        for index in 0..MAIN_FFT_SIZE {
            self.main_fft_buffer[index] = self.input_buffer[index] * self.main_window[index];
        }

        self.bass_fft
            .process_with_scratch(&mut self.bass_fft_buffer, &mut self.bass_fft_scratch);
        self.main_fft
            .process_with_scratch(&mut self.main_fft_buffer, &mut self.main_fft_scratch);
    }

    fn band_amplitudes(&self) -> [f32; BAND_COUNT] {
        let mut amplitudes = [0.0_f32; BAND_COUNT];
        for (band, range) in self.band_ranges.iter().enumerate() {
            let (fft, power_scale) = match range.resolution {
                FftResolution::Bass => (&self.bass_fft_buffer, self.bass_power_scale),
                FftResolution::Main => (&self.main_fft_buffer, self.main_power_scale),
            };
            // for z = fft(l + ir), (|z[k]|² + |z[n-k]|²) / 4 is
            // the mean l/r power. it cannot cancel with stereo phase changes.
            let power = (range.lower_bin..=range.upper_bin)
                .map(|bin| (fft[bin].norm_sqr() + fft[fft.len() - bin].norm_sqr()) * 0.25)
                .sum::<f32>();
            amplitudes[band] = (power * power_scale).sqrt();
        }
        amplitudes
    }

    fn update_reference_amplitude(&mut self, amplitudes: &[f32; BAND_COUNT], elapsed_seconds: f32) {
        // one slow, bounded gain reference preserves balance between bands and
        // does not normalize each hit back to the same height. no gain ramp in
        // silence, and no feedback from smoothed/displayed output into gain.
        let strongest = amplitudes.iter().copied().fold(0.0_f32, f32::max);
        if strongest > 0.005 {
            let target_reference = strongest.max(MIN_REFERENCE_AMPLITUDE);
            let time_constant = if target_reference > self.reference_amplitude {
                0.35
            } else {
                3.0
            };
            self.reference_amplitude += (target_reference - self.reference_amplitude)
                * (1.0 - (-elapsed_seconds / time_constant).exp());
        }
    }

    fn update_levels(&mut self, amplitudes: &[f32; BAND_COUNT], elapsed_seconds: f32) {
        let attack = 1.0 - (-elapsed_seconds / ATTACK_SECONDS).exp();
        let release = 1.0 - (-elapsed_seconds / RELEASE_SECONDS).exp();

        for (band, amplitude) in amplitudes.iter().copied().enumerate() {
            let amplitude = (amplitude - 0.0001).max(0.0);
            let relative = amplitude / self.reference_amplitude;
            // a soft shoulder keeps loud bands moving instead of hard
            // clipping, while leaving some room for accents above a sustained
            // level. unlike hard clipping, loud bands keep moving.
            let target = 1.0 - (-COMPRESSION_KNEE * relative.powf(0.85)).exp();
            let previous = self.output_levels[band];
            self.output_levels[band] +=
                (target - previous) * if target > previous { attack } else { release };
            if self.output_levels[band] < 0.0001 {
                self.output_levels[band] = 0.0;
            }
        }
    }

    fn levels(&self) -> [f32; BAND_COUNT] {
        self.output_levels
    }

    fn clear(&mut self) {
        self.input_buffer.fill(Complex::default());
        self.output_levels.fill(0.0);
        self.reference_amplitude = INITIAL_REFERENCE_AMPLITUDE;
    }
}

fn inverse_output_gain(output_gain: f64) -> f32 {
    // librespot applies soft volume before packets reach the audio sink. undo it
    // so changing listening volume does not flatten the visualizer.
    if output_gain.is_finite() && output_gain > 1.0e-4 {
        output_gain.recip() as f32
    } else {
        1.0
    }
}

fn analysis_sample(sample: f32, gain: f32) -> f32 {
    let value = sample * gain;
    // defend the analyzer only. never modify pcm sent to the audio device.
    if value.is_finite() {
        value.clamp(-16.0, 16.0)
    } else {
        0.0
    }
}

fn window_power_scale(window: &[f32]) -> f32 {
    // converts one-sided fft power to sine-equivalent amplitude, accounting
    // for window energy and fft length (including the longer bass window).
    4.0 / (window.len() as f32 * window.iter().map(|value| value * value).sum::<f32>())
}

fn hann_window(size: usize) -> Vec<f32> {
    (0..size)
        .map(|index| 0.5 * (1.0 - (TAU * index as f32 / (size - 1) as f32).cos()))
        .collect()
}

fn logarithmic_band_ranges() -> [BandRange; BAND_COUNT] {
    let mut lower_bins = [0; BAND_COUNT + 1];
    let mut upper_bins = [0; BAND_COUNT];
    let mut cutoff_frequencies = [0.0; BAND_COUNT + 1];
    let frequency_constant =
        (MIN_FREQUENCY_HZ / MAX_FREQUENCY_HZ).log10() / (1.0 / (BAND_COUNT + 1) as f32 - 1.0);
    let minimum_bandwidth = SAMPLE_RATE as f32 / BASS_FFT_SIZE as f32;
    let mut bass_cutoff_band = 0;
    let mut first_bar = true;

    for edge in 0..=BAND_COUNT {
        let coefficient =
            -frequency_constant + (edge + 1) as f32 / (BAND_COUNT + 1) as f32 * frequency_constant;
        cutoff_frequencies[edge] = MAX_FREQUENCY_HZ * 10.0_f32.powf(coefficient);
        if edge > 0 && cutoff_frequencies[edge - 1] >= cutoff_frequencies[edge] {
            cutoff_frequencies[edge] = cutoff_frequencies[edge - 1] + minimum_bandwidth;
        }

        let relative_cutoff = cutoff_frequencies[edge] / (SAMPLE_RATE as f32 / 2.0);
        if cutoff_frequencies[edge] < BASS_CUTOFF_HZ {
            lower_bins[edge] = (relative_cutoff * (BASS_FFT_SIZE / 2) as f32) as usize;
            lower_bins[edge] = lower_bins[edge].min(BASS_FFT_SIZE / 2);
            bass_cutoff_band += 1;
            if bass_cutoff_band > 1 {
                first_bar = false;
            }
        } else {
            lower_bins[edge] = (relative_cutoff * (MAIN_FFT_SIZE / 2) as f32).ceil() as usize;
            lower_bins[edge] = lower_bins[edge].min(MAIN_FFT_SIZE / 2);
            if edge == bass_cutoff_band {
                first_bar = true;
                if edge > 0 {
                    upper_bins[edge - 1] =
                        ((relative_cutoff * (BASS_FFT_SIZE / 2) as f32) as usize).saturating_sub(1);
                }
            } else {
                first_bar = false;
            }
        }

        if edge > 0 {
            if !first_bar {
                upper_bins[edge - 1] = lower_bins[edge].saturating_sub(1);
                if lower_bins[edge] <= lower_bins[edge - 1] {
                    let maximum_bin = if edge < bass_cutoff_band {
                        BASS_FFT_SIZE / 2
                    } else {
                        MAIN_FFT_SIZE / 2
                    };
                    if lower_bins[edge - 1] < maximum_bin {
                        lower_bins[edge] = lower_bins[edge - 1] + 1;
                        upper_bins[edge - 1] = lower_bins[edge] - 1;
                    }
                }
            } else if upper_bins[edge - 1] < lower_bins[edge - 1] {
                upper_bins[edge - 1] = lower_bins[edge - 1] + 1;
            }
        }

        let fft_size = if edge < bass_cutoff_band {
            BASS_FFT_SIZE
        } else {
            MAIN_FFT_SIZE
        };
        cutoff_frequencies[edge] =
            lower_bins[edge] as f32 / (fft_size / 2) as f32 * (SAMPLE_RATE as f32 / 2.0);
    }

    std::array::from_fn(|band| BandRange {
        lower_bin: lower_bins[band],
        upper_bin: upper_bins[band],
        resolution: if band < bass_cutoff_band {
            FftResolution::Bass
        } else {
            FftResolution::Main
        },
    })
}

#[cfg(test)]
mod tests {
    use super::{
        ANALYSIS_SAMPLE_COUNT, AnalyzedSource, BAND_COUNT, BASS_FFT_SIZE, FftResolution, HOP_SIZE,
        MAIN_FFT_SIZE, NUM_CHANNELS, Ordering, PlayQueue, PlayedChunk, RateConverter, SAMPLE_RATE,
        SharedSpectrum, SpectrumAnalyzer, SpectrumSink,
    };

    fn band_for_frequency(analyzer: &SpectrumAnalyzer, frequency: f32) -> usize {
        analyzer
            .band_ranges
            .iter()
            .position(|range| {
                let fft_size = match range.resolution {
                    FftResolution::Bass => BASS_FFT_SIZE,
                    FftResolution::Main => MAIN_FFT_SIZE,
                };
                let bin = (frequency * fft_size as f32 / SAMPLE_RATE as f32).round() as usize;
                (range.lower_bin..=range.upper_bin).contains(&bin)
            })
            .expect("frequency should be covered by the visualizer")
    }

    #[test]
    fn places_a_tone_in_its_frequency_band() {
        let mut analyzer = SpectrumAnalyzer::new();
        let frequency = 440.0;
        let frame_count = SAMPLE_RATE as usize * 2;
        let mut stereo_samples = Vec::with_capacity(frame_count * 2);

        for index in 0..frame_count {
            let sample =
                (std::f64::consts::TAU * frequency * index as f64 / SAMPLE_RATE as f64).sin() * 0.7;
            stereo_samples.extend([sample, sample]);
        }

        analyzer.push_interleaved(&stereo_samples, 1.0);
        let levels = analyzer.levels();
        let expected_band = band_for_frequency(&analyzer, frequency as f32);
        let strongest_band = levels
            .iter()
            .enumerate()
            .max_by(|(_, left), (_, right)| left.total_cmp(right))
            .map(|(index, _)| index);

        assert!(
            strongest_band.is_some_and(|band| band.abs_diff(expected_band) <= 1),
            "expected {expected_band}, got {strongest_band:?}: {levels:?}"
        );
        assert!(levels[expected_band] > 0.2);
    }

    /// the time stretcher hands the sink chunks shorter than one analysis window above 1x
    /// speed.
    #[test]
    fn analysis_windows_span_chunks_too_short_to_hold_one() {
        let spectrum = SharedSpectrum::new();
        spectrum.set_enabled(true);
        let before = spectrum.sequence.load(Ordering::Relaxed);
        let quarter = ANALYSIS_SAMPLE_COUNT / 4;
        let queue = PlayQueue::default();
        let mut spent = Vec::new();
        let mut block = vec![0.0_f32; 256];

        for chunk in 1..=4 {
            let source = AnalyzedSource::new(vec![0.4; quarter], spectrum.clone(), 1.0);
            queue.push(PlayedChunk::Analyzed(Box::new(source)), &mut spent);
            while queue.len() > 0 {
                queue.fill(&mut block);
            }
            assert_eq!(
                spectrum.sequence.load(Ordering::Relaxed) - before,
                u64::from(chunk == 4),
                "a quarter of a window should be carried to the next chunk, not discarded"
            );
        }
    }

    #[test]
    fn silence_produces_no_visible_bands() {
        let mut analyzer = SpectrumAnalyzer::new();
        analyzer.push_interleaved(&vec![0.0; MAIN_FFT_SIZE * 2], 1.0);

        assert_eq!(analyzer.levels(), [0.0; BAND_COUNT]);
    }

    #[test]
    fn skipped_packets_age_out_old_audio_and_advance_release() {
        let mut analyzer = SpectrumAnalyzer::new();
        for chunk in 0..20 {
            let samples = signal_chunk(chunk, |time| {
                let sample = (std::f64::consts::TAU * 440.0 * time).sin() * 0.7;
                (sample, sample)
            });
            analyzer.push_interleaved_f32(&samples, 1.0, 1);
        }
        let before = analyzer.levels().iter().copied().fold(0.0_f32, f32::max);
        assert!(before > 0.5);
        let silence = vec![0.0_f32; ANALYSIS_SAMPLE_COUNT];
        analyzer.push_interleaved_f32(&silence, 1.0, 8);
        let after = analyzer.levels().iter().copied().fold(0.0_f32, f32::max);
        assert!(after < before * 0.1, "{before} -> {after}");
        analyzer.push_interleaved_f32(&silence, 1.0, u64::MAX);
        assert!(analyzer.levels().iter().all(|level| level.is_finite()));
        assert!(analyzer.levels().iter().all(|level| *level < 0.0001));
    }

    #[test]
    fn bass_energy_retains_visual_headroom() {
        let mut analyzer = SpectrumAnalyzer::new();
        let frame_count = SAMPLE_RATE as usize;
        let mut samples = Vec::with_capacity(frame_count * 2);

        for index in 0..frame_count {
            let sample =
                (std::f64::consts::TAU * 80.0 * index as f64 / SAMPLE_RATE as f64).sin() * 0.25;
            samples.extend([sample, sample]);
        }

        let bass_band = band_for_frequency(&analyzer, 80.0);
        let mut response = Vec::new();
        for chunk in samples.chunks(ANALYSIS_SAMPLE_COUNT) {
            analyzer.push_interleaved(chunk, 1.0);
            response.push(analyzer.levels()[bass_band]);
        }
        let levels = analyzer.levels();

        assert!(
            response.iter().any(|level| (0.25..0.9).contains(level)),
            "{response:?}"
        );
        assert!(
            (0.62..0.92).contains(&levels[bass_band]),
            "steady bass needs headroom: {levels:?}"
        );
        assert!(
            levels[bass_band]
                > levels
                    .iter()
                    .enumerate()
                    .filter(|(band, _)| band.abs_diff(bass_band) > 2)
                    .map(|(_, level)| *level)
                    .fold(0.0, f32::max)
        );
    }

    #[test]
    fn loud_distorted_audio_keeps_smoothing_state_bounded() {
        let mut analyzer = SpectrumAnalyzer::new();

        for chunk_index in 0..90 {
            let mut samples = Vec::with_capacity(ANALYSIS_SAMPLE_COUNT);
            for frame in 0..HOP_SIZE {
                let index = chunk_index * HOP_SIZE + frame;
                let time = index as f64 / SAMPLE_RATE as f64;
                let dense_signal = (std::f64::consts::TAU * 73.0 * time).sin() * 0.72
                    + (std::f64::consts::TAU * 191.0 * time).sin() * 0.58
                    + (std::f64::consts::TAU * 863.0 * time).sin() * 0.42
                    + (std::f64::consts::TAU * 3_217.0 * time).sin() * 0.3;
                let sample = (dense_signal * 2.4).clamp(-1.0, 1.0);
                samples.extend([sample, sample]);
            }

            analyzer.push_interleaved(&samples, 1.0);
            assert!(
                analyzer
                    .output_levels
                    .iter()
                    .all(|level| (0.0..=1.0).contains(level))
            );
            assert!(analyzer.reference_amplitude.is_finite());
        }
    }

    #[test]
    fn amplitude_changes_create_smooth_visible_motion() {
        let mut analyzer = SpectrumAnalyzer::new();
        let frame_count = SAMPLE_RATE as usize;
        let mut samples = Vec::with_capacity(frame_count * 2);

        for index in 0..frame_count {
            let time = index as f64 / SAMPLE_RATE as f64;
            let envelope = 0.08 + 0.42 * (0.5 + 0.5 * (std::f64::consts::TAU * 4.0 * time).sin());
            let sample = (std::f64::consts::TAU * 440.0 * time).sin() * envelope;
            samples.extend([sample, sample]);
        }

        let motion_band = band_for_frequency(&analyzer, 440.0);
        let mut motion = Vec::new();
        for chunk in samples.chunks(HOP_SIZE * 2) {
            analyzer.push_interleaved(chunk, 1.0);
            motion.push(analyzer.levels()[motion_band]);
        }

        let settled_motion = &motion[10..];
        let minimum = settled_motion.iter().copied().fold(f32::INFINITY, f32::min);
        let maximum = settled_motion
            .iter()
            .copied()
            .fold(f32::NEG_INFINITY, f32::max);
        let largest_step = settled_motion
            .windows(2)
            .map(|pair| (pair[1] - pair[0]).abs())
            .fold(0.0_f32, f32::max);

        assert!(maximum - minimum > 0.22, "{motion:?}");
        assert!(largest_step > 0.04, "{motion:?}");
        assert!(largest_step < 0.22, "{motion:?}");
    }

    #[test]
    fn clear_discards_prior_audio() {
        let mut analyzer = SpectrumAnalyzer::new();
        let mut samples = Vec::with_capacity(MAIN_FFT_SIZE * 2);
        for index in 0..MAIN_FFT_SIZE {
            let sample =
                (std::f64::consts::TAU * 440.0 * index as f64 / SAMPLE_RATE as f64).sin() * 0.5;
            samples.extend([sample, sample]);
        }
        analyzer.push_interleaved(&samples, 1.0);
        assert!(analyzer.levels().iter().any(|level| *level > 0.0));

        analyzer.clear();

        assert_eq!(analyzer.levels(), [0.0; BAND_COUNT]);
    }

    #[test]
    fn analysis_is_independent_of_output_volume() {
        let mut full_volume = SpectrumAnalyzer::new();
        let mut low_volume = SpectrumAnalyzer::new();
        let mut full_samples = Vec::with_capacity(MAIN_FFT_SIZE * 2);
        let mut quiet_samples = Vec::with_capacity(MAIN_FFT_SIZE * 2);

        for index in 0..MAIN_FFT_SIZE {
            let sample =
                (std::f64::consts::TAU * 220.0 * index as f64 / SAMPLE_RATE as f64).sin() * 0.55;
            full_samples.extend([sample, sample]);
            quiet_samples.extend([sample * 0.08, sample * 0.08]);
        }

        full_volume.push_interleaved(&full_samples, 1.0);
        low_volume.push_interleaved(&quiet_samples, 0.08);

        let full_levels = full_volume.levels();
        let quiet_levels = low_volume.levels();
        for band in 0..BAND_COUNT {
            assert!((full_levels[band] - quiet_levels[band]).abs() < 0.015);
        }
    }

    #[test]
    fn bass_transient_punches_above_the_other_bands() {
        let mut analyzer = SpectrumAnalyzer::new();
        let chunk_frames = HOP_SIZE;
        let bass_band = band_for_frequency(&analyzer, 75.0);
        let bed_band = band_for_frequency(&analyzer, 1_200.0);

        let kick_chunk = 20;
        let mut baseline = [0.0; BAND_COUNT];
        let mut kick_levels = [0.0; BAND_COUNT];
        for chunk_index in 0..30 {
            let mut samples = Vec::with_capacity(chunk_frames * 2);
            for frame in 0..chunk_frames {
                let index = chunk_index * chunk_frames + frame;
                let time = index as f64 / SAMPLE_RATE as f64;
                let bed = (std::f64::consts::TAU * 1_200.0 * time).sin() * 0.08;
                let kick = if chunk_index == kick_chunk {
                    (std::f64::consts::TAU * 75.0 * time).sin() * 0.85
                } else {
                    0.0
                };
                samples.extend([bed + kick, bed + kick]);
            }
            analyzer.push_interleaved(&samples, 1.0);
            if chunk_index + 1 == kick_chunk {
                baseline = analyzer.levels();
            } else if (kick_chunk..kick_chunk + 8).contains(&chunk_index)
                && analyzer.levels()[bass_band] > kick_levels[bass_band]
            {
                kick_levels = analyzer.levels();
            }
        }

        assert!(kick_levels[bass_band] > 0.68, "{kick_levels:?}");
        assert!(
            kick_levels[bass_band] > baseline[bass_band] + 0.18,
            "baseline {baseline:?}, peak {kick_levels:?}"
        );
        let bass_rise = kick_levels[bass_band] - baseline[bass_band];
        let bed_rise = (kick_levels[bed_band] - baseline[bed_band]).max(0.0);
        assert!(
            bass_rise > bed_rise + 0.18,
            "baseline {baseline:?}, peak {kick_levels:?}"
        );
    }

    #[test]
    fn separated_tones_light_independent_bands() {
        let mut analyzer = SpectrumAnalyzer::new();
        let low_frequency = 90.0;
        let high_frequency = 4_200.0;
        let mut stereo_samples = Vec::with_capacity(BASS_FFT_SIZE * 2);

        for index in 0..BASS_FFT_SIZE {
            let time = index as f64 / SAMPLE_RATE as f64;
            let low = (std::f64::consts::TAU * low_frequency * time).sin() * 0.55;
            let high = (std::f64::consts::TAU * high_frequency * time).sin() * 0.45;
            stereo_samples.extend([low + high, low + high]);
        }

        analyzer.push_interleaved(&stereo_samples, 1.0);
        let levels = analyzer.levels();
        let low_band = band_for_frequency(&analyzer, low_frequency as f32);
        let high_band = band_for_frequency(&analyzer, high_frequency as f32);

        assert!(low_band.abs_diff(high_band) > 20);
        assert!(levels[low_band] > 0.2, "{levels:?}");
        assert!(levels[high_band] > 0.2, "{levels:?}");
    }

    #[test]
    fn sub_100_hz_tone_uses_the_finer_bass_fft() {
        let mut analyzer = SpectrumAnalyzer::new();
        let frequency = 73.0;
        let expected_band = band_for_frequency(&analyzer, frequency);
        let expected_range = analyzer.band_ranges[expected_band];
        assert_eq!(expected_range.resolution, FftResolution::Bass);
        assert!(
            SAMPLE_RATE as f32 / (BASS_FFT_SIZE as f32)
                < SAMPLE_RATE as f32 / (MAIN_FFT_SIZE as f32)
        );

        let frame_count = SAMPLE_RATE as usize * 2;
        let mut stereo_samples = Vec::with_capacity(frame_count * 2);
        for index in 0..frame_count {
            let sample = (std::f64::consts::TAU * frequency as f64 * index as f64
                / SAMPLE_RATE as f64)
                .sin()
                * 0.45;
            stereo_samples.extend([sample, sample]);
        }

        analyzer.push_interleaved(&stereo_samples, 1.0);
        let levels = analyzer.levels();
        let strongest_band = levels
            .iter()
            .enumerate()
            .max_by(|(_, left), (_, right)| left.total_cmp(right))
            .map(|(index, _)| index);

        assert_eq!(strongest_band, Some(expected_band), "{levels:?}");
        assert!(levels[expected_band] > 0.25, "{levels:?}");
    }

    fn signal_chunk(chunk: usize, sample: impl Fn(f64) -> (f64, f64)) -> Vec<f32> {
        (0..HOP_SIZE)
            .flat_map(|frame| {
                let time = (chunk * HOP_SIZE + frame) as f64 / SAMPLE_RATE as f64;
                let (left, right) = sample(time);
                [left as f32, right as f32]
            })
            .collect()
    }

    fn distorted_chord(time: f64) -> f64 {
        let tau = std::f64::consts::TAU;
        let chord = (tau * 73.0 * time).sin() * 0.72
            + (tau * 191.0 * time).sin() * 0.58
            + (tau * 863.0 * time).sin() * 0.42
            + (tau * 3_217.0 * time).sin() * 0.3;
        (chord * 4.0).clamp(-1.0, 1.0)
    }

    #[test]
    fn distorted_chord_keeps_following_its_envelope_after_settling() {
        let mut analyzer = SpectrumAnalyzer::new();
        let bass_band = band_for_frequency(&analyzer, 73.0);
        let lead_band = band_for_frequency(&analyzer, 863.0);
        let mut history = Vec::new();
        for chunk in 0..430 {
            let samples = signal_chunk(chunk, |time| {
                let envelope =
                    0.18 + 0.62 * (0.5 + 0.5 * (std::f64::consts::TAU * 3.0 * time).sin());
                let sample = distorted_chord(time) * envelope;
                (sample, sample)
            });
            analyzer.push_interleaved_f32(&samples, 1.0, 1);
            if chunk > 86 {
                history.push(analyzer.levels());
            }
        }
        for band in [bass_band, lead_band] {
            let low = history
                .iter()
                .map(|levels| levels[band])
                .fold(1.0_f32, f32::min);
            let high = history
                .iter()
                .map(|levels| levels[band])
                .fold(0.0_f32, f32::max);
            let pinned = history.iter().filter(|levels| levels[band] >= 0.98).count();
            println!(
                "distorted band {band}: range={:.3}, pinned={pinned}/{}",
                high - low,
                history.len()
            );
            assert!(high - low > 0.25, "band {band}: {low}..{high}");
            assert!(
                pinned < history.len() / 20,
                "band {band} pinned {pinned} times"
            );
        }
    }

    #[test]
    fn phase_inverted_stereo_preserves_the_same_spectral_energy() {
        let mut in_phase = SpectrumAnalyzer::new();
        let mut inverted = SpectrumAnalyzer::new();
        for chunk in 0..90 {
            let samples = signal_chunk(chunk, |time| {
                let sample = distorted_chord(time) * 0.7;
                (sample, sample)
            });
            let opposite: Vec<_> = samples
                .as_chunks::<2>()
                .0
                .iter()
                .flat_map(|frame| [frame[0], -frame[1]])
                .collect();
            in_phase.push_interleaved_f32(&samples, 1.0, 1);
            inverted.push_interleaved_f32(&opposite, 1.0, 1);
        }
        assert!(in_phase.levels().iter().any(|level| *level > 0.3));
        for (left, right) in in_phase.levels().iter().zip(inverted.levels()) {
            assert!(
                (left - right).abs() < 0.002,
                "in phase {left}, inverted {right}"
            );
        }
    }

    #[test]
    fn loud_audio_releases_to_silence_and_invalid_pcm_cannot_poison_the_fft() {
        let mut analyzer = SpectrumAnalyzer::new();
        for chunk in 0..90 {
            let samples = signal_chunk(chunk, |time| {
                let sample = distorted_chord(time);
                (sample, sample)
            });
            analyzer.push_interleaved_f32(&samples, 1.0, 1);
        }
        assert!(analyzer.levels().iter().any(|level| *level > 0.5));
        let invalid: Vec<_> = (0..ANALYSIS_SAMPLE_COUNT)
            .map(|index| {
                if index % 2 == 0 {
                    f32::NAN
                } else {
                    f32::INFINITY
                }
            })
            .collect();
        analyzer.push_interleaved_f32(&invalid, f64::NAN, 1);
        let silence = vec![0.0_f32; ANALYSIS_SAMPLE_COUNT];
        for _ in 0..32 {
            analyzer.push_interleaved_f32(&silence, 1.0, 1);
            assert!(analyzer.levels().iter().all(|level| level.is_finite()));
        }
        assert!(analyzer.levels().iter().all(|level| *level < 0.001));
        for chunk in 0..8 {
            let samples = signal_chunk(chunk, |time| {
                let sample = (std::f64::consts::TAU * 863.0 * time).sin() * 0.7;
                (sample, sample)
            });
            analyzer.push_interleaved_f32(&samples, 1.0, 1);
        }
        assert!(analyzer.levels()[band_for_frequency(&analyzer, 863.0)] > 0.5);
    }

    #[test]
    fn repeated_bass_and_midrange_hits_recover_on_a_distorted_bed() {
        for frequency in [75.0, 863.0, 3_217.0] {
            let mut analyzer = SpectrumAnalyzer::new();
            let band = band_for_frequency(&analyzer, frequency as f32);
            let mut baseline = 0.0_f32;
            let mut peak = 0.0_f32;
            for chunk in 0..350 {
                let samples = signal_chunk(chunk, |time| {
                    let pulse_time = time % 0.75;
                    let envelope = if pulse_time < 0.12 {
                        (1.0 - pulse_time / 0.12).powi(2)
                    } else {
                        0.0
                    };
                    let sample = distorted_chord(time) * 0.16
                        + (std::f64::consts::TAU * frequency * time).sin() * 0.75 * envelope;
                    (sample, sample)
                });
                analyzer.push_interleaved_f32(&samples, 1.0, 1);
                let pulse_time = ((chunk + 1) * HOP_SIZE) as f64 / SAMPLE_RATE as f64 % 0.75;
                if chunk > 86 {
                    if pulse_time > 0.55 {
                        baseline = baseline.max(analyzer.levels()[band]);
                    } else if pulse_time < 0.25 {
                        peak = peak.max(analyzer.levels()[band]);
                    }
                }
            }
            println!("{frequency} Hz hits: baseline={baseline:.3}, peak={peak:.3}");
            assert!(peak - baseline > 0.25, "{frequency} Hz: {baseline}..{peak}");
        }
    }

    /// how many analysis windows reach the worker when sources are built the
    /// way the sink builds them: up to a queue's worth ahead of playback, then
    /// played and dropped in order.
    fn windows_queued_through_a_sink_queue(packet_samples: &[usize]) -> u64 {
        let spectrum = SharedSpectrum::new();
        spectrum.set_enabled(true);
        let before = spectrum.sequence.load(Ordering::Relaxed);
        let queue = PlayQueue::default();
        let mut spent = Vec::new();
        let mut packets = packet_samples.iter();
        // a device's block of 512 frames
        let mut block = vec![0.0_f32; 1_024];
        loop {
            // the player thread refills a drained queue in one burst.
            if queue.len() <= SpectrumSink::QUEUE_LOW_WATER {
                while queue.len() <= SpectrumSink::QUEUE_HIGH_WATER {
                    let Some(&length) = packets.next() else { break };
                    let source = AnalyzedSource::new(vec![0.25; length], spectrum.clone(), 1.0);
                    queue.push(PlayedChunk::Analyzed(Box::new(source)), &mut spent);
                }
            }
            if queue.len() == 0 {
                break;
            }
            queue.fill(&mut block);
        }
        spectrum.sequence.load(Ordering::Relaxed) - before
    }

    /// vorbis decodes a transient as a run of short blocks: 128-frame packets,
    /// a quarter of the 1024-frame packets around them.
    #[test]
    fn short_packets_queued_ahead_still_fill_analysis_windows() {
        let frames = |count: usize| count * NUM_CHANNELS as usize;
        // a long run of short blocks, then drums: long blocks broken by
        // transients, each entered and left through a 576-frame packet.
        let short_run = vec![frames(128); 800];
        let drums: Vec<usize> = [1024, 1024, 576, 128, 128, 128, 576, 1024]
            .iter()
            .cycle()
            .take(800)
            .map(|&count| frames(count))
            .collect();

        for packets in [short_run, drums] {
            let expected = (packets.iter().sum::<usize>() / ANALYSIS_SAMPLE_COUNT) as u64;
            let queued = windows_queued_through_a_sink_queue(&packets);
            assert_eq!(
                queued, expected,
                "{queued} of {expected} analysis windows reached the analyzer"
            );
        }
    }

    /// librespot's packets brought to a 48 khz device: a tone comes out as the
    /// same tone sampled at 48 khz, with nothing lost or doubled where one
    /// packet ends and the next begins.
    #[test]
    fn rate_conversion_carries_across_packets() {
        let frequency = 1_000.0_f64;
        let tone = |time: f64| (std::f64::consts::TAU * frequency * time).sin() as f32;
        let sizes = [1_024_usize, 576, 128, 128, 128, 576, 1_024, 1, 2, 1_024];
        let mut converter = RateConverter::new(48_000);
        let mut played = Vec::new();
        let mut decoded_frames = 0_usize;
        for _ in 0..20 {
            for &size in &sizes {
                let packet: Vec<f32> = (decoded_frames..decoded_frames + size)
                    .flat_map(|frame| {
                        let sample = tone(frame as f64 / f64::from(SAMPLE_RATE));
                        [sample, -sample]
                    })
                    .collect();
                decoded_frames += size;
                played.extend(converter.convert(&packet));
            }
        }
        let played_frames = played.len() / 2;
        let expected_frames = decoded_frames as f64 * 48_000.0 / f64::from(SAMPLE_RATE);
        assert!(
            (played_frames as f64 - expected_frames).abs() <= 2.0,
            "{played_frames} frames for {expected_frames:.1}"
        );
        let (frames, _) = played.as_chunks::<2>();
        let worst = frames
            .iter()
            .enumerate()
            .map(|(frame, pair)| {
                let expected = tone(frame as f64 / 48_000.0);
                (pair[0] - expected).abs().max((pair[1] + expected).abs())
            })
            .fold(0.0_f32, f32::max);
        // linear interpolation of a 1 khz tone at 44.1 khz is off by at most
        // a quarter of a percent; a step at a packet's edge is far more, and
        // rodio's own conversion of the same packets drifts a whole wave away
        assert!(worst < 0.005, "off by up to {worst}");
    }

    /// a new track starts afresh: nothing of the last one is interpolated
    /// into its first frames
    #[test]
    fn rate_conversion_resets_for_a_new_track() {
        let mut converter = RateConverter::new(48_000);
        converter.convert(&[0.5; 2_048]);
        converter.reset();
        let played = converter.convert(&[-0.25; 2_048]);
        assert!(
            played.iter().all(|&sample| (sample + 0.25).abs() < 1e-6),
            "the last track's samples leaked into the next"
        );
    }

    /// chunks played at a device's rate still hand the analysis every sample
    /// librespot decoded, in windows of the decoder's rate
    #[test]
    fn converted_chunks_fill_the_same_analysis_windows() {
        let frames = |count: usize| count * NUM_CHANNELS as usize;
        let packets: Vec<usize> = [1_024, 1_024, 576, 128, 128, 128, 576, 1_024]
            .iter()
            .cycle()
            .take(400)
            .map(|&count| frames(count))
            .collect();
        let spectrum = SharedSpectrum::new();
        spectrum.set_enabled(true);
        let before = spectrum.sequence.load(Ordering::Relaxed);
        let mut converter = RateConverter::new(48_000);
        let queue = PlayQueue::default();
        let mut spent = Vec::new();
        let mut block = vec![0.0_f32; 1_024];
        for &length in &packets {
            let decoded = vec![0.25_f32; length];
            let played = converter.convert(&decoded);
            let source =
                AnalyzedSource::played_at(decoded, Some((played, 48_000)), spectrum.clone(), 1.0);
            queue.push(PlayedChunk::Analyzed(Box::new(source)), &mut spent);
            while queue.len() > 0 {
                queue.fill(&mut block);
            }
        }
        let queued = spectrum.sequence.load(Ordering::Relaxed) - before;
        let expected = (packets.iter().sum::<usize>() / ANALYSIS_SAMPLE_COUNT) as u64;
        assert_eq!(queued, expected, "{queued} of {expected} analysis windows");
    }

    /// the device takes the queue a block at a time rather than a sample at
    /// a time: what it plays is the chunks end to end, and the analysis still
    /// gets every window of what was decoded
    #[test]
    fn blocks_played_from_the_queue_are_the_chunks_end_to_end() {
        let frames = |count: usize| count * NUM_CHANNELS as usize;
        let packets: Vec<usize> = [1_024, 576, 128, 128, 1_024]
            .iter()
            .cycle()
            .take(200)
            .map(|&count| frames(count))
            .collect();
        let spectrum = SharedSpectrum::new();
        spectrum.set_enabled(true);
        let before = spectrum.sequence.load(Ordering::Relaxed);
        let queue = PlayQueue::default();
        let mut converter = RateConverter::new(48_000);
        let mut expected_played = Vec::new();
        let mut spent = Vec::new();
        for (index, &length) in packets.iter().enumerate() {
            let decoded: Vec<f32> = (0..length)
                .map(|sample| ((index * 7 + sample) % 97) as f32 / 97.0)
                .collect();
            let played = converter.convert(&decoded);
            expected_played.extend_from_slice(&played);
            let chunk = if index % 3 == 0 {
                PlayedChunk::Plain {
                    samples: played,
                    position: 0,
                }
            } else {
                PlayedChunk::Analyzed(Box::new(AnalyzedSource::played_at(
                    decoded,
                    Some((played, 48_000)),
                    spectrum.clone(),
                    1.0,
                )))
            };
            queue.push(chunk, &mut spent);
            spent.clear();
        }
        let mut played = Vec::new();
        let mut block = vec![0.0_f32; frames(470)];
        while queue.len() > 0 {
            queue.fill(&mut block);
            played.extend_from_slice(&block);
        }
        assert_eq!(&played[..expected_played.len()], &expected_played[..]);
        assert!(
            played[expected_played.len()..]
                .iter()
                .all(|&sample| sample == 0.0),
            "silence after"
        );
        assert!(
            spectrum.sequence.load(Ordering::Relaxed) > before,
            "the analysis was fed"
        );
    }

    #[test]
    #[ignore = "opt-in Release analyzer timing; not an app CPU or frame-rate measurement"]
    fn spectrum_worker_timing() {
        let mut analyzer = SpectrumAnalyzer::new();
        let samples = signal_chunk(0, |time| {
            let sample = distorted_chord(time);
            (sample, -sample * 0.8)
        });
        let started = std::time::Instant::now();
        for _ in 0..4_300 {
            analyzer.push_interleaved_f32(std::hint::black_box(&samples), 1.0, 1);
            std::hint::black_box(analyzer.levels());
        }
        println!(
            "analyzer: {:.1} microseconds/hop over 4300 loud stereo hops",
            started.elapsed().as_secs_f64() * 1_000_000.0 / 4_300.0
        );
    }
}
