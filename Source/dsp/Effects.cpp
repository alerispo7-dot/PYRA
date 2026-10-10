#include "Effects.h"
#include "../Params.h"

namespace pyra
{
//==============================================================================
void Biquad::setPeak (float sr, float f, float q, float db)
{
    const float A = std::pow (10.0f, db / 40.0f);
    const float w = kTwoPi * std::min (f, sr * 0.45f) / sr;
    const float alpha = std::sin (w) / (2.0f * q);
    const float c = std::cos (w);
    const float a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0; b1 = (-2.0f * c) / a0; b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * c) / a0; a2 = (1.0f - alpha / A) / a0;
}

void Biquad::setLowShelf (float sr, float f, float db)
{
    const float A = std::pow (10.0f, db / 40.0f);
    const float w = kTwoPi * std::min (f, sr * 0.45f) / sr;
    const float c = std::cos (w), s = std::sin (w);
    const float alpha = s / 2.0f * std::sqrt (2.0f);
    const float sq = 2.0f * std::sqrt (A) * alpha;
    const float a0 = (A + 1) + (A - 1) * c + sq;
    b0 = A * ((A + 1) - (A - 1) * c + sq) / a0;
    b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
    b2 = A * ((A + 1) - (A - 1) * c - sq) / a0;
    a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
    a2 = ((A + 1) + (A - 1) * c - sq) / a0;
}

void Biquad::setHighShelf (float sr, float f, float db)
{
    const float A = std::pow (10.0f, db / 40.0f);
    const float w = kTwoPi * std::min (f, sr * 0.45f) / sr;
    const float c = std::cos (w), s = std::sin (w);
    const float alpha = s / 2.0f * std::sqrt (2.0f);
    const float sq = 2.0f * std::sqrt (A) * alpha;
    const float a0 = (A + 1) - (A - 1) * c + sq;
    b0 = A * ((A + 1) + (A - 1) * c + sq) / a0;
    b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
    b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
    a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
    a2 = ((A + 1) - (A - 1) * c - sq) / a0;
}

//==============================================================================
void FxChain::init (juce::AudioProcessorValueTreeState& st)
{
    auto p = [&st] (const char* id) { auto* v = st.getRawParameterValue (id); jassert (v != nullptr); return v; };

    pd   = { p ("fx_dist_on"), p ("fx_dist_type"), p ("fx_dist_drive"), p ("fx_dist_tone"), p ("fx_dist_out"), p ("fx_dist_mix") };
    pr   = { p ("fx_res_on"), p ("fx_res_chord"), p ("fx_res_note"), p ("fx_res_decay"), p ("fx_res_bright"), p ("fx_res_mix") };
    pf   = { p ("fx_flt_on"), p ("fx_flt_type"), p ("fx_flt_cutoff"), p ("fx_flt_res"), p ("fx_flt_drive"), p ("fx_flt_mix") };
    pc   = { p ("fx_cho_on"), p ("fx_cho_rate"), p ("fx_cho_depth"), p ("fx_cho_delay"), p ("fx_cho_fb"), p ("fx_cho_width"), p ("fx_cho_mix") };
    pl   = { p ("fx_fla_on"), p ("fx_fla_rate"), p ("fx_fla_depth"), p ("fx_fla_fb"), p ("fx_fla_mix") };
    pp   = { p ("fx_pha_on"), p ("fx_pha_rate"), p ("fx_pha_depth"), p ("fx_pha_freq"), p ("fx_pha_fb"), p ("fx_pha_stages"), p ("fx_pha_mix") };
    pt   = { p ("fx_tape_on"), p ("fx_tape_wow"), p ("fx_tape_flutter"), p ("fx_tape_sat"), p ("fx_tape_tone"), p ("fx_tape_mix") };
    pdl  = { p ("fx_dly_on"), p ("fx_dly_mode"), p ("fx_dly_sync"), p ("fx_dly_div"), p ("fx_dly_time"), p ("fx_dly_fb"),
             p ("fx_dly_lowcut"), p ("fx_dly_highcut"), p ("fx_dly_width"), p ("fx_dly_mix") };
    pv   = { p ("fx_rev_on"), p ("fx_rev_type"), p ("fx_rev_size"), p ("fx_rev_decay"), p ("fx_rev_pre"), p ("fx_rev_damp"),
             p ("fx_rev_width"), p ("fx_rev_shim"), p ("fx_rev_mix") };
    pe   = { p ("fx_eq_on"), p ("fx_eq_lowf"), p ("fx_eq_lowg"), p ("fx_eq_midf"), p ("fx_eq_midg"), p ("fx_eq_midq"),
             p ("fx_eq_highf"), p ("fx_eq_highg") };
    pcmp = { p ("fx_comp_on"), p ("fx_comp_thresh"), p ("fx_comp_ratio"), p ("fx_comp_att"), p ("fx_comp_rel"),
             p ("fx_comp_makeup"), p ("fx_comp_mix") };
}

