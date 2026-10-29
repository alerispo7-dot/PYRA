#include "Creative.h"

namespace pyra
{
//==============================================================================
const std::vector<float>& glitchFractions()
{
    static const std::vector<float> f { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
    return f;
}

const GlitchParamSpec& glitchParamSpec (int fx, int index)
{
    static const GlitchParamSpec none {};
    static const GlitchParamSpec table[GCount][kGlitchParams] = {
        /* Off      */ { {}, {}, {} },
        /* Repeat   */ { { "Length", 0, 5, 0, true, "", 1 }, { "Decay", 0, 1, 0, false, "%" }, { "Pitch", -12, 12, 0, true, "st" } },
        /* Stutter  */ { { "Slice", 1, 5, 3, true, "", 1 }, { "Gate", 0.1f, 1, 1, false, "%" }, { "Ramp", -12, 12, 0, true, "st" } },
        /* Reverse  */ { { "Speed", 0.5f, 2, 1, false, "x" }, { "Swell", 0, 1, 0, false, "%" }, {} },
        /* TapeStop */ { { "Time", 0.1f, 1, 1, false, "%" }, { "Start", 0, 1, 0, true, "" }, {} },
        /* Slow     */ { { "Speed", 0.25f, 0.9f, 0.5f, false, "x" }, {}, {} },
        /* PitchUp  */ { { "Interval", 1, 24, 12, true, "st" }, {}, {} },
        /* Scratch  */ { { "Depth", 0.05f, 1, 0.5f, false, "%" }, { "Speed", 1, 8, 2, true, "x" }, {} },
        /* Gate     */ { { "Chops", 1, 8, 1, true, "" }, { "Open", 0.1f, 0.9f, 0.5f, false, "%" }, {} },
        /* Crush    */ { { "Bits", 1, 8, 4.6f, false, "" }, { "Downsample", 1, 32, 6, true, "x" }, {} },
    };
    if (fx < 0 || fx >= GCount || index < 0 || index >= kGlitchParams) return none;
    return table[fx][index];
}

juce::String glitchParamText (int fx, int index, float v)
{
    const auto& sp = glitchParamSpec (fx, index);
    if (sp.name == nullptr) return {};
    if (sp.choices == 1)
    {
        static const char* names[] = { "1 step", "1/2 step", "1/3 step", "1/4 step", "1/6 step", "1/8 step" };
        return names[juce::jlimit (0, 5, (int) std::lround (v))];
    }
    if (fx == GTapeStop && index == 1) return v > 0.5f ? "Start" : "Stop";
    if (juce::String (sp.unit) == "%") return juce::String (juce::roundToInt (v * 100.0f)) + "%";
    if (juce::String (sp.unit) == "st") return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " st";
    if (juce::String (sp.unit) == "x") return sp.integer ? juce::String (juce::roundToInt (v)) + "x" : juce::String (v, 2) + "x";
    return sp.integer ? juce::String (juce::roundToInt (v)) : juce::String (v, 1);
}

void GlitchPattern::setParam (int fx, int i, float v)
{
    const auto& sp = glitchParamSpec (fx, i);
    if (sp.name == nullptr) return;
    v = juce::jlimit (sp.min, sp.max, v);
    if (sp.integer) v = (float) std::lround (v);
    params[(size_t) fx][(size_t) i] = v;
    ++changeCounter;
}

void GlitchPattern::resetParams()
{
    for (int fx = 0; fx < GCount; ++fx)
        for (int i = 0; i < kGlitchParams; ++i) params[(size_t) fx][(size_t) i] = glitchParamSpec (fx, i).def;
}

bool GlitchPattern::paramsAreDefault() const
{
    for (int fx = 0; fx < GCount; ++fx)
        for (int i = 0; i < kGlitchParams; ++i)
            if (glitchParamSpec (fx, i).name != nullptr && std::abs (param (fx, i) - glitchParamSpec (fx, i).def) > 1.0e-4f) return false;
    return true;
}

void GlitchPattern::clear()
{
    for (auto& s : steps) { s.fx = GOff; s.lock = false; }
    resetParams();
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
    return paramsAreDefault();
}

juce::var GlitchPattern::toVar() const
{
    if (isEmpty()) return {};
    juce::Array<juce::var> a;
    for (auto& s : steps) a.add (s.fx.load());
    if (paramsAreDefault()) return a;          // the format of the earlier versions
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("steps", a);
    juce::Array<juce::var> ps;
    for (int fx = 0; fx < GCount; ++fx)
        for (int i = 0; i < kGlitchParams; ++i) ps.add (param (fx, i));
    obj->setProperty ("params", ps);
    return juce::var (obj);
}

void GlitchPattern::fromVar (const juce::var& v)
{
    const bool withParams = ! v.isArray() && v.getDynamicObject() != nullptr;   // (an array var also counts as an "object" in JUCE)
    auto* a = withParams ? v["steps"].getArray() : v.getArray();
    for (int k = 0; k < kMaxGlitchSteps; ++k)
    {
        steps[(size_t) k].fx = a != nullptr && k < a->size() ? juce::jlimit (0, GCount - 1, (int) a->getReference (k)) : (int) GOff;
        steps[(size_t) k].lock = false;
    }
    resetParams();
    if (auto* ps = withParams ? v["params"].getArray() : nullptr)
        for (int fx = 0; fx < GCount; ++fx)
            for (int i = 0; i < kGlitchParams; ++i)
            {
                const int idx = fx * kGlitchParams + i;
                if (idx < ps->size() && glitchParamSpec (fx, i).name != nullptr) setParam (fx, i, (float) ps->getReference (idx));
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
    juce::ValueTree ps ("PARAMS");
    for (int fx = 0; fx < GCount; ++fx)
        for (int i = 0; i < kGlitchParams; ++i)
            if (glitchParamSpec (fx, i).name != nullptr) ps.setProperty ("p" + juce::String (fx) + "_" + juce::String (i), param (fx, i), nullptr);
    t.appendChild (ps, nullptr);
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
    resetParams();
    const auto ps = t.getChildWithName ("PARAMS");
    for (int fx = 0; fx < GCount; ++fx)
        for (int i = 0; i < kGlitchParams; ++i)
        {
            const juce::Identifier id ("p" + juce::String (fx) + "_" + juce::String (i));
            if (ps.hasProperty (id)) setParam (fx, i, (float) ps.getProperty (id));
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
    const double sinceAnchor = (double) (t - anchor);
    constexpr double pi = juce::MathConstants<double>::pi;
    auto frac = [] (float idx) { const auto& f = glitchFractions(); return (double) f[(size_t) juce::jlimit (0, (int) f.size() - 1, (int) std::lround (idx))]; };
    double pos;
    switch (curFx)
    {
        case GRepeat:
        {   // the start of the step, looped (length = a fraction of the step), optionally re-pitched
            const double len = std::max (48.0, L * frac (curP[0]));
            pos = (double) anchor + std::fmod (sinceAnchor, len) * std::exp2 ((double) curP[2] / 12.0);
            break;
        }
        case GStutter:
        {   // short slices; each repeat can climb or fall in pitch (ramp)
            const double slice = std::max (48.0, L * frac (curP[0]));
            const double k = std::min (24.0, std::floor (sinceAnchor / slice));
            pos = (double) anchor + std::fmod (sinceAnchor, slice) * std::exp2 ((double) curP[2] * k / 12.0);
            break;
        }
        case GReverse:  pos = (double) anchor - sinceAnchor * (double) curP[0]; break;
        case GTapeStop:
        {
            const double T = std::max (64.0, L * (double) curP[0]);
            const double xx = std::min (x, T);
            if (curP[1] > 0.5f) pos = x < T ? (double) stepStart + xx * xx / (2.0 * T) : (double) stepStart + T * 0.5 + (x - T);   // tape start
            else pos = (double) stepStart + xx - xx * xx / (2.0 * T);                                                            // tape stop
            break;
        }
        case GSlow:     pos = (double) anchor + sinceAnchor * (double) curP[0]; break;
        case GPitchUp:
        {   // the step before, replayed faster (higher) so it ends where the step ends
            const double sp = std::exp2 ((double) curP[0] / 12.0);
            pos = (double) stepStart - L * (sp - 1.0) + sp * x;
            break;
        }
        case GScratch:
        {
            const double cyc = std::max (1.0, (double) curP[1]);
            const double amp = (double) curP[0] * L / (2.0 * pi * cyc);
            pos = (double) stepStart + 0.5 * x - amp * std::sin (2.0 * pi * cyc * x / L);
            break;
        }
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
            for (int i = 0; i < kGlitchParams; ++i) curP[i] = pattern.param (fx, i);
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

        // per-effect gain: gate chops, stutter gate, tape stopped, repeat decay, reverse swell
        const double L = std::max (64.0, stepLen);
        const double x = (double) (now - stepStart), sinceAnchor = (double) (now - anchor);
        float target = 1.0f, inst = 1.0f;
        switch (curFx)
        {
            case GGate:
            {
                const int chops = juce::jlimit (1, 8, (int) std::lround (curP[0]));
                if (chops <= 1) target = 0.0f;
                else target = std::fmod (x, L / chops) / (L / chops) < (double) curP[1] ? 1.0f : 0.0f;
                break;
            }
            case GStutter:
            {
                const auto& f = glitchFractions();
                const double slice = std::max (48.0, L * (double) f[(size_t) juce::jlimit (0, (int) f.size() - 1, (int) std::lround (curP[0]))]);
                if (curP[1] < 0.999f && std::fmod (sinceAnchor, slice) / slice > (double) curP[1]) target = 0.0f;
                break;
            }
            case GTapeStop:
                if (curP[1] < 0.5f && x > L * (double) curP[0]) target = 0.0f;   // stopped: silence instead of a frozen sample
                break;
            case GRepeat:
                if (curP[1] > 0.0f)
                {
                    const auto& f = glitchFractions();
                    const double len = std::max (48.0, L * (double) f[(size_t) juce::jlimit (0, (int) f.size() - 1, (int) std::lround (curP[0]))]);
                    inst = std::pow (1.0f - curP[1] * 0.85f, (float) std::floor (sinceAnchor / len));
                }
                break;
            case GReverse:
                if (curP[1] > 0.0f) inst = (float) std::min (1.0, x / (L * (double) curP[1]));
                break;
            default: break;
        }
        gateGain += (target - gateGain) * gateA;
        if (curFx == GCrush)
        {
            const int hold = juce::jlimit (1, 32, (int) std::lround (curP[1]));
            if (--crushCount <= 0) { crushHoldL = yl; crushHoldR = yr; crushCount = hold; }
            const float q = std::exp2 (juce::jlimit (1.0f, 8.0f, curP[0]));
            yl = std::round (crushHoldL * q) / q;
            yr = std::round (crushHoldR * q) / q;
        }
        l[k] = yl * gateGain * inst;
        r[k] = yr * gateGain * inst;
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
