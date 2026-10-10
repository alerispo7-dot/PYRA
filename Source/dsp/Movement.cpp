#include "Movement.h"
#include <juce_core/juce_core.h>

namespace pyra
{
namespace
{
    enum Group { Tone, Bite, Space };
    struct ChaosTarget { int kind; int index; int field; Group group; float depth; bool unipolar; };
    // kind: 0 = oscillator field, 1 = filter field, 2 = noise colour, 3 = FX destination
    const ChaosTarget kTargets[] = {
        { 0, 0, OPos, Tone, 0.35f, false }, { 0, 1, OPos, Tone, 0.35f, false }, { 0, 2, OPos, Tone, 0.35f, false },
        { 0, 0, ODetune, Tone, 0.2f, false }, { 0, 1, ODetune, Tone, 0.2f, false }, { 0, 2, ODetune, Tone, 0.2f, false },
        { 0, 0, OFine, Tone, 0.05f, false }, { 0, 1, OFine, Tone, 0.05f, false }, { 0, 2, OFine, Tone, 0.05f, false },
        { 0, 0, OWarp, Bite, 0.45f, true }, { 0, 1, OWarp, Bite, 0.45f, true }, { 0, 2, OWarp, Bite, 0.45f, true },
        { 0, 0, OFm, Bite, 0.35f, true }, { 0, 1, OFm, Bite, 0.35f, true }, { 0, 2, OFm, Bite, 0.35f, true },
        { 0, 0, OPan, Space, 0.6f, false }, { 0, 1, OPan, Space, 0.6f, false }, { 0, 2, OPan, Space, 0.6f, false },
        { 0, 0, OWidth, Space, 0.4f, true }, { 0, 1, OWidth, Space, 0.4f, true }, { 0, 2, OWidth, Space, 0.4f, true },
        { 1, 0, FCutoff, Tone, 0.22f, false }, { 1, 1, FCutoff, Tone, 0.22f, false },
        { 1, 0, FRes, Bite, 0.25f, true }, { 1, 1, FRes, Bite, 0.25f, true },
        { 1, 0, FDrive, Bite, 0.5f, true }, { 1, 1, FDrive, Bite, 0.5f, true },
        { 2, 0, 0, Tone, 0.5f, false },
        { 3, FxDistMix, 0, Bite, 0.55f, true }, { 3, FxRevMix, 0, Space, 0.35f, true },
        { 3, FxDlyMix, 0, Space, 0.25f, true }, { 3, FxFltCutoff, 0, Tone, 0.2f, false },
    };
    constexpr int kNumTargets = (int) std::size (kTargets);
    static_assert (kNumTargets <= 32, "too many chaos targets");

    float focusCurve (float x, float c) noexcept
    {
        x = std::clamp (x, 0.0f, 1.0f);
        if (c >= 0.0f) return std::pow (x, 1.0f + 3.0f * c);
        return 1.0f - std::pow (1.0f - x, 1.0f - 3.0f * c);
    }

