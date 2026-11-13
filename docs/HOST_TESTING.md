# Host test matrix

Automated checks that **were run** while developing (Linux container, VST3):

| Check | Result |
|---|---|
| `AutoClownTests` (35 DSP tests) | ✅ pass |
| pluginval 1.0.4, strictness 10, VST3, editor tests under Xvfb | ✅ pass (`docs/validation/`) |
| Editor rendered at 50 % / 100 % / 200 % (`AutoClownSnapshot`) | ✅ (`docs/screenshots/`) |

Checks that **must be run on macOS** (not possible in the Linux build container):

| Check | How |
|---|---|
| Universal binary (arm64 + x86_64) | `lipo -info` — automated in `scripts/build_sign_notarize.sh` |
| `auval -v aumf AcLn Ycmp` | automated in the release script |
| pluginval strictness 10 on AU and VST3 | automated in the release script |
| Codesign / notarization / Gatekeeper | automated in the release script |

## Manual host checklist (per release)

Hosts: **Logic Pro** (AU), **Ableton Live** (AU + VST3), **Reaper** (AU + VST3),
**Cubase / Studio One** (VST3), **Pro Tools** (AAX — later phase).

For each host/format:

1. **Load & latency** — insert on a vocal track; confirm the host shows 5 ms latency
   (Low Latency) and that delay compensation lines the track up with others. Switch
   Settings → Latency → High Quality: host should report 30 ms and re-compensate.
2. **Corner resize** — drag the striped corner from minimum (50 %) to maximum (200 %).
   Aspect ratio must stay locked, nothing overlaps, text stays sharp on Retina.
3. **Host resize** — resize from the host's window frame (Live, Reaper, Cubase, Studio
   One support VST3 host-driven resizing). The constrainer must snap to the aspect ratio
   with no jitter.
4. **Size presets** — Settings → Window Size → 75/100/125/150/200 %.
5. **Persistence** — save the project at 150 %, reopen: editor opens at 150 %. Insert a
   new instance in a fresh project: it opens at the last size used (global default).
6. **Audio during resize** — play a vocal loop while continuously resizing; there must be
   no dropouts (resizing touches no audio-thread state; check the host CPU meter).
7. **Automation** — automate Retune Speed and Key; undo/redo in the plugin.
8. **MIDI targets** — Logic: use the "MIDI-controlled effect" slot (AU type `aumf`) and
   select Auto Clown as the MIDI destination; Live/Reaper/Cubase: route a MIDI track to the
   plugin. Enable MIDI Targets and hold notes.
9. **Presets** — browse factory presets; save a user preset; host program list (VST3).
10. **Bypass** — host bypass and plugin bypass button: seamless, latency kept.

Record results (host version, macOS version, CPU, pass/fail, notes) in this file.
