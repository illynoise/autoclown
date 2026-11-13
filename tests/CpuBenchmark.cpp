// CPU benchmark: worst-case-ish streaming load (sung vowel with vibrato, stereo).
// Reports the percentage of one core used in real time for each configuration.
#include "../tests/TestSignals.h"
#include "dsp/AutoClownEngine.h"

#include <chrono>
#include <cstdio>

using namespace ac;

int main()
{
    std::printf ("%-8s %-8s %-6s %-5s %10s %10s\n", "rate", "engine", "lat", "block", "%core", "us/block");
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const double seconds = 10.0;
        auto x = actest::vowel (fs, 196.0, seconds, 5.5, 60.0, -45.0);
        for (auto mode : { EngineMode::Modern, EngineMode::Classic })
            for (auto lm : { LatencyMode::Low, LatencyMode::HighQuality })
                for (int block : { 32, 512 })
                {
                    AutoClownEngine e;
                    e.prepare (fs, block, 2, lm);
                    EngineParams p;
                    p.engine = mode;
                    p.scale = ScaleType::Major;
                    p.retune.retuneMs = 25.0f;
                    p.retune.humanize = 0.3f;
                    p.noteModes.fill (NoteMode::Scale);
                    e.setParams (p);
                    actest::Buffer l = x, r = x;
                    const auto t0 = std::chrono::steady_clock::now();
                    size_t blocks = 0;
                    for (size_t i = 0; i + (size_t) block <= x.size(); i += (size_t) block, ++blocks)
                    {
                        float* ch[2] = { l.data() + i, r.data() + i };
                        e.setParams (p); // as the plugin does every block
                        e.process (ch, 2, block);
                        MeterFrame f;
                        while (e.meterFifo.pop (f)) {}
                    }
                    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                    std::printf ("%-8.0f %-8s %-6s %-5d %9.2f%% %10.2f\n", fs,
                                 mode == EngineMode::Modern ? "Modern" : "Classic",
                                 lm == LatencyMode::Low ? "low" : "HQ", block,
                                 100.0 * el / seconds, 1.0e6 * el / (double) blocks);
                }
    }
    return 0;
}
