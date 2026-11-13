# CPU usage

Target: **< 3 % of one core per instance.** Measured with `AutoClownBench`
(`tests/CpuBenchmark.cpp`): 10 s of a synthetic sung vowel with 5.5 Hz / ±60 cent
vibrato and breath noise, **stereo**, C major, Retune 25 ms, Humanize 30 %, with
`setParams()` called every block exactly as the plugin does. The figure is wall-clock
processing time divided by audio duration (= % of one core in real time).

Measured 2026-11-13 on an **Intel Xeon @ 2.1 GHz cloud vCPU** (Linux, GCC 13, `-O3`).
Expect lower numbers on any recent Mac (Apple Silicon cores are considerably faster
per clock); re-run `AutoClownBench` on your target machines — the release script
saves its output to `build-release/cpu-benchmark.txt`.

| Sample rate | Engine  | Latency | 32-sample blocks | 512-sample blocks |
|------------:|---------|---------|-----------------:|------------------:|
| 44.1 kHz    | Modern  | Low     | 0.63 % | 0.62 % |
| 44.1 kHz    | Modern  | HQ      | 0.63 % | 0.62 % |
| 44.1 kHz    | Classic | Low     | 0.72 % | 0.71 % |
| 48 kHz      | Modern  | Low     | 0.83 % | 0.99 % |
| 48 kHz      | Modern  | HQ      | 1.04 % | 1.04 % |
| 48 kHz      | Classic | Low     | 1.12 % | 0.98 % |
| 96 kHz      | Modern  | Low     | 1.63 % | 1.37 % |
| 96 kHz      | Modern  | HQ      | 1.17 % | 1.15 % |
| 96 kHz      | Classic | HQ      | 1.33 % | 1.42 % |
| 192 kHz     | Modern  | Low     | 1.98 % | 1.91 % |
| 192 kHz     | Modern  | HQ      | 1.93 % | 1.88 % |
| 192 kHz     | Classic | Low     | 2.34 % | 2.28 % |

Worst case: **2.34 %** (192 kHz, Classic, stereo). All configurations are under the 3 %
budget. Run-to-run noise on a shared cloud VM is roughly ±0.2 %.

Why it is cheap:

* **Detector**: YIN runs on a ~11 kHz decimated signal (O(W·τ) with W, τ < 400) once
  every ~2.5 ms; refinement at the full rate evaluates only `decim + 5` lags.
* **PSOLA**: per output sample, ~2 overlapping grains × channels × one interpolated read.
* **No FFTs, no allocation, no locks** in the audio path (an allocation-counting unit
  test enforces zero heap allocations in `process()`).

The editor (meter + keyboard repaint at 60 fps) runs on the message thread and is not
included in these numbers.