void FxChain::prepare (double sampleRate, int maxBlock)
{
    sr = (float) sampleRate;
    maxChunk = std::max (64, std::min (maxBlock, 2048));
    tmpL.assign ((size_t) maxChunk, 0.0f);
    tmpR.assign ((size_t) maxChunk, 0.0f);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxChunk);
    distLatency = (int) std::lround (oversampler->getLatencyInSamples());
    for (auto& d : distDry) d.prepare (256);

    const int resMax = (int) (sr / 25.0f) + 8;
    for (auto& r : resLine) for (auto& d : r) d.prepare (resMax);
    for (auto& d : choLine) d.prepare ((int) (sr * 0.06f));
    for (auto& d : flaLine) d.prepare ((int) (sr * 0.02f));
    for (auto& d : tapeLine) d.prepare ((int) (sr * 0.1f));
    for (auto& d : dlyLine) d.prepare ((int) (sr * 4.1f));
    for (auto& d : revPre) d.prepare ((int) (sr * 0.3f));
    for (auto& c : revDiff) for (auto& d : c) d.prepare ((int) (sr * 0.03f));
    for (auto& d : revLine) d.prepare ((int) (sr * 0.25f));
    shimLine.prepare ((int) (sr * 0.2f));

    for (auto* m : { &fdDist, &fdRes, &fdFlt, &fdCho, &fdFla, &fdPha, &fdTape, &fdDly, &fdRev, &fdEq, &fdComp })
        m->prepare (sr);
    rng.seed (0x5eed1234u);
    reset();
}

void FxChain::reset()
{
    if (oversampler) oversampler->reset();
    for (auto& d : distDry) d.reset();
    for (auto& r : resLine) for (auto& d : r) d.reset();
    for (auto* d : { &choLine[0], &choLine[1], &flaLine[0], &flaLine[1], &tapeLine[0], &tapeLine[1],
                     &dlyLine[0], &dlyLine[1], &revPre[0], &revPre[1], &shimLine })
        d->reset();
    for (auto& c : revDiff) for (auto& d : c) d.reset();
    for (auto& d : revLine) d.reset();
    std::fill (std::begin (revLp), std::end (revLp), 0.0f);
    for (auto& z : phaZ) std::fill (std::begin (z), std::end (z), 0.0f);
    phaFb[0] = phaFb[1] = choFb[0] = choFb[1] = flaFb[0] = flaFb[1] = 0.0f;
    svf1[0] = svf1[1] = svf2[0] = svf2[1] = 0.0f;
    eqLow.reset(); eqMid.reset(); eqHigh.reset();
    compEnvDb = 0.0f; shimFb = 0.0f; dlyTimeSmoothed = -1.0f;
    for (auto& r : resLp) r[0] = r[1] = 0.0f;
}

void FxChain::process (float* l, float* r, int n, double bpm)
{
    int done = 0;
    while (done < n)
    {
        const int len = std::min (maxChunk, n - done);
        processChunk (l + done, r + done, len, bpm);
        done += len;
    }
}