    float smoothstep (float x) noexcept { x = std::clamp (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
}

void Movement::prepare (double sampleRate)
{
    sr = sampleRate;
    seededWith = -1;
    motionSmoothed = chaosSmoothed = 0.0f;
    lastCycle = -1;
}

void Movement::reseed (int seed)
{
    seededWith = seed;
    rng.seed ((uint32_t) seed * 2654435761u + 12345u);
    for (int g = 0; g < kGens; ++g)
    {
        genRate[g] = 0.6f + rng.uni() * 1.0f;
        genPhase[g] = rng.uni();
        genPrev[g] = rng.bi(); genNext[g] = rng.bi();
    }
    for (int t = 0; t < 32; ++t)
    {
        assign[t] = (int) (rng.next() % (uint32_t) kGens);
        sign[t] = rng.uni() < 0.5f ? -1.0f : 1.0f;
    }
}

float Movement::motionShape (int shape, double phase, double cycle)
{
    const float ph = (float) (phase - std::floor (phase));
    switch (shape)
    {
        case 0: return 1.0f - smoothstep (ph / 0.55f);                       // pump: dip at the beat, recover
        case 1: return ph >= 0.5f ? 1.0f : 0.0f;                             // gate
        case 2:
        {
            static const int pat[16] = { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1 };
            const int step = (int) std::fmod (std::max (0.0, cycle), 16.0);
            return pat[step] ? 1.0f : (ph > 0.7f ? 1.0f : 0.0f);              // trance gate (1 = silence)
        }
        case 3: return 1.0f - ph;                                            // filter sweep (saw down)
        case 4: return 0.5f - 0.5f * std::cos (kTwoPi * ph);                 // wobble
        case 5: return std::sin (kTwoPi * ph);                               // auto-pan
        case 6: return std::fmod (ph * 2.0f, 1.0f) > 0.5f ? 1.0f : 0.0f;     // stutter
        case 7: return stepRandom;                                           // random steps
        default: return (1.0f - ph) * (1.0f - ph);                           // swell
    }
}

void Movement::compute (const Settings& s, double beat, double bpm, int n, float* synthOff, float* fxOff)
{
    std::fill (synthOff, synthOff + DCount, 0.0f);
    std::fill (fxOff, fxOff + FxDestCount, 0.0f);
    const auto& mv = s.move;
    const float dt = (float) n / (float) sr;

    // ---------------- CHAOS
    if (mv.seed != seededWith) reseed (mv.seed);
    const float chaosTarget = mv.chaos;
    chaosSmoothed += (chaosTarget - chaosSmoothed) * std::min (1.0f, dt * 20.0f);
    for (int g = 0; g < kGens; ++g)
    {
        genPhase[g] += mv.speed * genRate[g] * dt;
        if (genPhase[g] >= 1.0f)
        {
            genPhase[g] -= std::floor (genPhase[g]);
            genPrev[g] = genNext[g];
            genNext[g] = rng.bi();
        }
        const float t = 0.5f - 0.5f * std::cos (kPi * genPhase[g]);
        genVal[g] = genPrev[g] + (genNext[g] - genPrev[g]) * t;
        uiGen[g].store (genVal[g] * chaosSmoothed, std::memory_order_relaxed);
    }
    chaosValue = genVal[0] * chaosSmoothed;

    if (chaosSmoothed > 1.0e-4f)
    {
        const float scale[3] = { chaosSmoothed, chaosSmoothed * mv.bite * 2.0f, chaosSmoothed * mv.space * 2.0f };
        for (int t = 0; t < kNumTargets; ++t)
        {
            const auto& tg = kTargets[t];
            const float g = genVal[assign[t]] * sign[t];
            const float v = (tg.unipolar ? 0.5f * (g + 1.0f) : g) * tg.depth * scale[tg.group];
            switch (tg.kind)
            {
                case 0: if (s.osc[tg.index].on) synthOff[dOsc (tg.index, (OscField) tg.field)] += v; break;
                case 1: if (s.filt[tg.index].on) synthOff[dFilt (tg.index, (FiltField) tg.field)] += v; break;
                case 2: if (s.noise.on) synthOff[DNoiseColor] += v; break;
                default: fxOff[tg.index] += v; break;
            }
        }
        // bite also pushes the distortion, space opens the delay feedback a little
        fxOff[FxDistDrive] += chaosSmoothed * mv.bite * 0.5f;
        fxOff[FxDlyFb] += chaosSmoothed * mv.space * 0.15f;

        // MANGLE: the top of the CHAOS range tears the sound apart, like Scorch at full:
        // glitch steps (repeat, reverse, tape stop...), grains, octave jumps and frequency shifting
        const float wild = juce::jlimit (0.0f, 1.6f, smoothstep ((chaosSmoothed - 0.4f) / 0.6f) * (0.2f + 1.6f * mv.mangle));
        if (wild > 0.0f)
        {
            fxOff[FxGltMix]    += std::min (1.0f, wild * 1.2f);
            fxOff[FxGltChance] += std::min (0.9f, wild * 0.75f);
            fxOff[FxGrnMix]    += std::min (0.8f, wild * 0.6f * (0.45f + 0.55f * (0.5f + 0.5f * genVal[3])));
            fxOff[FxGrnSpray]  += wild * 0.3f;
            if (wild > 0.5f)
                fxOff[FxGrnPitch] += (genVal[5] > 0.35f ? 12.0f : (genVal[5] < -0.45f ? -12.0f : (genVal[5] > 0.1f ? 7.0f : 0.0f))) / 48.0f;
            fxOff[FxShfMix]    += wild * 0.4f * std::max (0.0f, genVal[6]);
            fxOff[FxShfFreq]   += wild * 0.12f * genVal[7];
            fxOff[FxDistMix]   += wild * 0.15f * mv.bite * 2.0f;
            fxOff[FxRevMix]    += wild * 0.12f * mv.space * 2.0f;
        }
    }

    // ---------------- FOCUS
    focusValue = mv.focus;
    if (mv.focus > 0.0f)
        for (auto& sl : mv.slots)
        {
            if (sl.dst <= DNone || sl.depth == 0.0f) continue;
            const float y = focusCurve (mv.focus, sl.curve) * sl.depth;
            if (sl.dst < DCount) synthOff[sl.dst] += y;
            else if (sl.dst - DCount < FxDestCount) fxOff[sl.dst - DCount] += y;
        }

    // ---------------- MOTION
    if (mv.motion > 0.0f)
    {
        const double divBeats = choices::syncDivisionBeats (mv.rate);
        const double phase = beat / divBeats;
        const double cycle = std::floor (phase);
        if (cycle != lastCycle) { lastCycle = cycle; stepRandom = rng.bi(); }
        (void) bpm;

        const float raw = motionShape (mv.shape, phase, cycle);
        const float tau = 0.0015f + mv.smooth * mv.smooth * 0.08f;
        motionSmoothed += (raw - motionSmoothed) * std::min (1.0f, dt / tau);
        const float m = motionSmoothed, d = mv.motion;
        motionValue = m;
        uiMotion.store (m, std::memory_order_relaxed);

        switch (mv.shape)
        {
            case 0: case 1: case 2: case 6: case 8:
                synthOff[DAmp] += -0.95f * d * m;
                break;
            case 3:
                for (int k = 0; k < kNumFilters; ++k) synthOff[dFilt (k, FCutoff)] += d * (0.55f * m - 0.3f);
                break;
            case 4:
                for (int k = 0; k < kNumFilters; ++k) synthOff[dFilt (k, FCutoff)] += d * (0.45f * m - 0.25f);
                for (int o = 0; o < kNumOsc; ++o) synthOff[dOsc (o, OPos)] += d * 0.25f * m;
                break;
            case 5:
                for (int o = 0; o < kNumOsc; ++o) synthOff[dOsc (o, OPan)] += d * 0.9f * m * ((o & 1) ? -1.0f : 1.0f);
                break;
            default:
                for (int k = 0; k < kNumFilters; ++k) synthOff[dFilt (k, FCutoff)] += d * 0.3f * m;
                for (int o = 0; o < kNumOsc; ++o) synthOff[dOsc (o, OPos)] += d * 0.3f * m * ((o & 1) ? -1.0f : 1.0f);
                break;
        }
    }
    else { motionValue = 0.0f; uiMotion.store (0.0f, std::memory_order_relaxed); motionSmoothed = 0.0f; }
}

} // namespace pyra
