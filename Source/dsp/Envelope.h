#pragma once

#include "DspUtil.h"
#include "../Params.h"

namespace pyra
{
/** DAHDSR envelope with curved segments. Restarts from its current level, so
    retriggered/stolen voices never click. */
class Envelope
{
public:
    enum Stage { Idle, Delay, Attack, Hold, Decay, Sustain, Release };

    void setSampleRate (double s) noexcept { sr = (float) s; }

    void noteOn() noexcept
    {
        startLevel = value;
        stage = Delay;
        pos = 0.0f;
    }

    void noteOff() noexcept
    {
        if (stage == Idle || stage == Release) return;
        releaseLevel = value;
        stage = Release;
        pos = 0.0f;
    }

    void kill() noexcept { stage = Idle; value = 0.0f; }

    bool isActive() const noexcept   { return stage != Idle; }
    bool isReleasing() const noexcept { return stage == Release; }
    float getValue() const noexcept  { return value; }

    /** timeScale lets "voice spread" make each voice slightly different. */
    inline float next (const EnvSettings& s, float timeScale = 1.0f) noexcept
    {
        switch (stage)
        {
            case Idle: value = 0.0f; break;

            case Delay:
                if (advance (s.dly * timeScale)) { stage = Attack; pos = 0.0f; }
                else { value = startLevel; break; }
                [[fallthrough]];

            case Attack:
                if (advance (s.att * timeScale)) { stage = Hold; pos = 0.0f; value = 1.0f; }
                else { value = startLevel + (1.0f - startLevel) * curveShape (pos, s.aCrv); break; }
                [[fallthrough]];

            case Hold:
                if (advance (s.hold)) { stage = Decay; pos = 0.0f; }
                else { value = 1.0f; break; }
                [[fallthrough]];

            case Decay:
                if (advance (s.dec * timeScale)) { stage = Sustain; value = s.sus; }
                else { value = 1.0f - (1.0f - s.sus) * curveShape (pos, s.dCrv); }
                break;

            case Sustain:
                // follow the knob smoothly
                value += (s.sus - value) * 0.002f;
                break;

            case Release:
                if (advance (s.rel * timeScale)) { stage = Idle; value = 0.0f; }
                else value = releaseLevel * (1.0f - curveShape (pos, s.rCrv));
                break;
        }
        return value;
    }

private:
    /** Moves through the current stage; returns true when finished. */
    inline bool advance (float seconds) noexcept
    {
        if (seconds <= 0.0005f) return true;
        pos += 1.0f / (seconds * sr);
        if (pos >= 1.0f) return true;
        return false;
    }

    float sr = 44100.0f;
    Stage stage = Idle;
    float value = 0.0f, pos = 0.0f, startLevel = 0.0f, releaseLevel = 0.0f;
};

//==============================================================================
class Lfo
{
public:
    void setSampleRate (double s) noexcept { sr = (float) s; }

    void noteOn (const LfoSettings& s, uint32_t seed) noexcept
    {
        rng.seed (seed);
        elapsed = 0.0f;
        traveled = 0.0f;
        done = false;
        phase = s.phase;
        held = rng.bi(); prev = held; nextVal = rng.bi();
        smoothed = currentShape (s.shape, phase);
        started = true;
    }

    /** Advances by n samples and returns the value (-1..1, or 0..1 if unipolar). */
    float advance (const LfoSettings& s, float hz, int n, float globalPhase) noexcept
    {
        const float dt = (float) n / sr;
        elapsed += dt;

        float ph;
        if (s.mode == LfoFree)
            ph = globalPhase;
        else
        {
            if (! done)
            {
                const float old = phase;
                phase += hz * dt;
                traveled += hz * dt;
                if ((int) phase != (int) old) newRandom();
                phase -= std::floor (phase);
                if (s.mode == LfoEnvelope && traveled >= 1.0f)
                {
                    done = true;
                    phase = std::fmod (s.phase + 0.9999f, 1.0f);
                }
            }
            ph = phase;
        }

        if (s.mode == LfoFree)
        {
            if (ph < lastPhase) newRandom();
            lastPhase = ph;
        }

        float v = currentShape (s.shape, ph);

        if (s.smooth > 0.0f)
        {
            const float coef = std::exp (-dt / (0.0005f + s.smooth * s.smooth * 0.5f));
            smoothed = v + (smoothed - v) * coef;
            v = smoothed;
        }
        else smoothed = v;

        float amp = 1.0f;
        if (elapsed < s.delay) amp = 0.0f;
        else if (s.fade > 0.0f) amp = std::min (1.0f, (elapsed - s.delay) / s.fade);

        v *= amp;
        out = s.uni ? (v + 1.0f) * 0.5f * (amp > 0.0f ? 1.0f : 0.0f) : v;
        return out;
    }

    float getValue() const noexcept { return out; }

    static float shapeValue (int shape, float p, float held = 0, float prev = 0, float nextV = 0) noexcept
    {
        switch (shape)
        {
            case 0:  return std::sin (kTwoPi * p);
            case 1:  return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case 2:  return 2.0f * p - 1.0f;
            case 3:  return 1.0f - 2.0f * p;
            case 4:  return p < 0.5f ? 1.0f : -1.0f;
            case 5:  return p < 0.25f ? 1.0f : -1.0f;
            case 6:  return std::floor (p * 4.0f) / 1.5f - 1.0f;
            case 7:  return 2.0f * (std::exp2 (p * 4.0f) - 1.0f) / 15.0f - 1.0f;
            case 8:  return 2.0f * (std::exp2 ((1.0f - p) * 4.0f) - 1.0f) / 15.0f - 1.0f;
            case 9:  return held;
            case 10: { const float t = 0.5f - 0.5f * std::cos (kPi * p); return prev + (nextV - prev) * t; }
            default: return 0.0f;
        }
    }

private:
    float currentShape (int shape, float p) const noexcept { return shapeValue (shape, p, held, prev, nextVal); }

    void newRandom() noexcept
    {
        held = rng.bi();
        prev = nextVal;
        nextVal = rng.bi();
    }

    float sr = 44100.0f;
    float phase = 0, traveled = 0, elapsed = 0, out = 0, smoothed = 0, lastPhase = 0;
    float held = 0, prev = 0, nextVal = 0;
    bool done = false, started = false;
    Rng rng;
};

} // namespace pyra
