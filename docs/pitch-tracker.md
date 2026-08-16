# Pitch tracker design

This document describes the current tracked-pitch implementation as it exists in
`PitchTracker`, `TrackedPitch`, and `SpectrogramWidget`. It is a baseline for
listening tests, not a claim that the current settings are optimal for every
instrument or musical style.

The tracker complements the FFT rather than replacing it. The FFT remains the
high-resolution greyscale view of transients, noise, harmonics, and timbre. The
pitch tracker produces a much smaller logarithmic field intended to identify
stable fundamentals for colour and note annotations. It does not perform key,
chord, score, or instrument recognition.

## Signal flow

For each analysis hop, the tracker performs the following operations:

1. Downmixed mono audio passes through a first-order 20 Hz DC blocker.
2. A bank of logarithmically spaced complex resonators integrates the signal.
3. Resonator magnitudes are smoothed across adjacent frequency bins using
   weights `0.25, 0.5, 0.25`.
4. Local maxima are scored relative to the adaptive signal level, median
   spectral floor, neighbouring bins, and current time-domain input peak.
5. Low-frequency candidates are accepted first. Higher candidates close to
   integer harmonics 2 through 8 of an accepted lower peak are rejected.
6. The remaining peaks are associated with persistent note tracks. Track
   frequency and strength use preset-dependent attack, release, and following.
7. Active tracks are rendered into a 256-bin absolute log-frequency field. The
   field contains confidence-like strengths in the range 0 to 1.
8. The UI finds local maxima in that field, interpolates their positions, and
   converts them to frequency, nearest twelve-tone equal-tempered MIDI note,
   and cents relative to that note.

The published field uses the maximum contribution at each output bin. It is not
a collection of independently addressable voices. Two broad or nearby tracks
can therefore merge into one local maximum during UI extraction.

## Frequency grid and tuning

The analysis bank has 24 bins per octave, or one bin every 50 cents, across six
octaves. Its frequency for analysis bin `b` is:

```text
f(b) = concertA / 8 * 2^(b / 24)
```

At the default `concertA = 440 Hz`, the useful range is approximately A1 to A7:
55 Hz up to just below 3520 Hz. Changing concert A scales the whole grid. The
public tuning control accepts 400 through 480 Hz; values outside that range are
clamped. Changing tuning or preset rebuilds and resets the tracker.

The 144 analysis-bin field is resampled into 256 output bins. Quadratic
interpolation around an output local maximum provides a smoother displayed
frequency and cents value, but it does not create additional resolving power in
the underlying 50-cent analysis grid.

## Adaptive peak score

The detector intentionally avoids a fixed amplitude threshold. It maintains an
adaptive reference level with an attack coefficient of `0.35` and a release
coefficient of `0.015` per analysis hop. The median smoothed resonator magnitude
acts as a noise-floor estimate.

Each local maximum receives a product score made from:

- level relative to the adaptive reference;
- local prominence over its immediate neighbours;
- contrast over the median noise floor;
- coherent resonator magnitude relative to the input peak.

Smooth transition bands are used rather than hard boundaries. Candidates below
`0.025` after scoring are discarded. This makes the tracker adapt to quiet and
loud sources, but it also means confidence is a detector strength, not a
calibrated probability.

## Harmonic suppression assumption

The tracker assumes that a strong low-frequency candidate may explain peaks at
integer multiples of its frequency. A higher candidate is rejected when all of
the following are true:

- its ratio to an already accepted lower candidate is near harmonic 2 through 8;
- the distance is within the preset's harmonic tolerance;
- the lower candidate has strength greater than `0.06`.

This is useful for colouring a synthesizer or acoustic note at its fundamental
instead of colouring every overtone. It is necessarily ambiguous for polyphonic
music: an independent upper voice at an octave, twelfth, or another coincident
harmonic can be mistaken for an overtone of the lower voice. The greyscale FFT
continues to show the suppressed energy.

## Persistent note tracks

Up to 12 note tracks are retained. A new peak is matched to the nearest active
track inside the preset's matching distance. A matched track follows the peak
in frequency and strength. An unmatched track multiplies its strength by the
preset release factor once per analysis hop and is removed below `0.01`.

Because release is applied per hop, its wall-clock duration depends on the hop
size and sample rate. The standalone default is a 2048-sample FFT with a
512-sample hop. At 48 kHz this yields a new analysis row every 10.67 ms after the
initial 42.67 ms window has filled.

The resonator memory is frequency-dependent. Its approximate time constant is:

```text
time constant = resonatorCycles / frequency
```

For example, Fast has an approximate 73 ms resonator time constant at 55 Hz but
only 9 ms at 440 Hz. Low bass notes therefore need more time to become distinct
than notes in the middle register, even when the analysis hop is short.

## Presets

The presets expose useful combinations rather than eleven independent UI
controls. Balanced is the default.

| Preset | Intended use | Trade-off |
| --- | --- | --- |
| Fast | Arpeggios, rhythmic synthesizers, quickly changing monophonic lines | Broadest matching and pitch field; more likely to follow noise, slide between neighbouring notes, or retain harmonic ambiguity |
| Balanced | General music and the initial integration default | Middle ground between onset response, stability, and separation |
| Stable | Sustained tones, tuning inspection, relatively clean arrangements | Longest low-frequency memory and slowest movement; brief notes are most likely to be missed |

The implementation parameters are:

