# Recorded test material

Put licensed vocal/instrument recordings here (`*.wav`, PCM 16/24/32-bit or float).
The detector test suite picks them up automatically:

* Every file is checked for octave stability (frame-to-frame jumps > 9 semitones
  between voiced frames must stay below 0.5 % of frames).
* If a sidecar file `<name>.f0` exists containing a single number (expected median f0
  in Hz), the median detected pitch must be within 50 cents of it.

No recordings are committed to the repository for licensing reasons; the synthetic
vowel generator in `TestSignals.h` (glottal source + formants + vibrato + breath
noise) covers the same code paths in CI.
