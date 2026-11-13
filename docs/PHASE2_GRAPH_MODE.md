# Phase 2 — Graph mode (plan, not yet implemented)

Prerequisite: Auto mode signed off in the host matrix (`HOST_TESTING.md`).

## Scope
* **Capture**: record input audio + the detector's pitch/voicing track into a
  session-owned, time-stamped store (host timeline position from `AudioPlayHead`).
* **Display**: "The Big Top Ledger" view — a piano-roll grid (gold lanes for scale notes,
  red for removed), detected pitch as a cream curve, corrected pitch as a gold curve,
  waveform strip below, striped canopy time ruler.
* **Edit**: note objects (auto-segmented from the pitch track: stable regions between
  voicing gaps / note changes), drag to retune, split/join, per-note retune speed & vibrato
  depth; freehand and line tools for target curves; undo via the existing UndoManager.
* **Render**: the engine reads a target-pitch curve (time → MIDI) instead of the
  ScaleMapper result when a region is edited; everything else (PSOLA, formants) is reused.

## ARA2
JUCE supports ARA2 (`juce_add_plugin(... IS_ARA_EFFECT TRUE)`) in Logic, Studio One,
Cubase/Nuendo, Reaper and Pro Tools. With ARA the host hands us the whole audio
modification at once, so analysis runs offline (non-real-time, higher accuracy: we can
run the detector bidirectionally and use a larger window) and edits are non-destructive
playback regions. Plan:
1. Factor the engine so detection and synthesis can run on arbitrary sample ranges.
2. ARA document controller owning per-audio-source analysis (pitch track + note objects),
   serialised in the ARA archive.
3. Playback renderer = PSOLA driven by the edited curve; Auto mode stays the non-ARA path.
4. Without ARA, fall back to capture-on-playback (like a "Track" button).

## Risks
* Note segmentation quality on legato singing — use pitch derivative + voicing + energy.
* Host differences in ARA support (Live has no ARA) — keep the capture fallback.
