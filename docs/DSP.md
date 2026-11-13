# Auto Clown — DSP design

Signal flow (all in `src/dsp`, no JUCE dependency):

```
in ─┬─ mono sum ─► PitchDetector ──(every ~2.5 ms)──► RetuneEngine ─► ratio, voiced
    │                  └─► RangeLearner (Learn) / KeyDetector (Auto Key)
    ├─ history ring ─► PsolaShifter (Modern) | ClassicShifter (Classic) ─► wet
    └─ history ring ─► dry, delayed by the reported latency
out = dry + bypass · mix · voicedGate · (wet − dry)
```

## 1. Pitch detection — `PitchDetector`

**Choice: two-stage YIN.** YIN is robust on voice, needs no training and gives a
built-in periodicity measure (CMNDF) we reuse for voicing. McLeod (MPM) is similar
in quality; YIN's absolute-threshold rule makes octave behaviour easier to control.

* **Coarse stage** at ~11 kHz (input high-passed at 20 Hz, 4th-order Butterworth
  low-pass at 0.2·fs_dec, decimated by ⌊fs/11 kHz⌋). The difference function is
  evaluated directly — W, τ ≤ ~400 — so cost is bounded and there is no FFT.
* **Fine stage** at the full rate: the plain difference function for `decim + 5` lags
  around the coarse lag, then parabolic interpolation. Steady sines are measured to
  **< 0.3 cents** at 44.1–192 kHz across every input range (unit test).
* **Octave errors**
  * first-dip-below-threshold (YIN) avoids octave-low picks;
  * *octave-down check*: if the (interpolated) dip at 2τ is > 3× deeper than at τ, τ
    was only a harmonic (catches a weak fundamental under a strong 2nd harmonic);
  * *sub-harmonic check*: prefer τ/2 or τ/3 if nearly as periodic;
  * *continuity guard*: an octave jump vs. the last voiced frame must persist for
    4 hops (~10 ms) unless the old-octave dip disappears.
* **Voiced/unvoiced**: periodic (CMNDF < threshold, with 30 % hysteresis) **and**
  above a level gate **and** enough energy below ~2 kHz (fricatives fail this). The
  Tracking control moves all three (strict ↔ relaxed for breathy input).
* **Latency**: none added. The detector is causal and reports the newest window.
* **Input Type** sets the search range (Soprano 140–1400 Hz, Alto/Tenor 85–900,
  Low Male 55–520, Instrument 55–2000, Bass 30–420 Hz).

## 2. Pitch shifting

### Modern — TD-PSOLA (`PsolaShifter`)

**Choice:** for monophonic voice, pitch-synchronous overlap-add is the cheapest method
that inherently preserves formants (each grain is one glottal period, not resampled), and
it has none of the phase-vocoder "phasiness".

* **Analysis marks** are placed one period apart and snapped (±20 % of a period) to the
  peak of a 900 Hz low-passed copy, so every grain starts at the same phase of the glottal
  cycle. Marks keep running through unvoiced audio, so onsets need no restart.
* **Synthesis marks** advance by `T_analysis / ratio`; each takes the nearest *complete*
  analysis grain (Hann, 2 periods). A `sqrt(f/ratio)` gain keeps loudness constant.
* **Formant control**: grains are resampled by `f = ratio^(1 − amount)`. 100 % keeps
  formants in place; Formant Correction off moves them with the pitch. Unit test: +5 st,
  spectral centroid moves ×1.09 with correction and ×1.31 without.
* **Phase lock at ratio ≈ 1**: the synthesis grid is pulled onto the analysis marks when
  no correction is applied, so wet ≈ dry sample-for-sample (correlation 0.9999 in HQ mode)
  and Mix < 100 % or voicing crossfades don't comb-filter.

### Classic — delay-line shifter (`ClassicShifter`)

Two read heads sweep through a delay line centred on the plugin latency with sin² gains
(sum = 1). The sweep window is a whole number of detected periods and only changes
while the live head sits exactly at the centre, so resizing never clicks. Formants move
with pitch and the splices add grit — a deliberately "characterful" alternative.

### Note transitions and consonants

* Ratio changes are applied per grain (Modern) or through a 2 ms smoother (Classic).
* **Unvoiced segments pass through unshifted:** the output crossfades (6 ms) to the dry
  signal delayed by exactly the reported latency. A test asserts bit-exact pass-through.

### Latency

| Mode | Latency | Notes |
|---|---|---|
| Low (default) | **5 ms** | PSOLA uses the newest complete grain; misalignment is bounded to < 2 periods instead of adding delay |
| High Quality | 30 ms | ≥ 2 periods down to ~67 Hz → perfectly time-aligned grains |

Reported with `setLatencySamples()`, and kept during bypass (the engine keeps running).

## 3. Target notes — `ScaleMapper`

13 scales (chromatic, major, natural/harmonic/melodic minor, the modes, major/minor
pentatonic, blues) as 12-bit masks. Each pitch class resolves to **Target**, **Bypass**
(no correction when nearest, but still "catches" the singer) or **Remove** (snap to
neighbours). Keyboard overrides beat the scale. With MIDI Targets on and notes held, the
held pitch classes are the only targets (keyboard Remove still vetoes).
**Hysteresis**: a challenger note must be closer than the current note by 0.18 semitones.

## 4. Retune / Humanize / Flex / Vibrato — `RetuneEngine`

Runs once per detector hop:

1. **Vibrato split**: centre = 2-pole critically damped low-pass (3 Hz) of the detected
   pitch; `p' = centre + g·(p − centre)`, g from Natural Vibrato (−12…+12 dB). The rest
   of the chain works on p', so vibrato is scaled independently of the correction.
2. **Note choice** with hysteresis (above).
3. **Flex Tune**: correction strength fades from 1 to 0 between an inner and outer cents
   window (outer: 60 cents at Flex ≈ 0 → 12 cents at 100), so scoops and bends far from
   the note are left alone.
4. **Retune Speed**: a one-pole glide on the *correction offset* (not the absolute pitch)
   with time constant = Retune Speed. 0 ms = hard snap; slow settings keep natural motion.
5. **Humanize**: after a note has lasted 120 ms the time constant grows (over 400 ms) by
   up to `max(3·retune, 150 ms)·Humanize` — short notes stay snappy, sustains relax.
6. **Transpose** and **Detune** (reference A4 410–470 Hz) are applied to the target grid;
   **Hold** freezes the output pitch.

## 5. Learn and Auto Key — `Analysis`

* **Learn**: the detector temporarily listens wide (50–1600 Hz); a semitone histogram of
  4 s of voiced audio gives 5th–95th percentiles, and the covering range with the closest
  log-centre wins (Instrument penalised so voices get vocal ranges).
* **Auto Key**: an 8 s duration- and confidence-weighted pitch-class profile (glides
  ignored), correlated with the 24 Krumhansl–Kessler major/minor profiles.

Both are fixed-size and run on the audio thread; results reach the UI through atomics and
are applied as host-visible parameter changes from the message thread.

## Real-time safety

`process()` never allocates, locks or does I/O (enforced by an allocation-counting test).
Output is identical for any block size, including 1 and 32 samples (tested). UI data goes
through a wait-free SPSC FIFO (`ac::SpscFifo`), drained at the display refresh rate.