//==============================================================================
void FxChain::processChunk (float* l, float* r, int n, double bpm)
{
    float* tl = tmpL.data();
    float* tr = tmpR.data();

    // Runs one module on a copy of the signal and blends it in with a click-free fade.
    auto run = [&] (ModuleFade& fade, bool on, auto&& body)
    {
        if (! fade.active (on)) return;
        std::copy (l, l + n, tl);
        std::copy (r, r + n, tr);
        body (tl, tr);
        for (int k = 0; k < n; ++k)
        {
            const float g = fade.next (on);
            l[k] += (tl[k] - l[k]) * g;
            r[k] += (tr[k] - r[k]) * g;
        }
    };

    // ------------------------------------------------------------ Distortion
    run (fdDist, b (pd.on), [&] (float* L, float* R)
    {
        const int type = i (pd.type);
        const float drive = f (pd.drive), mix = f (pd.mix);
        const float pre = juce::Decibels::decibelsToGain (drive * 30.0f);
        const float comp = 1.0f / (1.0f + drive * 1.5f);

        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = oversampler->processSamplesUp (block);
        const int un = (int) up.getNumSamples();
        float* ul = up.getChannelPointer (0);
        float* ur = up.getChannelPointer (1);

        auto shape = [&] (float x) -> float
        {
            const float y = x * pre;
            switch (type)
            {
                case 0: return fastTanh (y) * comp;
                case 1: return std::clamp (y, -1.0f, 1.0f) * comp;
                case 2: return (std::tanh (y + 0.25f) - 0.24491866f) * comp;
                case 3: return std::sin (y * 1.2f) * comp;
                case 4: return (y > 0.0f ? fastTanh (y) : 0.35f * fastTanh (y)) * comp;
                case 5: { const float c = std::clamp (y, -1.0f, 1.0f); return 1.5f * (c - c * c * c / 3.0f) * comp; }
                case 6:
                {
                    const float q = std::exp2 (15.0f - drive * 12.5f);
                    return std::round (x * q) / q;
                }
                default: return x;
            }
        };

        if (type == 7)
        {
            const int factor = 1 + (int) (drive * 40.0f);
            for (int k = 0; k < un; ++k)
            {
                if (--holdCount <= 0) { holdL = ul[k]; holdR = ur[k]; holdCount = factor; }
                ul[k] = holdL; ur[k] = holdR;
            }
        }
        else
            for (int k = 0; k < un; ++k) { ul[k] = shape (ul[k]); ur[k] = shape (ur[k]); }

        oversampler->processSamplesDown (block);

        const float toneA = OnePole::coef (800.0f * std::pow (25.0f, f (pd.tone)), sr);
        const float dcA = OnePole::coef (12.0f, sr);
        const float out = juce::Decibels::decibelsToGain (f (pd.out));
        for (int k = 0; k < n; ++k)
        {
            distDry[0].push (l[k]); distDry[1].push (r[k]);
            const float dl = distDry[0].read ((float) distLatency + 1.0f);
            const float dr = distDry[1].read ((float) distLatency + 1.0f);
            const float wl = distDc[0].hp (distTone[0].lp (L[k], toneA), dcA) * out;
            const float wr = distDc[1].hp (distTone[1].lp (R[k], toneA), dcA) * out;
            L[k] = dl + (wl - dl) * mix;
            R[k] = dr + (wr - dr) * mix;
        }
    });

    // ------------------------------------------------------------ Resonator
    run (fdRes, b (pr.on), [&] (float* L, float* R)
    {
        static const std::vector<int> chordTable[] = { { 0 }, { 0, 12, 24 }, { 0, 7, 12 }, { 0, 4, 7, 12 }, { 0, 3, 7, 12 },
                                                       { 0, 5, 7, 12 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 7, 12, 19, 24 } };
        const auto& chord = chordTable[juce::jlimit (0, 8, i (pr.chord))];
        const int count = std::min (5, (int) chord.size());
        const float fb = 0.8f + f (pr.decay) * 0.195f;
        const float damp = 0.15f + f (pr.bright) * 0.84f;
        const float mix = f (pr.mix);
        const float norm = 1.0f / std::sqrt ((float) count);
        float delays[5][2];
        for (int c = 0; c < count; ++c)
            for (int ch = 0; ch < 2; ++ch)
            {
                const float hz = midiToHz ((float) (i (pr.note) + chord[(size_t) c]) + (ch == 1 ? 0.04f : 0.0f));
                delays[c][ch] = std::clamp (sr / hz, 2.0f, (float) resLine[c][ch].capacity() - 2.0f);
            }

        for (int k = 0; k < n; ++k)
        {
            float in[2] = { L[k], R[k] }, out[2] = { 0, 0 };
            for (int c = 0; c < count; ++c)
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& line = resLine[c][ch];
                    const float d = line.read (delays[c][ch]);
                    resLp[c][ch] += damp * (d - resLp[c][ch]);
                    const float y = in[ch] * 0.3f + fb * resLp[c][ch];
                    line.push (fastTanh (y));
                    out[ch] += y;
                }
            const float wl = fastTanh (out[0] * norm), wr = fastTanh (out[1] * norm);
            L[k] = in[0] + (wl - in[0]) * mix;
            R[k] = in[1] + (wr - in[1]) * mix;
        }
    });

    // ------------------------------------------------------------ FX filter
    run (fdFlt, b (pf.on), [&] (float* L, float* R)
    {
        const int type = i (pf.type);
        const float g = std::tan (kPi * std::min (f (pf.cutoff), sr * 0.45f) / sr);
        const float k = 2.0f - 1.96f * f (pf.res);
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float drive = f (pf.drive), mix = f (pf.mix);
        const float dg = 1.0f + drive * 6.0f;
        float* io[2] = { L, R };
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < n; ++s)
            {
                const float dry = io[ch][s];
                const float x = drive > 0.0f ? fastTanh (dry * dg) / (1.0f + drive) : dry;
                const float v3 = x - svf2[ch];
                const float v1 = a1 * svf1[ch] + a2 * v3;
                const float v2 = svf2[ch] + a2 * svf1[ch] + a3 * v3;
                svf1[ch] = 2.0f * v1 - svf1[ch];
                svf2[ch] = 2.0f * v2 - svf2[ch];
                float y;
                switch (type)
                {
                    case 1:  y = x - k * v1 - v2; break;
                    case 2:  y = v1 * k; break;
                    case 3:  y = x - k * v1; break;
                    default: y = v2; break;
                }
                io[ch][s] = dry + (y - dry) * mix;
            }
    });

    // ------------------------------------------------------------ Chorus
    run (fdCho, b (pc.on), [&] (float* L, float* R)
    {
        const float inc = f (pc.rate) / sr;
        const float base = f (pc.delay) * 0.001f * sr;
        const float depth = f (pc.depth) * 0.006f * sr;
        const float fb = f (pc.fb) * 0.8f, width = f (pc.width), mix = f (pc.mix);
        for (int k = 0; k < n; ++k)
        {
            choPhase += inc; if (choPhase >= 1.0f) choPhase -= 1.0f;
            float wet[2];
            const float in[2] = { L[k], R[k] };
            for (int ch = 0; ch < 2; ++ch)
            {
                float sum = 0.0f;
                for (int t = 0; t < 3; ++t)
                {
                    const float ph = choPhase + (float) t / 3.0f + (ch == 1 ? 0.25f * width : 0.0f);
                    const float d = base + depth * (0.5f + 0.5f * std::sin (kTwoPi * ph));
                    sum += choLine[ch].read (std::max (2.0f, d));
                }
                wet[ch] = sum / 3.0f;
                choLine[ch].push (in[ch] + choFb[ch] * fb);
                choFb[ch] = wet[ch];
            }
            const float mid = 0.5f * (wet[0] + wet[1]);
            const float wl = mid + (wet[0] - mid) * width, wr = mid + (wet[1] - mid) * width;
            L[k] = in[0] + (wl - in[0]) * mix;
            R[k] = in[1] + (wr - in[1]) * mix;
        }
    });

    // ------------------------------------------------------------ Flanger
    run (fdFla, b (pl.on), [&] (float* L, float* R)
    {
        const float inc = f (pl.rate) / sr;
        const float minD = 0.0003f * sr, range = f (pl.depth) * 0.0075f * sr;
        const float fb = f (pl.fb), mix = f (pl.mix);
        for (int k = 0; k < n; ++k)
        {
            flaPhase += inc; if (flaPhase >= 1.0f) flaPhase -= 1.0f;
            const float in[2] = { L[k], R[k] };
            float out[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float ph = flaPhase + (ch == 1 ? 0.25f : 0.0f);
                const float d = minD + range * (0.5f + 0.5f * std::sin (kTwoPi * ph));
                const float y = flaLine[ch].read (std::max (1.5f, d));
                flaLine[ch].push (fastTanh (in[ch] + fb * flaFb[ch]));
                flaFb[ch] = y;
                out[ch] = y;
            }
            L[k] = in[0] + (out[0] - in[0]) * mix;
            R[k] = in[1] + (out[1] - in[1]) * mix;
        }
    });

    // ------------------------------------------------------------ Phaser
    run (fdPha, b (pp.on), [&] (float* L, float* R)
    {
        const float inc = f (pp.rate) / sr;
        const int stages = juce::jlimit (2, 12, i (pp.stages));
        const float centre = f (pp.freq), depth = f (pp.depth), fb = f (pp.fb), mix = f (pp.mix);
        for (int k = 0; k < n; ++k)
        {
            phaPhase += inc; if (phaPhase >= 1.0f) phaPhase -= 1.0f;
            const float in[2] = { L[k], R[k] };
            float out[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float ph = phaPhase + (ch == 1 ? 0.25f : 0.0f);
                const float hz = std::clamp (centre * std::exp2 (depth * 2.5f * std::sin (kTwoPi * ph)), 20.0f, sr * 0.45f);
                const float t = std::tan (kPi * hz / sr);
                const float a = (t - 1.0f) / (t + 1.0f);
                float x = in[ch] + fb * phaFb[ch];
                for (int s = 0; s < stages; ++s)
                {
                    const float y = a * x + phaZ[ch][s];
                    phaZ[ch][s] = x - a * y;
                    x = y;
                }
                phaFb[ch] = fastTanh (x);
                out[ch] = 0.5f * (in[ch] + x);
            }
            L[k] = in[0] + (out[0] - in[0]) * mix;
            R[k] = in[1] + (out[1] - in[1]) * mix;
        }
    });

    // ------------------------------------------------------------ Tape wow & flutter
    run (fdTape, b (pt.on), [&] (float* L, float* R)
    {
        const float wow = f (pt.wow), flut = f (pt.flutter), sat = f (pt.sat), mix = f (pt.mix);
        const float lpA = OnePole::coef (3500.0f * std::pow (5.5f, f (pt.tone)), sr);
        const float satGain = 1.0f + sat * 4.0f;
        const float satComp = 1.0f / (1.0f + sat * 1.2f);
        const float dt = 1.0f / sr;
        for (int k = 0; k < n; ++k)
        {
            wowPhase += 0.7f * dt; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            flutPhase1 += 6.3f * dt; if (flutPhase1 >= 1.0f) flutPhase1 -= 1.0f;
            flutPhase2 += 10.7f * dt; if (flutPhase2 >= 1.0f) flutPhase2 -= 1.0f;
            wowTimer -= dt;
            if (wowTimer <= 0.0f) { wowTarget = rng.bi(); wowTimer = 0.3f + rng.uni() * 0.8f; }
            wowRand += (wowTarget - wowRand) * 2.0f * dt;

            const float mod = wow * 0.0030f * sr * (0.5f + 0.35f * std::sin (kTwoPi * wowPhase) + 0.15f * wowRand)
                            + flut * 0.00025f * sr * (0.6f * std::sin (kTwoPi * flutPhase1) + 0.4f * std::sin (kTwoPi * flutPhase2));
            const float d = 0.008f * sr + mod;
            const float in[2] = { L[k], R[k] };
            float out[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                tapeLine[ch].push (in[ch]);
                float y = tapeLine[ch].read (d + (ch == 1 ? 0.00004f * sr : 0.0f));
                y = (fastTanh (y * satGain + 0.05f * sat) - fastTanh (0.05f * sat)) * satComp;
                out[ch] = tapeLp[ch].lp (y, lpA);
            }
            L[k] = in[0] + (out[0] - in[0]) * mix;
            R[k] = in[1] + (out[1] - in[1]) * mix;
        }
    });

    // ------------------------------------------------------------ Delay
    run (fdDly, b (pdl.on), [&] (float* L, float* R)
    {
        const int mode = i (pdl.mode);
        const double beats = choices::syncDivisionBeats (i (pdl.div));
        const float seconds = b (pdl.sync) ? (float) (beats * 60.0 / std::max (20.0, bpm)) : f (pdl.time) * 0.001f;
        const float target = std::clamp (seconds * sr, 1.0f, (float) dlyLine[0].capacity() - 4.0f);
        if (dlyTimeSmoothed < 0.0f) dlyTimeSmoothed = target;
        const float fb = f (pdl.fb), width = f (pdl.width), mix = f (pdl.mix);
        const float hpA = OnePole::coef (f (pdl.lowcut), sr);
        const float lpA = OnePole::coef (f (pdl.highcut) * (mode == 1 ? 0.55f : 1.0f), sr);
        const float glide = 1.0f - std::exp (-1.0f / (0.12f * sr));
        for (int k = 0; k < n; ++k)
        {
            dlyTimeSmoothed += (target - dlyTimeSmoothed) * glide;
            float d = dlyTimeSmoothed;
            if (mode == 1)
            {
                dlyWowPhase += 0.9f / sr; if (dlyWowPhase >= 1.0f) dlyWowPhase -= 1.0f;
                d *= 1.0f + 0.003f * std::sin (kTwoPi * dlyWowPhase);
            }
            const float in[2] = { L[k], R[k] };
            const float yl = dlyLine[0].read (d);
            const float yr = dlyLine[1].read (mode == 2 ? d : d * (1.0f + width * 0.06f));

            auto loop = [&] (int ch, float y)
            {
                y = dlyLp[ch].lp (dlyHp[ch].hp (y, hpA), lpA);
                return mode == 1 ? fastTanh (y * 1.3f) / 1.1f : fastTanh (y);
            };

            if (mode == 2)
            {   // ping-pong: mono input enters left, echoes alternate sides
                dlyLine[0].push (0.5f * (in[0] + in[1]) + fb * loop (1, yr));
                dlyLine[1].push (fb * loop (0, yl));
            }
            else
            {
                dlyLine[0].push (in[0] + fb * loop (0, yl));
                dlyLine[1].push (in[1] + fb * loop (1, yr));
            }

            const float mid = 0.5f * (yl + yr);
            const float wl = mid + (yl - mid) * (0.4f + width * 0.6f);
            const float wr = mid + (yr - mid) * (0.4f + width * 0.6f);
            L[k] = in[0] + wl * mix;   // delay adds echoes on top of the dry sound
            R[k] = in[1] + wr * mix;
        }
    });

    // ------------------------------------------------------------ Reverb (8-line FDN)
    run (fdRev, b (pv.on), [&] (float* L, float* R)
    {
        const int type = i (pv.type);
        const float typeScale = type == 0 ? 0.55f : (type == 1 ? 1.15f : 1.3f);
        const float size = (0.35f + f (pv.size) * 1.4f) * typeScale;
        const float rt = std::max (0.1f, f (pv.decay) * (type == 0 ? 0.6f : 1.0f));
        static const float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 67.7f };
        float len[8], g[8];
        for (int j = 0; j < 8; ++j)
        {
            len[j] = std::min (baseMs[j] * size * 0.001f * sr, (float) revLine[j].capacity() - 40.0f);
            g[j] = std::pow (10.0f, -3.0f * len[j] / (sr * rt));
        }
        const float dampA = OnePole::coef (18000.0f * std::pow (0.06f, f (pv.damp)), sr);
        const float pre = std::max (1.0f, f (pv.pre) * 0.001f * sr);
        const float width = f (pv.width), mix = f (pv.mix);
        const float shim = type == 2 ? f (pv.shim) * 0.55f : 0.0f;
        static const float diffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        const float shimWin = 0.06f * sr;

        for (int k = 0; k < n; ++k)
        {
            const float in[2] = { L[k], R[k] };
            float x[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                revPre[ch].push (in[ch]);
                float v = revPre[ch].read (pre);
                for (int a = 0; a < 4; ++a)
                {   // series all-pass diffusion
                    const float dl = diffMs[a] * 0.001f * sr * (ch == 1 ? 1.07f : 1.0f);
                    const float delayed = revDiff[ch][a].read (dl);
                    const float w = v + 0.6f * delayed;
                    revDiff[ch][a].push (w);
                    v = delayed - 0.6f * w;
                }
                x[ch] = v;
            }

            // read the 8 lines (4 of them slowly modulated against metallic ringing)
            float y[8];
            for (int j = 0; j < 8; ++j)
            {
                float d = len[j];
                if (j < 4)
                {
                    revModPhase[j] += (0.3f + 0.17f * (float) j) / sr;
                    if (revModPhase[j] >= 1.0f) revModPhase[j] -= 1.0f;
                    d += 0.0006f * sr * std::sin (kTwoPi * revModPhase[j]);
                }
                y[j] = revLine[j].read (d);
                revLp[j] += dampA * (y[j] - revLp[j]);
                y[j] = revLp[j];
            }

            // Hadamard mixing
            float h[8];
            for (int j = 0; j < 8; ++j) h[j] = y[j];
            for (int s = 1; s < 8; s <<= 1)
                for (int a = 0; a < 8; a += s << 1)
                    for (int c = a; c < a + s; ++c)
                    {
                        const float u = h[c], v = h[c + s];
                        h[c] = u + v; h[c + s] = u - v;
                    }
            const float hn = 0.35355339f;

            // shimmer: octave-up pitch shifter fed back into the tank
            float shimOut = 0.0f;
            if (shim > 0.0f)
            {
                shimLine.push (shimFb);
                shimPhase += 1.0f / shimWin;   // pitch ratio 2 => read pointer advances 1 extra sample per sample
                if (shimPhase >= 1.0f) shimPhase -= 1.0f;
                const float p2 = shimPhase + 0.5f >= 1.0f ? shimPhase - 0.5f : shimPhase + 0.5f;
                const float d1 = 2.0f + (1.0f - shimPhase) * shimWin, d2 = 2.0f + (1.0f - p2) * shimWin;
                shimOut = shimLine.read (d1) * std::sin (kPi * shimPhase) + shimLine.read (d2) * std::sin (kPi * p2);
            }

            for (int j = 0; j < 8; ++j)
            {
                const float inj = (j & 1) ? x[1] : x[0];
                // gentle saturation keeps the tank bounded even with long decays + shimmer
                revLine[j].push (2.0f * fastTanh (0.5f * (h[j] * hn * g[j] * (1.0f - shim * 0.3f) + inj * 0.5f + shimOut * shim * 0.35f)));
            }

            float ol = (y[0] - y[2] + y[4] - y[6]) * 0.5f;
            float orr = (y[1] - y[3] + y[5] - y[7]) * 0.5f;
            shimFb = fastTanh (0.5f * (ol + orr));
            const float mid = 0.5f * (ol + orr);
            ol = mid + (ol - mid) * width;
            orr = mid + (orr - mid) * width;
            L[k] = in[0] * (1.0f - mix * 0.5f) + ol * mix;
            R[k] = in[1] * (1.0f - mix * 0.5f) + orr * mix;
        }
    });

    // ------------------------------------------------------------ EQ
    run (fdEq, b (pe.on), [&] (float* L, float* R)
    {
        const float v[7] = { f (pe.lowf), f (pe.lowg), f (pe.midf), f (pe.midg), f (pe.midq), f (pe.highf), f (pe.highg) };
        if (! std::equal (std::begin (v), std::end (v), std::begin (eqCache)))
        {
            std::copy (std::begin (v), std::end (v), std::begin (eqCache));
            eqLow.setLowShelf (sr, v[0], v[1]);
            eqMid.setPeak (sr, v[2], v[4], v[3]);
            eqHigh.setHighShelf (sr, v[5], v[6]);
        }
        for (int k = 0; k < n; ++k)
        {
            L[k] = eqHigh.process (0, eqMid.process (0, eqLow.process (0, L[k])));
            R[k] = eqHigh.process (1, eqMid.process (1, eqLow.process (1, R[k])));
        }
    });

    // ------------------------------------------------------------ Compressor
    {
        const bool on = b (pcmp.on);
        if (! fdComp.active (on)) gainReductionDb = 0.0f;
        run (fdComp, on, [&] (float* L, float* R)
        {
            const float thr = f (pcmp.thresh), ratio = std::max (1.0f, f (pcmp.ratio));
            const float att = std::exp (-1.0f / (std::max (0.1f, f (pcmp.att)) * 0.001f * sr));
            const float rel = std::exp (-1.0f / (std::max (5.0f, f (pcmp.rel)) * 0.001f * sr));
            const float makeup = juce::Decibels::decibelsToGain (f (pcmp.makeup));
            const float mix = f (pcmp.mix);
            const float knee = 6.0f;
            float maxGr = 0.0f;
            for (int k = 0; k < n; ++k)
            {
                const float peakIn = std::max (std::abs (L[k]), std::abs (R[k]));
                const float lvl = juce::Decibels::gainToDecibels (peakIn, -100.0f);
                const float over = lvl - thr;
                float gr = 0.0f;   // dB of reduction (positive)
                if (over > knee * 0.5f) gr = over * (1.0f - 1.0f / ratio);
                else if (over > -knee * 0.5f) { const float t = over + knee * 0.5f; gr = (1.0f - 1.0f / ratio) * t * t / (2.0f * knee); }
                compEnvDb = gr > compEnvDb ? att * compEnvDb + (1.0f - att) * gr : rel * compEnvDb + (1.0f - rel) * gr;
                maxGr = std::max (maxGr, compEnvDb);
                const float gain = juce::Decibels::decibelsToGain (-compEnvDb) * makeup;
                L[k] = L[k] * (1.0f - mix) + L[k] * gain * mix;
                R[k] = R[k] * (1.0f - mix) + R[k] * gain * mix;
            }
            gainReductionDb = maxGr;
        });
    }
}

} // namespace pyra
