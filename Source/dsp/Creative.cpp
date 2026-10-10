#include "Creative.h"

namespace pyra
{
//==============================================================================
void GlitchPattern::clear()
{
    for (auto& s : steps) { s.fx = GOff; s.lock = false; }
    ++changeCounter;
}

void GlitchPattern::randomize (int numSteps, uint32_t seed, float density)
{
    Rng r; r.seed (seed * 2654435761u + 99u);
    // weighted pool: repeats and reverses are the "Gross Beat" sound, gates add rhythm
    static const int pool[] = { GRepeat, GRepeat, GStutter, GStutter, GStutter, GReverse, GReverse, GReverse, GTapeStop,
                                GSlow, GSlow, GPitchUp, GScratch, GScratch, GGate, GGate, GGate, GCrush };
    const int n = juce::jlimit (1, kMaxGlitchSteps, numSteps);
    int k = 0;
    while (k < n)
    {
        auto& s = steps[(size_t) k];
        if (s.lock.load()) { ++k; continue; }
        if (r.uni() >= density) { s.fx = GOff; ++k; continue; }
        const int fx = pool[r.next() % (uint32_t) std::size (pool)];
        // repeats / reverses / slow often last 2 steps (sounds more musical)
        const int run = (fx == GRepeat || fx == GReverse || fx == GSlow) && r.uni() < 0.6f ? 2 : 1;
        for (int j = 0; j < run && k < n; ++j, ++k)
            if (! steps[(size_t) k].lock.load()) steps[(size_t) k].fx = fx;
    }
    ++changeCounter;
}

bool GlitchPattern::isEmpty() const
{
    for (auto& s : steps) if (s.fx.load() != GOff) return false;
    return true;
}

juce::var GlitchPattern::toVar() const
{
    if (isEmpty()) return {};
    juce::Array<juce::var> a;
    for (auto& s : steps) a.add (s.fx.load());
    return a;
}

void GlitchPattern::fromVar (const juce::var& v)
{
    auto* a = v.getArray();
    for (int k = 0; k < kMaxGlitchSteps; ++k)
    {
        steps[(size_t) k].fx = a != nullptr && k < a->size() ? juce::jlimit (0, GCount - 1, (int) a->getReference (k)) : (int) GOff;
        steps[(size_t) k].lock = false;
    }
    ++changeCounter;
}

juce::ValueTree GlitchPattern::toTree() const
{
    juce::ValueTree t ("GLITCH_PATTERN");
    for (auto& s : steps)
    {
        juce::ValueTree c ("STEP");
        c.setProperty ("fx", s.fx.load(), nullptr);
        c.setProperty ("lock", s.lock.load(), nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void GlitchPattern::fromTree (const juce::ValueTree& t)
{
    if (! t.hasType ("GLITCH_PATTERN")) { clear(); return; }
    for (int k = 0; k < kMaxGlitchSteps; ++k)
    {
        auto c = t.getChild (k);
        steps[(size_t) k].fx = c.isValid() ? juce::jlimit (0, GCount - 1, (int) c.getProperty ("fx", 0)) : (int) GOff;
        steps[(size_t) k].lock = c.isValid() && (bool) c.getProperty ("lock", false);
    }
    ++changeCounter;
}

//==============================================================================
void GlitchFx::prepare (float sampleRate)
{
    sr = sampleRate;
    hist.prepare ((int) (sr * 8.0f));
    reset();
}

void GlitchFx::reset()
{
    hist.clear();
    now = 0; curStep = LLONG_MIN; curFx = prevFx = GOff;
    stepStart = anchor = 0; lastPos = -1.0; lastSpeed = 1.0; fadeLeft = 0;
    gateGain = 1.0f; crushCount = 0;
    uiStep = -1; uiEffect = 0;
}

int GlitchFx::effectFor (long long si, const Params& p, const GlitchPattern& pattern)
{
    const int steps = juce::jlimit (1, kMaxGlitchSteps, p.steps);
    const int idx = (int) (((si % steps) + steps) % steps);
    int fx = p.patternOn ? pattern.steps[(size_t) idx].fx.load() : (int) GOff;

    if (p.chance > 0.0f)
    {
        // random effects, decided per pair of steps so they last long enough to be heard;
        // derived from the song position, so the same bar glitches the same way every time it plays
        const uint32_t pair = (uint32_t) (si >> 1);
        Rng r; r.seed (pair * 2654435761u + 0x51ed270bu);
        r.next();
        if (r.uni() < p.chance)
        {
            static const int pool[] = { GRepeat, GRepeat, GRepeat, GStutter, GStutter, GStutter, GReverse, GReverse, GReverse,
                                        GTapeStop, GSlow, GSlow, GPitchUp, GScratch, GScratch, GGate, GGate, GCrush };
            fx = pool[r.next() % (uint32_t) std::size (pool)];
        }
    }
    return fx;
}

double GlitchFx::positionFor (long long t) const
{
    const double x = (double) (t - stepStart);
    const double L = std::max (64.0, stepLen);
    double pos;
    switch (curFx)
    {
        case GRepeat:   pos = (double) anchor + std::fmod ((double) (t - anchor), L); break;
        case GStutter:  { const double slice = std::max (48.0, L * 0.25); pos = (double) anchor + std::fmod ((double) (t - anchor), slice); break; }
        case GReverse:  pos = (double) anchor - (double) (t - anchor); break;
        case GTapeStop: { const double xx = std::min (x, L); pos = (double) stepStart + xx - xx * xx / (2.0 * L); break; }
        case GSlow:     pos = (double) anchor + (double) (t - anchor) * 0.5; break;
        case GPitchUp:  pos = (double) stepStart - L + 2.0 * x; break;
        case GScratch:  pos = (double) stepStart + 0.5 * x - L / (8.0 * juce::MathConstants<double>::pi)
                                                         * std::sin (4.0 * juce::MathConstants<double>::pi * x / L); break;
        default:        pos = (double) t; break;
    }
    return juce::jlimit ((double) (t - hist.capacity() + 8), (double) t, pos);
}

void GlitchFx::process (float* l, float* r, int n, double beat, double bpm, const Params& p, const GlitchPattern& pattern)
{
    bpm = std::max (20.0, bpm);
    const double bps = bpm / 60.0 / sr;
    const double stepBeats = choices::syncDivisionBeats (p.rateIndex);
    fadeLen = std::max (16, (int) (sr * (0.001f + p.smooth * 0.019f)));
    const float gateA = 1.0f - std::exp (-1.0f / (0.003f * sr));
    const long long runLimit = (long long) (sr * 3.0f);

    for (int k = 0; k < n; ++k)
    {
        const double b = beat + k * bps;
        const long long si = (long long) std::floor (b / stepBeats + 1.0e-9);
        if (si != curStep)
        {
            const int fx = effectFor (si, p, pattern);
            stepLen = stepBeats * 60.0 / bpm * sr;
            const bool continues = fx == curFx && (fx == GRepeat || fx == GStutter || fx == GSlow)
                                   && si == curStep + 1 && now - anchor < runLimit;
            prevFx = curFx;
            curFx = fx;
            curStep = si;
            stepStart = now;
            if (! continues) anchor = now;
            const int steps = juce::jlimit (1, kMaxGlitchSteps, p.steps);
            uiStep = (int) (((si % steps) + steps) % steps);
            uiEffect = fx;
        }

        hist.write (now, l[k], r[k]);
        const double pos = positionFor (now);
        if (lastPos >= 0.0 && std::abs (pos - (lastPos + lastSpeed)) > 1.5)
        {   // the read head jumped: fade out the old one
            fadePos = lastPos + lastSpeed;
            fadeSpeed = lastSpeed;
            fadeLeft = fadeLen;
        }

        float yl = hist.read (0, pos), yr = hist.read (1, pos);
        if (fadeLeft > 0)
        {
            const float g = (float) fadeLeft / (float) fadeLen;
            fadePos = std::min (fadePos, (double) now);
            const float ol = hist.read (0, fadePos), orr = hist.read (1, fadePos);
            yl = yl + (ol - yl) * g;
            yr = yr + (orr - yr) * g;
            fadePos += fadeSpeed;
            --fadeLeft;
        }
        lastSpeed = positionFor (now + 1) - pos;
        if (curFx == GRepeat || curFx == GStutter)   // a wrap is a jump, not a speed
            lastSpeed = 1.0;
        lastPos = pos;

        // gate and crush
        gateGain += ((curFx == GGate ? 0.0f : 1.0f) - gateGain) * gateA;
        if (curFx == GCrush)
        {
            if (--crushCount <= 0) { crushHoldL = yl; crushHoldR = yr; crushCount = 6; }
            const float q = 24.0f;
            yl = std::round (crushHoldL * q) / q;
            yr = std::round (crushHoldR * q) / q;
        }
        l[k] = yl * gateGain;
        r[k] = yr * gateGain;
        ++now;
    }
}

void GlitchFx::idle (const float* l, const float* r, int n)
{
    for (int k = 0; k < n; ++k) hist.write (now++, l[k], r[k]);
    curStep = LLONG_MIN; curFx = GOff; lastPos = -1.0; fadeLeft = 0; gateGain = 1.0f;
    uiStep = -1;
}

//==============================================================================
void GranularFx::idle (const float* l, const float* r, int n)
{
    for (int k = 0; k < n; ++k)
    {
        hist.write (writeT++, l[k], r[k]);
        if (std::abs (l[k]) + std::abs (r[k]) > 1.0e-4f) ++soundSamples;
    }
}

void GranularFx::prepare (float sampleRate)
{
    sr = sampleRate;
    hist.prepare ((int) (sr * 4.0f));
    reset();
}

void GranularFx::reset()
{
    hist.clear();
    writeT = 0; soundSamples = 0; spawnIn = 0.0;
    for (auto& g : grains) g.active = false;
    rng.seed (0x6a09e667u);
}

void GranularFx::process (float* l, float* r, int n, const Params& p)
{
    const int maxBack = hist.capacity() - 64;
    const float sizeSec = std::max (0.005f, p.sizeMs * 0.001f);
    const float overlap = std::max (1.0f, p.density * sizeSec);
    const float gain = 1.25f / std::sqrt (overlap);

    // freeze holds the buffer, but only once it holds at least a second of sound (freezing silence is useless)
    const bool frozen = p.freeze && soundSamples > (long long) sr;
    for (int k = 0; k < n; ++k)
    {
        if (! frozen)
        {
            hist.write (writeT++, l[k], r[k]);
            if (std::abs (l[k]) + std::abs (r[k]) > 1.0e-4f) ++soundSamples;
        }
        const long long head = writeT - 1;

        spawnIn -= 1.0;
        if (spawnIn <= 0.0)
        {
            spawnIn += sr / std::max (0.5f, p.density) * (0.7f + 0.6f * rng.uni());
            for (auto& g : grains)
            {
                if (g.active) continue;
                g.length = std::max (32, (int) (sizeSec * sr * (0.9f + 0.2f * rng.uni())));
                const float semis = p.pitch + p.pitchRand * 12.0f * rng.bi();
                double speed = std::exp2 (semis / 12.0f);
                const bool rev = rng.uni() < p.reverse;
                double back = 48.0 + p.spray * rng.uni() * sr;
                if (! rev) back += g.length * std::max (0.0, speed - (frozen ? 0.0 : 1.0));
                else speed = -speed;
                back = std::min (back, (double) maxBack - g.length * std::abs (speed));
                g.pos = (double) head - std::max (32.0, back);
                g.speed = speed;
                g.age = 0;
                float gl, gr;
                panGains (rng.bi() * p.width, gl, gr);
                g.gl = gl; g.gr = gr;
                g.active = true;
                break;
            }
        }

        float ol = 0.0f, orr = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float ph = (float) g.age / (float) g.length;
            const float w = std::sin (kPi * ph);
            const float win = w * w;
            const double pos = std::min (g.pos, (double) head);
            ol += hist.read (0, pos) * win * g.gl;
            orr += hist.read (1, pos) * win * g.gr;
            g.pos += g.speed;
            if (++g.age >= g.length) g.active = false;
        }
        l[k] = ol * gain;
        r[k] = orr * gain;
    }
}

//==============================================================================
void ShifterFx::Hilbert::process (float x, float& re, float& im) noexcept
{
    static const float ca[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
    static const float cb[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
    float a = x, b = x;
    for (int s = 0; s < 4; ++s)
    {
        const float c2a = ca[s] * ca[s];
        const float ya0 = c2a * (a + ya[s][1]) - xa[s][1];
        xa[s][1] = xa[s][0]; xa[s][0] = a;
        ya[s][1] = ya[s][0]; ya[s][0] = ya0;
        a = ya0;

        const float c2b = cb[s] * cb[s];
        const float yb0 = c2b * (b + yb[s][1]) - xb[s][1];
        xb[s][1] = xb[s][0]; xb[s][0] = b;
        yb[s][1] = yb[s][0]; yb[s][0] = yb0;
        b = yb0;
    }
    re = a;
    im = delayB;
    delayB = b;
}

void ShifterFx::prepare (float sampleRate) { sr = sampleRate; reset(); }

void ShifterFx::reset()
{
    for (auto& h : hil) h.reset();
    phase[0] = phase[1] = 0.0;
    fb[0] = fb[1] = 0.0f;
}

void ShifterFx::process (float* l, float* r, int n, const Params& p)
{
    const double inc[2] = { p.freq / sr, p.freq * (1.0 + 0.08 * p.width) / sr };
    const double offset = 0.25 * p.width;
    float* io[2] = { l, r };
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < n; ++k)
        {
            const float x = io[ch][k] + p.feedback * fastTanh (fb[ch]);
            phase[ch] += inc[ch];
            phase[ch] -= std::floor (phase[ch]);
            const double ph = phase[ch] + (ch == 1 ? offset : 0.0);
            const float c = (float) std::cos (juce::MathConstants<double>::twoPi * ph);
            const float s = (float) std::sin (juce::MathConstants<double>::twoPi * ph);
            float y;
            if (p.mode == 1) y = x * s;
            else
            {
                float re, im;
                hil[ch].process (x, re, im);
                y = re * c - im * s;
            }
            fb[ch] = y;
            io[ch][k] = y;
        }
}

} // namespace pyra
