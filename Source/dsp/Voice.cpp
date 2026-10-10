#include "Voice.h"

namespace pyra
{
namespace
{
    constexpr float kVoiceGain = 0.5f;   // headroom: a 4-note chord peaks around -6 dBFS

    inline float frac (float x) noexcept { return x - std::floor (x); }

    inline float warpPhase (float p, int mode, float a) noexcept
    {
        switch (mode)
        {
            case WarpSync:      return frac (p * (1.0f + a * 7.0f));
            case WarpBendPlus:  return std::pow (p, 1.0f / (1.0f + a * 4.0f));
            case WarpBendMinus: return std::pow (p, 1.0f + a * 4.0f);
            case WarpPwm:
            {
                const float w = 1.0f - a * 0.95f;
                return p < w ? p / w : 0.0f;
            }
            case WarpAsym:
            {
                const float w = 0.5f * (1.0f - a * 0.96f);
                return p < w ? 0.5f * p / w : 0.5f + 0.5f * (p - w) / (1.0f - w);
            }
            case WarpMirror:
            {
                const float m = p < 0.5f ? p * 2.0f : (1.0f - p) * 2.0f;
                return frac (p + (m - p) * a);
            }
            default: return p;
        }
    }

    inline float warpOutput (float x, int mode, float a) noexcept
    {
        if (mode == WarpFold && a > 0.0f)
            return std::sin (x * (1.0f + a * 6.0f) * 1.5707963f);
        if (mode == WarpQuantize && a > 0.0f)
        {
            const float levels = std::exp2 (12.0f - a * 10.5f) * 0.5f;
            return std::round (x * levels) / levels;
        }
        return x;
    }
}

void Voice::prepare (double sampleRate)
{
    sr = (float) sampleRate;
    for (auto& f : filters) f.prepare (sampleRate);
    ampEnv.setSampleRate (sampleRate);
    for (auto& e : modEnv) e.setSampleRate (sampleRate);
    for (auto& l : lfos) l.setSampleRate (sampleRate);
    kill();
}

void Voice::start (int midiNote, int midiChannel, float vel, float glideFrom, const Settings& s, uint32_t seed)
{
    const bool wasSounding = active && ampEnv.getValue() > 0.001f;

    rng.seed (seed);
    note = midiNote; channel = midiChannel; velocity = vel;
    keyDown = true; sustained = false;
    polyPressure = 0; mpeBendSemis = 0; mpePressure = 0; mpeSlide = 0;
    randomNote = rng.bi();
    for (auto& r : spreadR) r = rng.bi();
    for (auto& t : tolOsc) t = rng.bi();
    tolFilter = rng.bi();
    driftVal = rng.bi() * 0.5f; driftTarget = rng.bi(); driftTimer = 0.0f;

    noteTarget = (float) midiNote;
    if (glideFrom >= 0.0f && s.glide > 0.0f)
    {
        noteCur = glideFrom;
        glideStep = s.glideType == 1 ? 12.0f / (s.glide * sr)   // constant rate: 'glide' seconds per octave
                                       : std::abs (noteTarget - noteCur) / (s.glide * sr);
    }
    else { noteCur = noteTarget; glideStep = 0.0f; }

    if (! wasSounding)
    {
        // fresh start: reset phases and filters
        for (int o = 0; o < kNumOsc; ++o)
        {
            auto& os = osc[o];
            for (int u = 0; u < kMaxUnison; ++u)
                os.phase[u] = frac (s.osc[o].phase + s.osc[o].rand * rng.uni());
            os.gainL = os.gainR = 0.0f;
            os.lastOut = 0.0f;
        }
        subPhase = 0.0f; subOut = 0.0f;
        for (auto& f : filters) f.reset();
        for (auto& p : pink) p = 0.0f;
        brown = noiseLp = noiseOut = noiseHold = 0.0f;
        subGainL = subGainR = noiseGainL = noiseGainR = 0.0f;
    }

    startSample (s);
    ampEnv.noteOn();
    for (auto& e : modEnv) e.noteOn();
    for (int l = 0; l < kNumLfo; ++l)
        lfos[l].noteOn (s.lfo[l], seed * 31u + (uint32_t) l * 7919u);

    active = true;
}

void Voice::legatoTo (int midiNote, float vel, bool retriggerEnvelopes, bool glide, const Settings& s)
{
    note = midiNote;
    noteTarget = (float) midiNote;
    keyDown = true; sustained = false;
    if (glide && s.glide > 0.0f)
        glideStep = s.glideType == 1 ? 12.0f / (s.glide * sr)   // constant rate: 'glide' seconds per octave
                                       : std::abs (noteTarget - noteCur) / (s.glide * sr);
    else { noteCur = noteTarget; glideStep = 0.0f; }

    if (retriggerEnvelopes)
    {
        velocity = vel;
        startSample (s);
        ampEnv.noteOn();
        for (auto& e : modEnv) e.noteOn();
        for (int l = 0; l < kNumLfo; ++l)
            if (s.lfo[l].mode != LfoFree)
                lfos[l].noteOn (s.lfo[l], rng.next());
    }
}

void Voice::startSample (const Settings& s)
{
    smpDone = true;
    smpZone = nullptr;
    if (! s.smp.on || s.smp.inst <= 0) return;
    smpInstIndex = s.smp.inst;
    smpInst = SampleBank::get().acquire (s.smp.inst);
    if (smpInst == nullptr) return;   // still loading: the next notes will have it
    const int vel = juce::jlimit (1, 127, (int) std::lround (100.0f + (velocity * 127.0f - 100.0f) * s.smp.velLayers));
    smpZone = smpInst->find (note, vel);
    if (smpZone == nullptr) return;
    smpPos = (double) s.smp.start * 0.3 * smpInst->sampleRate;   // up to 300 ms into the sample
    if (smpPos >= smpZone->length - 4) smpPos = 0;
    smpDone = false;
    smpGainL = smpGainR = 0.0f;
}

void Voice::release()
{
    keyDown = false;
    ampEnv.noteOff();
    for (auto& e : modEnv) e.noteOff();
}

void Voice::kill()
{
    active = false;
    keyDown = false; sustained = false;
    ampEnv.kill();
    for (auto& e : modEnv) e.kill();
    note = -1;
}


//==============================================================================
void Voice::computeModulation (const VoiceContext& ctx, float* v, float* cutoffOffset)
{
    const Settings& s = *ctx.s;
    std::copy (s.baseRaw, s.baseRaw + DCount, v);
    for (int f = 0; f < kNumFilters; ++f) cutoffOffset[f] = 0.0f;
    const int ng = ctx.globalOff != nullptr ? s.numGlobal : 0;
    if (s.numSlots == 0 && ng == 0) return;

    float off[DCount];
    for (int i = 0; i < s.numSlots; ++i) off[s.slots[i].dst] = 0.0f;
    for (int i = 0; i < ng; ++i) off[s.globalDests[i]] = ctx.globalOff[s.globalDests[i]];

    if (s.numSlots > 0)
    {
        float src[SrcCount] {};
        src[SrcEnv1] = ampEnv.getValue();
        for (int e = 1; e < kNumEnv; ++e) src[SrcEnv1 + e] = modEnv[e - 1].getValue();
        for (int l = 0; l < kNumLfo; ++l) src[SrcLfo1 + l] = lfos[l].getValue();
        src[SrcVelocity]  = velocity;
        src[SrcNote]      = (noteCur - 60.0f) / 60.0f;
        src[SrcRandom]    = randomNote;
        src[SrcModWheel]  = ctx.modWheel;
        src[SrcPressure]  = std::max ({ ctx.channelPressure, polyPressure, mpePressure });
        src[SrcPitchBend] = (s.mpe && channel != 1)
                                ? std::clamp (mpeBendSemis / (float) std::max (1, s.mpeRange), -1.0f, 1.0f)
                                : ctx.pitchBend;
        src[SrcSlide]     = mpeSlide;
        for (int m = 0; m < kNumMacros; ++m) src[SrcMacro1 + m] = s.macros[m];
        src[SrcChaos] = ctx.chaos; src[SrcFocus] = ctx.focus; src[SrcMotion] = ctx.motion;

        for (int i = 0; i < s.numSlots; ++i) off[s.slots[i].dst] += src[s.slots[i].src] * s.slots[i].amt;
    }

    auto convert = [&] (int d)
    {
        const auto& c = s.conv[d];
        switch (c.kind)
        {
            case DestConv::LfoRate: v[d] = std::exp2 (off[d] * 6.0f); break;
            case DestConv::Amp:     v[d] = std::clamp (1.0f + off[d], 0.0f, 2.0f); break;
            default:                v[d] = c.toRaw (s.baseNorm[d] + off[d]); break;
        }
    };
    for (int i = 0; i < s.numSlots; ++i) convert (s.slots[i].dst);
    for (int i = 0; i < ng; ++i) convert (s.globalDests[i]);

    for (int f = 0; f < kNumFilters; ++f)
        if (s.destModded[dFilt (f, FCutoff)])
            cutoffOffset[f] = off[dFilt (f, FCutoff)];
}

//==============================================================================
void Voice::render (float* outL, float* outR, int n, const VoiceContext& ctx)
{
    if (! active || n <= 0) return;

    const Settings& s = *ctx.s;
    const float dt = (float) n / sr;
    const auto& bank = Wavetables::get();

    // ---- glide
    if (glideStep > 0.0f)
    {
        const float d = noteTarget - noteCur, step = glideStep * (float) n;
        if (std::abs (d) <= step) { noteCur = noteTarget; glideStep = 0.0f; }
        else noteCur += d > 0.0f ? step : -step;
    }
    else noteCur = noteTarget;

    // ---- analog drift (slow random wandering)
    if (s.drift > 0.0f)
    {
        driftTimer -= dt;
        if (driftTimer <= 0.0f) { driftTarget = rng.bi(); driftTimer = 0.4f + rng.uni() * 1.2f; }
        driftVal += (driftTarget - driftVal) * std::min (1.0f, dt * 1.5f);
    }

    const float timeScale = 1.0f + spreadR[2] * s.spread * 0.2f + tolFilter * s.tolerance * 0.1f;

    // ---- modulation envelopes + LFOs
    for (int e = 1; e < kNumEnv; ++e)
        if (s.envUsed[e])
            for (int i = 0; i < n; ++i) modEnv[e - 1].next (s.env[e], timeScale);

    for (int l = 0; l < kNumLfo; ++l)
        if (s.lfoUsed[l])
        {
            const float hz = s.lfo[l].sync ? ctx.lfoSyncHz[l] : s.lfo[l].rate;
            lfos[l].advance (s.lfo[l], hz * lfoRateMul[l], n, ctx.lfoGlobalPhase[l]);
        }

    float v[DCount], cutOff[kNumFilters];
    computeModulation (ctx, v, cutOff);
    for (int l = 0; l < kNumLfo; ++l) lfoRateMul[l] = v[DLfoRateBase + l];

    // ---- pitch
    const float bend = (s.mpe && channel != 1) ? mpeBendSemis + ctx.globalBendSemis : ctx.globalBendSemis;
    const float basePitch = noteCur + (float) s.transpose + s.tune * 0.01f + bend
                          + s.drift * driftVal * 0.18f + spreadR[0] * s.spread * 0.12f;

    // ---- oscillator setup
    struct OscRun
    {
        const float* a = nullptr; const float* b = nullptr;
        int len = 0, voices = 1, warp = 0, fmSrc = 0, route = 0;
        float ff = 0, warpAmt = 0, fmDepth = 0, stepL = 0, stepR = 0;
        float inc[kMaxUnison], gl[kMaxUnison], gr[kMaxUnison];
        bool on = false;
    } run[kNumOsc];

    for (int o = 0; o < kNumOsc; ++o)
    {
        const auto& os = s.osc[o];
        auto& r = run[o];
        r.on = os.on;
        if (! r.on) { osc[o].lastOut = 0.0f; continue; }

        const float pitch = basePitch + (float) (os.oct * 12) + v[dOsc (o, OPitch)] + v[dOsc (o, OFine)] * 0.01f
                          + tolOsc[o] * s.tolerance * 0.09f;   // component tolerance: each oscillator slightly off
        const float hz = midiToHz (pitch);
        const int nv = os.voices;
        const float detune = v[dOsc (o, ODetune)];
        const float semis = detune * detune * 1.5f;
        const float blend = v[dOsc (o, OBlend)];
        const float width = v[dOsc (o, OWidth)];

        r.voices = nv; r.warp = os.warp; r.warpAmt = v[dOsc (o, OWarp)];
        r.fmSrc = os.fmSrc; r.route = os.route;
        const float fm = v[dOsc (o, OFm)];
        r.fmDepth = os.fmSrc != FmOff ? fm * fm * 1.5f : 0.0f;

        float effHz = hz * (nv > 1 ? std::exp2 (semis / 12.0f) : 1.0f);
        if (r.warp == WarpSync) effHz *= 1.0f + r.warpAmt * 7.0f;
        if (r.fmDepth > 0.0f)   effHz *= 1.0f + fm * 4.0f;
        const int mip = Wavetables::mipFor (effHz, sr);

        const auto& tab = bank.table (os.wt);
        const float fpos = std::clamp (v[dOsc (o, OPos)], 0.0f, 1.0f) * (float) (tab.numFrames - 1);
        const int f0 = (int) fpos;
        const int f1 = std::min (f0 + 1, tab.numFrames - 1);
        r.ff = fpos - (float) f0;
        r.a = tab.level (f0, mip);
        r.b = tab.level (f1, mip);
        r.len = Wavetables::mipLength (mip);

        float power = 0.0f;
        for (int u = 0; u < nv; ++u)
        {
            const float d = nv == 1 ? 0.0f : -1.0f + 2.0f * (float) u / (float) (nv - 1);
            r.inc[u] = std::min (hz * std::exp2 (d * semis / 12.0f) / sr, 0.49f);
            const bool centre = nv <= 2 || ((nv & 1) ? u == nv / 2 : (u == nv / 2 || u == nv / 2 - 1));
            const float g = centre ? 1.0f - blend * 0.5f : blend;
            power += g * g;
            float pl, pr;
            panGains (nv == 1 ? 0.0f : std::abs (d) * width * ((u & 1) ? -1.0f : 1.0f), pl, pr);
            r.gl[u] = g * pl; r.gr[u] = g * pr;
        }
        const float norm = power > 0.0f ? 1.0f / std::sqrt (power) : 0.0f;
        for (int u = 0; u < nv; ++u) { r.gl[u] *= norm; r.gr[u] *= norm; }

        float pl, pr;
        panGains (v[dOsc (o, OPan)], pl, pr);
        const float lvl = v[dOsc (o, OLevel)];
        r.stepL = (lvl * pl - osc[o].gainL) / (float) n;
        r.stepR = (lvl * pr - osc[o].gainR) / (float) n;
    }

    // ---- sampler setup
    const bool smpActive = s.smp.on && ! smpDone && smpZone != nullptr && smpInst != nullptr && smpInstIndex == s.smp.inst;
    double smpInc = 0.0; float smpStepL = 0, smpStepR = 0;
    if (smpActive)
    {
        const float pitch = basePitch + (float) (12 * s.smp.oct + s.smp.semi) + s.smp.fine * 0.01f + smpZone->tune * 0.01f
                          - (float) smpZone->root;
        smpInc = std::exp2 ((double) pitch / 12.0) * smpInst->sampleRate / (double) sr;
        float pl, pr; panGains (s.smp.pan, pl, pr);
        smpStepL = (s.smp.level * pl - smpGainL) / (float) n;
        smpStepR = (s.smp.level * pr - smpGainR) / (float) n;
    }

    // ---- sub + noise setup
    const float subInc = std::min (midiToHz (basePitch + 12.0f * (float) s.sub.oct) / sr, 0.49f);
    float subStepL = 0, subStepR = 0;
    if (s.sub.on)
    {
        float pl, pr; panGains (s.sub.pan, pl, pr);
        const float lvl = v[DSubLevel];
        subStepL = (lvl * pl - subGainL) / (float) n;
        subStepR = (lvl * pr - subGainR) / (float) n;
    }
    float noiseStepL = 0, noiseStepR = 0;
    const float color = v[DNoiseColor];
    if (s.noise.on)
    {
        float pl, pr; panGains (s.noise.pan, pl, pr);
        const float lvl = v[DNoiseLevel] * 0.6f;
        noiseStepL = (lvl * pl - noiseGainL) / (float) n;
        noiseStepR = (lvl * pr - noiseGainR) / (float) n;
    }
    const float noiseLpCoef = std::min (1.0f, 2.0f * kPi * 700.0f / sr);

    // ---- filters
    const float env2 = modEnv[0].getValue();
    const float keyOffset = (noteCur - 60.0f) / 12.0f;
    for (int f = 0; f < kNumFilters; ++f)
    {
        if (! s.filt[f].on) continue;
        const float norm = std::clamp (s.baseNorm[dFilt (f, FCutoff)] + cutOff[f] + env2 * v[dFilt (f, FEnv)], 0.0f, 1.0f);
        const float hz = 20.0f * std::pow (1000.0f, norm)
                       * std::exp2 (s.filt[f].key * keyOffset + spreadR[1] * s.spread * 0.25f + s.drift * driftVal * 0.08f
                                    + tolFilter * s.tolerance * 0.2f);
        filters[f].setTarget (s.filt[f].type, hz, v[dFilt (f, FRes)], v[dFilt (f, FDrive)], v[dFilt (f, FMix)], n);
    }

    // ---- amp
    const float targetAmp = v[DAmp] * (1.0f - s.velAmp * (1.0f - velocity)) * (1.0f + spreadR[3] * s.spread * 0.1f);
    const float ampStep = (targetAmp - ampGain) / (float) n;

    const bool serial = s.filterRouting == 0;
    const bool f1On = s.filt[0].on, f2On = s.filt[1].on;

    for (int i = 0; i < n; ++i)
    {
        float b1L = 0, b1R = 0, b2L = 0, b2R = 0, dL = 0, dR = 0;

        auto route = [&] (int r, float l, float rr)
        {
            switch (r)
            {
                case RouteF1:   b1L += l; b1R += rr; break;
                case RouteF2:   b2L += l; b2R += rr; break;
                case RouteBoth: b1L += l; b1R += rr; if (! serial) { b2L += l; b2R += rr; } break;
                default:        dL += l; dR += rr; break;
            }
        };

        const float fmVals[6] = { 0.0f, osc[0].lastOut, osc[1].lastOut, osc[2].lastOut, subOut, noiseOut };

        for (int o = 0; o < kNumOsc; ++o)
        {
            auto& r = run[o];
            if (! r.on) continue;
            auto& os = osc[o];

            const float pm = r.fmDepth * fmVals[r.fmSrc];
            const bool phaseWarp = r.warp >= WarpSync && r.warp <= WarpMirror && r.warpAmt > 0.0f;
            const bool outWarp = (r.warp == WarpFold || r.warp == WarpQuantize) && r.warpAmt > 0.0f;
            const float lenF = (float) r.len;

            float sumL = 0, sumR = 0, mono = 0;
            for (int u = 0; u < r.voices; ++u)
            {
                float p = os.phase[u];
                if (pm != 0.0f) p = frac (p + pm);
                if (phaseWarp) p = warpPhase (p, r.warp, r.warpAmt);

                const float idx = p * lenF;
                int i0 = (int) idx;
                if (i0 >= r.len) i0 = r.len - 1;
                if (i0 < 0) i0 = 0;
                const float fr = idx - (float) i0;
                float x = r.a[i0] + fr * (r.a[i0 + 1] - r.a[i0]);
                if (r.ff > 0.0f)
                {
                    const float y = r.b[i0] + fr * (r.b[i0 + 1] - r.b[i0]);
                    x += r.ff * (y - x);
                }
                if (outWarp) x = warpOutput (x, r.warp, r.warpAmt);

                sumL += x * r.gl[u];
                sumR += x * r.gr[u];
                mono += x;

                float ph = os.phase[u] + r.inc[u];
                if (ph >= 1.0f) ph -= 1.0f;
                os.phase[u] = ph;
            }
            os.lastOut = mono / (float) r.voices;
            os.gainL += r.stepL; os.gainR += r.stepR;
            route (r.route, sumL * os.gainL, sumR * os.gainR);
        }

        if (s.sub.on)
        {
            const float t = subPhase;
            float x;
            switch (s.sub.shape)
            {
                case 0:  x = std::sin (kTwoPi * t); break;
                case 1:  x = 4.0f * std::abs (t - 0.5f) - 1.0f; break;
                case 2:  x = 2.0f * t - 1.0f - polyBlep (t, subInc); break;
                default: x = (t < 0.5f ? 1.0f : -1.0f) + polyBlep (t, subInc) - polyBlep (frac (t + 0.5f), subInc); break;
            }
            subPhase += subInc;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
            subOut = x;
            subGainL += subStepL; subGainR += subStepR;
            route (s.sub.route, x * subGainL, x * subGainR);
        }
        else subOut = 0.0f;

        if (smpActive && ! smpDone)
        {
            const auto& z = *smpZone;
            const int i1 = (int) smpPos;
            const float t = (float) (smpPos - (double) i1);
            const int i0 = std::max (0, i1 - 1), i2 = std::min (z.length - 1, i1 + 1), i3 = std::min (z.length - 1, i1 + 2);
            auto hermite = [t] (float y0, float y1, float y2, float y3)
            {
                const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                return ((c3 * t + c2) * t + c1) * t + y1;
            };
            const float xl = hermite (z.frameL (i0), z.frameL (i1), z.frameL (i2), z.frameL (i3));
            const float xr = hermite (z.frameR (i0), z.frameR (i1), z.frameR (i2), z.frameR (i3));
            smpGainL += smpStepL; smpGainR += smpStepR;
            route (s.smp.route, xl * smpGainL, xr * smpGainR);
            smpPos += smpInc;
            if (z.loopEnd > z.loopStart)
            {
                while (smpPos >= (double) z.loopEnd) smpPos -= (double) (z.loopEnd - z.loopStart);
            }
            else if (smpPos >= (double) (z.length - 2)) smpDone = true;
        }

        if (s.noise.on)
        {
            const float w = rng.bi();
            float x;
            switch (s.noise.type)
            {
                case 1:
                    pink[0] = 0.99765f * pink[0] + w * 0.0990460f;
                    pink[1] = 0.96300f * pink[1] + w * 0.2965164f;
                    pink[2] = 0.57000f * pink[2] + w * 1.0526913f;
                    x = (pink[0] + pink[1] + pink[2] + w * 0.1848f) * 0.3f;
                    break;
                case 2:
                    brown = (brown + 0.02f * w) / 1.02f;
                    x = brown * 3.5f;
                    break;
                case 3:
                    noiseHold *= 0.93f;
                    if (rng.uni() < 40.0f / sr) noiseHold = rng.bi();
                    x = noiseHold;
                    break;
                case 4:
                    if (--noiseCounter <= 0) { noiseHold = rng.bi(); noiseCounter = 3 + (int) (rng.next() % 14u); }
                    x = noiseHold;
                    break;
                default: x = w; break;
            }
            noiseLp += (x - noiseLp) * noiseLpCoef;
            if (color < 0.0f) x += (noiseLp - x) * -color;
            else              x += ((x - noiseLp) - x) * color;
            noiseOut = x;
            noiseGainL += noiseStepL; noiseGainR += noiseStepR;
            route (s.noise.route, x * noiseGainL, x * noiseGainR);
        }
        else noiseOut = 0.0f;

        // ---- filters
        if (f1On) filters[0].process (b1L, b1R);
        float yL, yR;
        if (serial)
        {
            b2L += b1L; b2R += b1R;
            if (f2On) filters[1].process (b2L, b2R);
            yL = b2L + dL; yR = b2R + dR;
        }
        else
        {
            if (f2On) filters[1].process (b2L, b2R);
            yL = b1L + b2L + dL; yR = b1R + b2R + dR;
        }

        if (s.circuit > 0.0f)
        {   // analog circuit: soft, slightly asymmetric saturation
            const float cg = 1.0f + s.circuit * 3.5f, cc = 1.0f / (1.0f + s.circuit * 1.6f), bias = s.circuit * 0.08f;
            yL = (fastTanh (yL * cg + bias) - fastTanh (bias)) * cc;
            yR = (fastTanh (yR * cg + bias) - fastTanh (bias)) * cc;
        }

        ampGain += ampStep;
        const float a = ampEnv.next (s.env[0], timeScale) * ampGain * kVoiceGain;
        outL[i] += yL * a;
        outR[i] += yR * a;
    }

    lastEnv[0] = ampEnv.getValue();
    for (int e = 1; e < kNumEnv; ++e) lastEnv[e] = modEnv[e - 1].getValue();
    for (int l = 0; l < kNumLfo; ++l) lastLfo[l] = lfos[l].getValue();

    if (! ampEnv.isActive())
        kill();
}

} // namespace pyra