| Parameter | Fast | Balanced | Stable | Meaning |
| --- | ---: | ---: | ---: | --- |
| `resonatorCycles` | 4.0 | 6.0 | 12.0 | Approximate cycles of resonator memory; larger values improve stability and narrow frequency response but react more slowly |
| `noteAttack` | 0.80 | 0.65 | 0.45 | Fraction of a new strength difference applied per hop |
| `noteRelease` | 0.75 | 0.86 | 0.93 | Strength retained per unmatched hop; values nearer 1 release more slowly |
| `notePositionFollow` | 0.60 | 0.35 | 0.25 | Fraction of a matched position difference followed per hop |
| `noteMatchDistance` | 3.0 | 2.0 | 1.5 | Maximum association distance in 50-cent analysis bins: 150, 100, and 75 cents |
| `fieldSigma` | 1.10 | 0.82 | 0.65 | Width of each track in the output field, in analysis-bin units |
| `prominenceLower` | 0.004 | 0.01 | 0.03 | Start of the local-prominence transition |
| `prominenceUpper` | 0.10 | 0.15 | 0.25 | Full local-prominence score |
| `coherenceLower` | 0.12 | 0.08 | 0.05 | Start of the resonator/input-peak coherence transition |
| `coherenceUpper` | 0.45 | 0.35 | 0.25 | Full coherence score |
| `harmonicTolerance` | 1.00 | 0.65 | 0.50 | Harmonic rejection distance in 50-cent analysis bins: 50, 32.5, and 25 cents |

Some thresholds are deliberately common to all presets: the 20 Hz DC blocker,
adaptive-level coefficients, median-noise transition, `0.025` candidate cutoff,
`0.06` lower-peak requirement for harmonic suppression, and `0.01` track
removal strength.

## Available application controls

The supported runtime controls are intentionally small:

- concert A reference from 400 to 480 Hz;
- Fast, Balanced, or Stable preset;
- pitch-colour overlay enabled or disabled;
- tracked-note annotations enabled or disabled;
- logarithmic or linear FFT display axis;
- vertical waterfall or horizontal history orientation.

Concert A and preset affect the analysis. The remaining controls affect only
visualisation. Internal detector parameters are compile-time implementation
choices and are not currently individual user-facing controls.

## Display and history behaviour

`extractTrackedPitches` considers output-field maxima at confidence `0.05` or
higher, selects the strongest results up to its destination capacity, then
sorts them by frequency for stable presentation. The current widget requests at
most six notes approximately once every 100 ms.

The normal display can retain 12 fading annotations. Horizontal history retains
48 note segments and archives only released segments whose best confidence was
at least `0.15`. Its cards use the sequence number of the corresponding analysis
row and scroll with the FFT history. These are presentation limits, separate
from the tracker's 12 active analysis tracks.

## Assumptions

The present design assumes:

- the input is a mono mixture or can reasonably be downmixed to mono;
- pitched content produces coherent, approximately periodic energy;
- the lowest strong candidate usually represents a fundamental rather than
  unrelated noise or a subharmonic;
- integer-multiple peaks are more often overtones than independent voices;
- twelve-tone equal temperament and a single concert-A reference are suitable
  for naming and cents display;
- six octaves beginning at `concertA / 8` cover the useful fundamentals;
- a confidence-like relative detector strength is sufficient for visualisation;
- pitch colour should favour stable fundamentals while the FFT preserves
  unclassified and transient information.

The tracker does not assume or estimate a musical key. Circle-of-fifths colour
is assigned directly from each detected note's pitch class.

## Current limitations

| Stage | Current limit | Consequence |
| --- | ---: | --- |
| Analysis range | Six octaves from `concertA / 8`; approximately 55–3520 Hz at A4=440 | Fundamentals below A1 and at or above the top of the range are not tracked |
| Analysis resolution | 24 bins per octave, followed by adjacent-bin smoothing | Very close voices can merge; interpolated cents are smoother than the underlying resolving power |
| Active analysis tracks | 12 | Additional accepted fundamentals are discarded; candidates are considered from low to high frequency |
| Harmonic model | Integer harmonics 2–8 | A real upper voice coincident with a lower voice's harmonic may be suppressed |
| UI extraction | Six strongest field maxima | Up to six of the tracker's active notes reach the annotations at one update |
| UI extraction cadence | Approximately 100 ms | A fast note can begin and end between card updates even though analysis rows exist every hop |
| Normal annotation storage | 12 fading cards | Older or weaker overlapping normal-view labels can be reused |
| Horizontal annotation storage | 48 note segments | Dense passages can evict old cards before the 512-row waterfall has fully scrolled |
| Low-frequency response | Resonator memory measured in cycles | Bass notes require longer tones than middle- and high-register notes |
| Voice identity | Nearest-position association only | Crossing, gliding, or rapidly alternating voices may exchange or reuse tracks |
| Output representation | Maximum of track fields | Nearby broad tracks can become one extracted maximum; individual voice identity is not published |
| Musical interpretation | None | No key, chord, scale, score, onset, or instrument classification is performed |

These limits should be evaluated with real instruments in the consuming
application before refinement. In particular, raising capacities does not solve
harmonic ambiguity or low-frequency observation time, and aggressive latency
settings can exchange stability for visually plausible false positives.

## Evaluation guidance

For listening tests, begin with Balanced and a correctly tuned concert-A value.
Compare the coloured fundamentals with the greyscale FFT rather than treating a
missing card as missing audio. Then try Fast for short synthesizer arpeggios and
Stable for sustained tuning work. Useful observations include:

- which register and minimum note duration produce reliable cards;
- whether independent octave voices are suppressed;
- whether attacks briefly choose a harmonic before settling on the fundamental;
- how pitch bends, vibrato, unison detuning, and oscillator drift affect cents;
- whether confidence follows perceptual clarity on the actual synth patch;
- whether six displayed notes and 48 history segments are sufficient in
  realistic JammerNetz sessions.

Changes to capacities, cadence, or preset parameters should be accompanied by
focused synthetic tests and another real-instrument comparison.
