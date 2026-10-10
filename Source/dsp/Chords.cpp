#include "Chords.h"
#include "Engine.h"
#include "../Params.h"
#include <set>

namespace pyra
{
//==============================================================================
const juce::StringArray& chordQualityNames()
{
    static juce::StringArray a { "Auto (scale)", "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Maj7", "Min7", "Dom7", "Min7b5",
                                 "Add9", "Min9", "Maj9", "Power" };
    return a;
}

const std::vector<double>& chordSlotLengths() { static const std::vector<double> v { 0.25, 0.5, 1.0, 2.0 }; return v; }
const juce::StringArray& chordSlotLengthNames() { static juce::StringArray a { "1/4 x", "1/2 x", "1 x", "2 x" }; return a; }

namespace
{
    const std::vector<int>& qualityIntervals (int q)
    {
        static const std::vector<int> t[QCount] = {
            { 0, 4, 7 }, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 3, 6 }, { 0, 4, 8 }, { 0, 2, 7 }, { 0, 5, 7 }, { 0, 4, 7, 11 },
            { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 3, 6, 10 }, { 0, 4, 7, 14 }, { 0, 3, 7, 10, 14 }, { 0, 4, 7, 11, 14 }, { 0, 7, 12 } };
        return t[juce::jlimit (0, QCount - 1, q)];
    }

    const std::vector<int>& scaleFor (int scale)
    {
        const auto& st = choices::scaleSteps (scale);
        static const std::vector<int> major { 0, 2, 4, 5, 7, 9, 11 };
        return st.size() >= 12 ? major : st;   // chromatic: build chords from the major scale
    }

    /** Semitones (from C) of a scale degree, any octave. */
    int scaleNote (int scale, int key, int degree)
    {
        const auto& st = scaleFor (scale);
        const int n = (int) st.size();
        const int oct = (int) std::floor ((double) degree / n);
        const int idx = degree - oct * n;
        return key + st[(size_t) idx] + 12 * oct;
    }

    uint32_t hash3 (uint32_t a, uint32_t b, uint32_t c)
    {
        uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
        return h;
    }

    float hashUni (uint32_t h) { return (float) (h >> 8) * (1.0f / 16777216.0f); }

    /** Tones of a slot relative to C (not yet placed in an octave), plus the root. */
    std::vector<int> slotTones (const ChordPattern& p, int slot, const ChordSettings& s, int& rootRel)
    {
        const auto& sl = p.slots[(size_t) juce::jlimit (0, kMaxChordSlots - 1, slot)];
        const int deg = sl.degree.load(), shift = sl.shift.load(), q = sl.quality.load();
        rootRel = scaleNote (s.scale, s.key, deg) + shift;
        std::vector<int> tones;
        if (q != QAuto)
        {
            for (int iv : qualityIntervals (q)) tones.push_back (rootRel + iv);
            return tones;
        }

        int size = s.size;
        if (size == 5)
        {   // mixed: richer chords more often with higher complexity
            const float u = hashUni (hash3 ((uint32_t) slot, (uint32_t) (deg + 64), 77u));
            const float c = s.complexity;
            size = u < 1.0f - c * 0.75f ? 0 : (u < 1.0f - c * 0.25f ? 1 : (u < 1.0f - c * 0.1f ? 2 : 3));
        }
        auto tone = [&] (int k) { return scaleNote (s.scale, s.key, deg + k) + shift; };
        switch (size)
        {
            case 1:  tones = { tone (0), tone (2), tone (4), tone (6) }; break;
            case 2:  tones = { tone (0), tone (2), tone (4), tone (6), tone (8) }; break;
            case 3:  tones = { tone (0), tone (3), tone (4) }; break;
            case 4:  tones = { rootRel, rootRel + 7, rootRel + 12 }; break;
            default: tones = { tone (0), tone (2), tone (4) }; break;
        }
        return tones;
    }
}

bool chordUseFlats (int scale, int key)
{
    // spell the scale both ways and keep the one that uses each letter once (C minor = Eb Ab Bb, not D# G# A#)
    static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const auto& st = scaleFor (scale);
    auto letters = [&] (const char* const* names)
    {
        std::set<char> l;
        for (int x : st) l.insert (names[(key + x) % 12][0]);
        return (int) l.size();
    };
    const int sh = letters (sharpNames), fl = letters (flatNames);
    if (fl != sh) return fl > sh;
    return key == 5 || key == 10 || key == 3 || key == 8 || key == 1;   // F, Bb, Eb, Ab, Db
}

juce::String pitchClassName (int pc, bool flats)
{
    static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pc = ((pc % 12) + 12) % 12;
    return flats ? flatNames[pc] : sharpNames[pc];
}

int chordRootPc (const ChordPattern& p, int slot, const ChordSettings& s)
{
    int root = 0;
    slotTones (p, slot, s, root);
    return ((root % 12) + 12) % 12;
}

std::vector<int> chordNotes (const ChordPattern& p, int slot, const ChordSettings& s, const std::vector<int>& previous)
{
    int rootRel = 0;
    auto tones = slotTones (p, slot, s, rootRel);
    const int rootPc = ((rootRel % 12) + 12) % 12;
    const int rootAbs = 53 + ((rootPc - 5) % 12 + 12) % 12;   // root between F3 and E4
    std::vector<int> closeV;
    for (int t : tones) closeV.push_back (rootAbs + (t - rootRel));
    std::sort (closeV.begin(), closeV.end());
    closeV.erase (std::unique (closeV.begin(), closeV.end()), closeV.end());

    auto invert = [] (std::vector<int> v, int inv)
    {
        for (int k = 0; k < inv && ! v.empty(); ++k) { v.push_back (v.front() + 12); v.erase (v.begin()); }
        return v;
    };

    std::vector<int> notes;
    const int n = (int) closeV.size();
    if (s.inversion == 0 && ! previous.empty())
    {   // smooth: the inversion and octave closest to the previous chord
        double prevAvg = 0; for (int x : previous) prevAvg += x; prevAvg /= (double) previous.size();
        const int prevTop = *std::max_element (previous.begin(), previous.end());
        double best = 1e9;
        for (int inv = 0; inv < n; ++inv)
            for (int o : { -12, 0, 12 })
            {
                auto v = invert (closeV, inv);
                for (auto& x : v) x += o;
                double avg = 0; for (int x : v) avg += x; avg /= (double) v.size();
                double cost = std::abs (avg - prevAvg) + 0.5 * std::abs (v.back() - prevTop);
                if (avg < 54.0) cost += (54.0 - avg) * 2.0;
                if (avg > 72.0) cost += (avg - 72.0) * 2.0;
                if (cost < best) { best = cost; notes = v; }
            }
    }
    else
        notes = invert (closeV, s.inversion <= 1 ? 0 : std::min (s.inversion - 1, n - 1));

    // voicing
    std::sort (notes.begin(), notes.end());
    if (notes.size() >= 3)
    {
        switch (s.voicing)
        {
            case 1: notes[1] += 12; break;                                            // open
            case 2:
            {   // spread: lowest, third voice, then the second voice an octave up
                std::vector<int> v { notes[0], notes[2], notes[1] + 12 };
                for (size_t k = 3; k < notes.size(); ++k) v.push_back (notes[k] + 12);
                notes = v;
                break;
            }
            case 3: notes[notes.size() - 2] -= 12; break;                              // drop 2
            default: break;
        }
        std::sort (notes.begin(), notes.end());
    }

    if (s.bass && ! notes.empty())
    {
        int b = notes.front() - 5;
        b -= ((b - rootPc) % 12 + 12) % 12;   // the root, at least a fourth under the chord
        notes.insert (notes.begin(), b);
    }
    for (auto& x : notes) x = juce::jlimit (0, 127, x + 12 * s.octave);
    std::sort (notes.begin(), notes.end());
    notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
    return notes;
}

juce::String chordName (const ChordPattern& p, int slot, const ChordSettings& s)
{
    int rootRel = 0;
    auto tones = slotTones (p, slot, s, rootRel);
    std::set<int> iv;
    for (int t : tones) iv.insert ((((t - rootRel) % 12) + 12) % 12);
    struct Q { std::set<int> set; const char* suffix; };
    static const std::vector<Q> table {
        { { 0, 4, 7 }, "" }, { { 0, 3, 7 }, "m" }, { { 0, 3, 6 }, "dim" }, { { 0, 4, 8 }, "aug" }, { { 0, 2, 7 }, "sus2" },
        { { 0, 5, 7 }, "sus4" }, { { 0, 4, 7, 11 }, "maj7" }, { { 0, 3, 7, 10 }, "m7" }, { { 0, 4, 7, 10 }, "7" },
        { { 0, 3, 6, 10 }, "m7b5" }, { { 0, 3, 6, 9 }, "dim7" }, { { 0, 3, 7, 11 }, "m(maj7)" }, { { 0, 4, 8, 11 }, "maj7#5" },
        { { 0, 2, 4, 7 }, "add9" }, { { 0, 2, 3, 7 }, "m(add9)" }, { { 0, 2, 3, 7, 10 }, "m9" }, { { 0, 2, 4, 7, 11 }, "maj9" },
        { { 0, 2, 4, 7, 10 }, "9" }, { { 0, 1, 3, 6, 10 }, "m7b5b9" }, { { 0, 1, 3, 7, 10 }, "m7b9" }, { { 0, 1, 4, 7, 10 }, "7b9" },
        { { 0, 2, 3, 7, 11 }, "m(maj9)" }, { { 0, 2, 3, 6, 10 }, "m9b5" }, { { 0, 1, 4, 7, 11 }, "maj7b9" }, { { 0, 7 }, "5" },
        { { 0, 5, 7, 10 }, "7sus4" }, { { 0, 2, 4, 8, 11 }, "maj9#5" }, { { 0, 2, 3, 6, 9 }, "dim9" }, { { 0, 1, 3, 7 }, "m(b9)" },
        { { 0, 2, 5, 7 }, "sus2/4" }, { { 0, 4, 6 }, "b5" }, { { 0, 4, 6, 10 }, "7b5" }, { { 0, 5, 6 }, "sus4b5" },
        { { 0, 2, 6 }, "sus2b5" }, { { 0, 1, 5 }, "sus4b9" } };
    const int pc = ((rootRel % 12) + 12) % 12;
    const bool flats = chordUseFlats (s.scale, s.key) || p.slots[(size_t) juce::jlimit (0, kMaxChordSlots - 1, slot)].shift.load() < 0;
    for (auto& q : table)
        if (q.set == iv) return pitchClassName (pc, flats) + q.suffix;
    return pitchClassName (pc, flats) + "*";
}

juce::String chordRoman (const ChordPattern& p, int slot, const ChordSettings& s)
{
    const auto& sl = p.slots[(size_t) juce::jlimit (0, kMaxChordSlots - 1, slot)];
    int rootRel = 0;
    auto tones = slotTones (p, slot, s, rootRel);
    std::set<int> iv;
    for (int t : tones) iv.insert ((((t - rootRel) % 12) + 12) % 12);
    static const char* numerals[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII" };
    const int n = (int) scaleFor (s.scale).size();
    const int d = ((sl.degree.load() % n) + n) % n;
    juce::String r (numerals[juce::jlimit (0, 7, d)]);
    const bool minor = iv.count (3) && ! iv.count (4);
    const bool dim = minor && iv.count (6) && ! iv.count (7);
    if (minor) r = r.toLowerCase();
    if (dim) r << juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
    if (iv.count (4) && iv.count (8) && ! iv.count (7)) r << "+";
    if (iv.count (11)) r << "maj7";
    else if (iv.count (10)) r << "7";
    if (iv.count (2) && iv.size() >= 4) r << "(9)";
    else if (! iv.count (3) && ! iv.count (4)) r << (iv.count (5) ? "sus4" : (iv.count (2) ? "sus2" : "5"));
    const int sh = sl.shift.load();
    if (sh < 0) r = "b" + r;
    if (sh > 0) r = "#" + r;
    return r;
}

std::vector<std::pair<double, double>> chordRhythmHits (int rhythm, double L, float swing)
{
    std::vector<std::pair<double, double>> hits;
    auto addBar = [&] (std::initializer_list<std::pair<double, double>> bar, double barLen)
    {
        for (double b0 = 0.0; b0 < L - 1e-9; b0 += barLen)
            for (auto [o, len] : bar)
                if (b0 + o < L - 1e-9) hits.push_back ({ b0 + o, std::min (len, L - (b0 + o)) });
    };
    switch (rhythm)
    {
        case 1: addBar ({ { 0.0, 1.0 } }, 1.0); break;
        case 2: addBar ({ { 0.0, 0.5 } }, 0.5); break;
        case 3: addBar ({ { 0.5, 0.5 } }, 1.0); break;
        case 4: addBar ({ { 0.0, 1.5 }, { 1.5, 1.5 }, { 3.0, 1.0 } }, 4.0); break;
        case 5: addBar ({ { 0.0, 1.0 }, { 1.5, 0.5 } }, 4.0); break;
        case 6: addBar ({ { 0.0, 0.25 }, { 0.75, 0.25 }, { 1.5, 0.25 }, { 2.5, 0.25 }, { 3.25, 0.25 } }, 4.0); break;
        default: hits.push_back ({ 0.0, L }); break;
    }
    if (swing > 0.0f)
        for (auto& h : hits)
        {
            const double frac = h.first - std::floor (h.first);
            if (std::abs (frac - 0.5) < 1e-6) { h.first += swing * 0.16; h.second = std::max (0.05, h.second - swing * 0.16); }
        }
    return hits;
}

//==============================================================================
void ChordPattern::clear()
{
    static const int def[kMaxChordSlots] = { 0, 5, 2, 6, 0, 3, 4, 0 };   // i VI III VII (works in minor and major)
    for (int k = 0; k < kMaxChordSlots; ++k)
    {
        auto& s = slots[(size_t) k];
        s.degree = def[k]; s.shift = 0; s.quality = QAuto; s.length = 2; s.lock = false;
    }
    ++changeCounter;
}

bool ChordPattern::isDefault() const
{
    static const int def[kMaxChordSlots] = { 0, 5, 2, 6, 0, 3, 4, 0 };
    for (int k = 0; k < kMaxChordSlots; ++k)
    {
        auto& s = slots[(size_t) k];
        if (s.degree.load() != def[k] || s.shift.load() != 0 || s.quality.load() != QAuto || s.length.load() != 2) return false;
    }
    return true;
}

void ChordPattern::randomize (int count, int scale, float complexity, int sizeMode, uint32_t seed)
{
    (void) sizeMode;
    Rng r; r.seed (seed * 2654435761u + 0x1234567u);
    r.next();
    const auto& st = scaleFor (scale);
    const bool sevenNotes = st.size() == 7;
    const bool majorLike = std::find (st.begin(), st.end(), 4) != st.end() && std::find (st.begin(), st.end(), 3) == st.end();
    count = juce::jlimit (1, kMaxChordSlots, count);

    static const std::vector<std::vector<int>> majorT {
        { 0, 4, 5, 3 }, { 5, 3, 0, 4 }, { 0, 5, 3, 4 }, { 1, 4, 0, 0 }, { 0, 3, 5, 4 }, { 0, 3, 0, 4 }, { 3, 0, 4, 5 },
        { 0, 2, 3, 4 }, { 5, 4, 3, 4 }, { 0, 4, 5, 2, 3, 0, 3, 4 }, { 0, 3, 1, 4 }, { 3, 4, 2, 5 } };
    static const std::vector<std::vector<int>> minorT {
        { 0, 5, 2, 6 }, { 0, 3, 6, 2 }, { 0, 5, 3, 4 }, { 0, 6, 5, 6 }, { 0, 3, 4, 0 }, { 0, 2, 6, 5 }, { 5, 6, 0, 0 },
        { 0, 5, 6, 4 }, { 0, 3, 5, 4 }, { 0, 6, 5, 4 }, { 5, 3, 0, 4 }, { 0, 0, 5, 6 } };
    // functional harmony walk: weights for the next degree
    static const float walk[7][7] = {
        { 0, 2, 1, 3, 3, 3, 0.3f }, { 0.5f, 0, 0, 1, 5, 0.5f, 1 }, { 0.5f, 0, 0, 3, 0.5f, 3, 0 }, { 3, 2, 0.5f, 0, 4, 1, 0.5f },
        { 5, 0, 0.5f, 1, 0, 3, 0.3f }, { 1, 3, 1, 4, 2, 0, 0.3f }, { 5, 0, 1, 0, 0, 2, 0 } };
    static const float walkMinor[7][7] = {
        { 0, 0.5f, 2, 3, 2, 3, 3 }, { 0.5f, 0, 0, 1, 4, 0.5f, 1 }, { 1, 0, 0, 2, 0.5f, 3, 2 }, { 3, 0.5f, 1, 0, 3, 1, 2 },
        { 5, 0, 0.5f, 1, 0, 3, 0.5f }, { 1, 0.5f, 2, 3, 2, 0, 3 }, { 4, 0, 3, 0.5f, 0.5f, 2, 0 } };

    std::vector<int> degrees;
    if (sevenNotes && r.uni() > complexity * 0.7f)
    {
        const auto& set = majorLike ? majorT : minorT;
        const auto& t = set[r.next() % (uint32_t) set.size()];
        for (int k = 0; k < count; ++k) degrees.push_back (t[(size_t) (k % (int) t.size())]);
        if (count > (int) t.size() && r.uni() < 0.5f)
            for (int k = (int) t.size(); k < count; ++k) degrees[(size_t) k] = (int) (r.next() % 7u);   // varied second half
    }
    else
    {
        const int n = (int) st.size();
        int d = r.uni() < 0.7f ? 0 : (n >= 6 ? 5 : n - 1);
        for (int k = 0; k < count; ++k)
        {
            degrees.push_back (d);
            if (n == 7)
            {
                const auto& w = majorLike ? walk[d] : walkMinor[d];
                float total = 0; for (int j = 0; j < 7; ++j) total += w[j];
                float u = r.uni() * total;
                int nd = 0;
                for (int j = 0; j < 7; ++j) { u -= w[j]; if (u <= 0) { nd = j; break; } }
                d = nd;
            }
            else d = (int) (r.next() % (uint32_t) n);
        }
        if (count >= 4 && n == 7 && r.uni() < 0.5f) degrees.back() = 4;   // end on the dominant: loops back home
    }

    for (int k = 0; k < count; ++k)
    {
        auto& s = slots[(size_t) k];
        if (s.lock.load()) continue;
        int deg = degrees[(size_t) k], shift = 0, q = QAuto;

        if (sevenNotes && r.uni() < complexity * 0.18f)
        {   // borrowed chords
            if (majorLike)
            {
                const float u = r.uni();
                if (u < 0.4f) { deg = 6; shift = -1; q = QMaj; }        // bVII
                else if (u < 0.75f) { deg = 3; q = QMin; }              // iv
                else { deg = 5; shift = -1; q = QMaj; }                 // bVI
            }
            else
            {
                if (r.uni() < 0.6f) { deg = 4; q = r.uni() < 0.5f ? QMaj : QDom7; }   // major V (harmonic minor)
                else { deg = 1; shift = -1; q = QMaj; }                              // Neapolitan bII
            }
        }
        else if (r.uni() < complexity * 0.45f)
        {   // colour: sevenths, add9, sus
            int rootRel = 0;
            ChordSettings tmp; tmp.scale = scale; tmp.key = 0; tmp.size = 0;
            s.degree = deg; s.shift = 0; s.quality = QAuto;
            auto tones = slotTones (*this, k, tmp, rootRel);
            const int third = tones.size() > 1 ? (((tones[1] - rootRel) % 12) + 12) % 12 : 4;
            const int fifth = tones.size() > 2 ? (((tones[2] - rootRel) % 12) + 12) % 12 : 7;
            const float u = r.uni();
            if (fifth == 6) q = QHalfDim;
            else if (third == 3) q = u < 0.55f ? QMin7 : (u < 0.85f ? QMin9 : QSus4);
            else if (deg == 4) q = u < 0.6f ? QDom7 : QSus4;
            else q = u < 0.45f ? QMaj7 : (u < 0.75f ? QAdd9 : (u < 0.9f ? QSus2 : QMaj9));
        }

        int len = 2;
        if (r.uni() < complexity * 0.3f) len = r.uni() < 0.5f ? 1 : 3;
        s.degree = deg; s.shift = shift; s.quality = q; s.length = len;
    }
    ++changeCounter;
}

juce::var ChordPattern::toVar() const
{
    juce::Array<juce::var> a;
    for (auto& s : slots)
        a.add (juce::Array<juce::var> { s.degree.load(), s.shift.load(), s.quality.load(), s.length.load() });
    return a;
}

void ChordPattern::fromVar (const juce::var& v)
{
    auto* a = v.getArray();
    if (a == nullptr) { clear(); return; }
    for (int k = 0; k < kMaxChordSlots; ++k)
    {
        auto& s = slots[(size_t) k];
        const auto e = k < a->size() ? a->getReference (k) : juce::var();
        if (! e.isArray()) continue;
        s.degree = juce::jlimit (-14, 21, (int) e[0]);
        s.shift = juce::jlimit (-2, 2, (int) e[1]);
        s.quality = juce::jlimit (0, QCount - 1, (int) e[2]);
        s.length = juce::jlimit (0, (int) chordSlotLengths().size() - 1, (int) e[3]);
        s.lock = false;
    }
    ++changeCounter;
}

juce::ValueTree ChordPattern::toTree() const
{
    juce::ValueTree t ("CHORD_PATTERN");
    for (auto& s : slots)
    {
        juce::ValueTree c ("SLOT");
        c.setProperty ("deg", s.degree.load(), nullptr);
        c.setProperty ("shift", s.shift.load(), nullptr);
        c.setProperty ("q", s.quality.load(), nullptr);
        c.setProperty ("len", s.length.load(), nullptr);
        c.setProperty ("lock", s.lock.load(), nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void ChordPattern::fromTree (const juce::ValueTree& t)
{
    if (! t.hasType ("CHORD_PATTERN")) { clear(); return; }
    for (int k = 0; k < kMaxChordSlots && k < t.getNumChildren(); ++k)
    {
        auto c = t.getChild (k);
        auto& s = slots[(size_t) k];
        s.degree = (int) c.getProperty ("deg", 0);
        s.shift = (int) c.getProperty ("shift", 0);
        s.quality = juce::jlimit (0, QCount - 1, (int) c.getProperty ("q", 0));
        s.length = juce::jlimit (0, (int) chordSlotLengths().size() - 1, (int) c.getProperty ("len", 2));
        s.lock = (bool) c.getProperty ("lock", false);
    }
    ++changeCounter;
}

//==============================================================================
juce::MidiFile chordsToMidiFile (const ChordSettings& s, const ChordPattern& pattern, int loops)
{
    constexpr int ppq = 960;
    juce::MidiMessageSequence seq;
    const int count = juce::jlimit (1, kMaxChordSlots, s.count);
    const double base = choices::chordLengthBeats (s.lengthIndex);
    double t0 = 0.0;
    std::vector<int> prev;
    for (int loop = 0; loop < std::max (1, loops); ++loop)
        for (int k = 0; k < count; ++k)
        {
            const double L = base * chordSlotLengths()[(size_t) juce::jlimit (0, 3, pattern.slots[(size_t) k].length.load())];
            auto notes = chordNotes (pattern, k, s, prev);
            prev = notes;
            for (auto [o, len] : chordRhythmHits (s.rhythm, L, s.swing))
                for (size_t j = 0; j < notes.size(); ++j)
                {
                    const double on = t0 + o + (double) j * s.strum * 0.083;
                    const double off = t0 + o + std::max (0.05, len * s.gate);
                    if (on >= off) continue;
                    seq.addEvent (juce::MidiMessage::noteOn (1, notes[j], juce::jlimit (0.05f, 1.0f, s.velocity)), on * ppq);
                    seq.addEvent (juce::MidiMessage::noteOff (1, notes[j]), off * ppq - 1.0);
                }
            t0 += L;
        }
    seq.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (seq);
    return file;
}

//==============================================================================
void ChordGenerator::init (juce::AudioProcessorValueTreeState& st, ChordPattern& pat)
{
    pattern = &pat;
    auto p = [&st] (const char* id) { auto* v = st.getRawParameterValue (id); jassert (v != nullptr); return v; };
    pOn = p ("chord_on"); pMode = p ("chord_mode"); pTrig = p ("chord_trigger"); pKey = p ("chord_key"); pScale = p ("chord_scale");
    pCount = p ("chord_count"); pLen = p ("chord_len"); pSize = p ("chord_size"); pVoicing = p ("chord_voicing");
    pInv = p ("chord_inv"); pRhythm = p ("chord_rhythm"); pOct = p ("chord_oct"); pBass = p ("chord_bass"); pGate = p ("chord_gate");
    pStrum = p ("chord_strum"); pHuman = p ("chord_human"); pVel = p ("chord_vel"); pSwing = p ("chord_swing");
    pComplex = p ("chord_complex");
}

void ChordGenerator::prepare (double sampleRate)
{
    sr = sampleRate;
    held.clear(); held.reserve (128);
    playing.clear(); playing.reserve (512);
    for (auto& k : keyChords) k.clear();
    std::fill (std::begin (counts), std::end (counts), 0);
    active = false; lastKey = -1; internalBeat = 0.0; expectedBeat = -1.0; lastSlot = -1;
    currentSlot = -1; previewLeft = 0; previewNotes.clear();
    rng.seed (0xc0ffee11u);
}

ChordSettings ChordGenerator::readSettings() const
{
    auto i = [] (std::atomic<float>* a) { return (int) std::lround (a->load()); };
    ChordSettings s;
    s.mode = i (pMode); s.trigger = i (pTrig); s.key = i (pKey); s.scale = i (pScale); s.count = i (pCount);
    s.lengthIndex = i (pLen); s.size = i (pSize); s.voicing = i (pVoicing); s.inversion = i (pInv); s.rhythm = i (pRhythm);
    s.octave = i (pOct); s.bass = pBass->load() > 0.5f; s.gate = pGate->load(); s.strum = pStrum->load();
    s.humanize = pHuman->load(); s.velocity = pVel->load(); s.swing = pSwing->load(); s.complexity = pComplex->load();
    return s;
}

void ChordGenerator::noteOn (juce::MidiBuffer& out, int note, float vel, int pos)
{
    if (note < 0 || note > 127) return;
    if (counts[note] > 0) out.addEvent (juce::MidiMessage::noteOff (1, note), pos);   // retrigger
    out.addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.05f, 1.0f, vel)), pos);
    ++counts[note];
}

void ChordGenerator::noteOff (juce::MidiBuffer& out, int note, int pos)
{
    if (note < 0 || note > 127 || counts[note] <= 0) return;
    if (--counts[note] == 0) out.addEvent (juce::MidiMessage::noteOff (1, note), pos);
}

void ChordGenerator::allOff (juce::MidiBuffer& out, int pos)
{
    for (int n = 0; n < 128; ++n)
        if (counts[n] > 0) { out.addEvent (juce::MidiMessage::noteOff (1, n), pos); counts[n] = 0; }
    playing.clear();
    for (auto& k : keyChords) k.clear();
    lastChord.clear();
}

void ChordGenerator::process (juce::MidiBuffer& midi, int numSamples, const HostInfo& host)
{
    const bool on = pOn->load() > 0.5f;
    const auto s = readSettings();
    juce::MidiBuffer out;

    // ---------------------------------------------------------------- preview from the UI
    juce::MidiBuffer previewOut;
    if (const int pr = previewRequest.exchange (-1); pr >= 0)
    {
        for (int n : previewNotes) previewOut.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        previewNotes = chordNotes (*pattern, pr, s, {});
        for (int n : previewNotes) previewOut.addEvent (juce::MidiMessage::noteOn (1, n, s.velocity), 0);
        previewLeft = (int) (0.9 * sr);
    }
    else if (previewLeft > 0)
    {
        previewLeft -= numSamples;
        if (previewLeft <= 0)
        {
            for (int n : previewNotes) previewOut.addEvent (juce::MidiMessage::noteOff (1, n), std::max (0, std::min (numSamples - 1, numSamples + previewLeft)));
            previewNotes.clear();
        }
    }

    if (! on)
    {
        if (wasOn)
        {
            allOff (out, 0);
            for (const auto meta : midi) out.addEvent (meta.getMessage(), meta.samplePosition);
            midi.swapWith (out);
            currentSlot = -1;
        }
        wasOn = false; active = false; held.clear(); lastKey = -1;
        if (! previewOut.isEmpty()) midi.addEvents (previewOut, 0, numSamples, 0);
        return;
    }
    if (! wasOn) { std::fill (std::begin (counts), std::end (counts), 0); playing.clear(); lastChord.clear(); }
    wasOn = true;

    // ---------------------------------------------------------------- chord per key
    if (s.mode == 1)
    {
        if (active) { allOff (out, 0); active = false; }
        currentSlot = -1;
        const auto& st = scaleFor (s.scale);
        const int n = (int) st.size();
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const int pos = meta.samplePosition;
            if (m.isNoteOn())
            {
                const int note = m.getNoteNumber();
                // which scale degree is this key (snapped down into the scale)?
                const int rel = ((note - s.key) % 12 + 12) % 12;
                int idx = 0;
                for (int k = 0; k < n; ++k) if (st[(size_t) k] <= rel) idx = k;
                const int target = note - (rel - st[(size_t) idx]);   // the played key, snapped into the scale

                ChordPattern single;
                single.slots[0].degree = idx;
                ChordSettings cs = s;
                if (cs.inversion == 0) cs.inversion = 1;   // root position: the chord sits on the key you play
                auto notes = chordNotes (single, 0, cs, {});
                const int rootPc = chordRootPc (single, 0, cs);
                const int rootAbs = 53 + ((rootPc - 5) % 12 + 12) % 12;
                const int shift = 12 * (int) std::round ((double) (target - rootAbs) / 12.0);
                auto& list = keyChords[note];
                for (int x : list) noteOff (out, x, pos);
                list.clear();
                for (int x : notes)
                {
                    const int y = x + shift;
                    if (y < 0 || y > 127) continue;
                    noteOn (out, y, m.getFloatVelocity(), pos);
                    list.push_back (y);
                }
            }
            else if (m.isNoteOff())
            {
                auto& list = keyChords[m.getNoteNumber()];
                for (int x : list) noteOff (out, x, pos);
                list.clear();
            }
            else out.addEvent (m, pos);
        }
        midi.swapWith (out);
        if (! previewOut.isEmpty()) midi.addEvents (previewOut, 0, numSamples, 0);
        return;
    }

    // ---------------------------------------------------------------- progression
    for (auto& k : keyChords) if (! k.empty()) { for (int x : k) noteOff (out, x, 0); k.clear(); }

    const double bpm = host.bpm > 1.0 ? host.bpm : 120.0;
    const double bps = bpm / 60.0 / sr;
    const bool hostRunning = host.playing && host.ppqValid;
    double startBeat = hostRunning ? host.ppq : internalBeat;
    if (hostRunning != wasRunning || (hostRunning && expectedBeat >= 0.0 && std::abs (startBeat - expectedBeat) > 0.05))
    {   // transport started, stopped or jumped
        for (auto& p : playing) noteOff (out, p.note, 0);
        playing.clear();
        if (hostRunning) origin = 0.0;
        lastSlot = -1;
    }
    wasRunning = hostRunning;

    const int count = juce::jlimit (1, kMaxChordSlots, s.count);
    const double base = choices::chordLengthBeats (s.lengthIndex);
    double starts[kMaxChordSlots + 1] {};
    for (int k = 0; k < count; ++k)
        starts[k + 1] = starts[k] + base * chordSlotLengths()[(size_t) juce::jlimit (0, 3, pattern->slots[(size_t) k].length.load())];
    const double T = std::max (0.25, starts[count]);

    // chords of this block (voice leading follows the progression order)
    std::vector<int> slotNotes[kMaxChordSlots];
    {
        std::vector<int> prev;
        // start from the last chord so the loop back to chord 1 is smooth too
        if (s.inversion == 0 && count > 1) prev = chordNotes (*pattern, count - 1, s, {});
        for (int k = 0; k < count; ++k) { slotNotes[k] = chordNotes (*pattern, k, s, prev); prev = slotNotes[k]; }
    }

    auto beatAt = [&] (int pos) { return startBeat + pos * bps; };
    auto posAt = [&] (double beat) { return juce::jlimit (0, numSamples - 1, (int) std::floor ((beat - startBeat) / bps)); };
    bool catchUp = false;

    auto lowestHeld = [&] { int lo = -1; for (int n : held) if (lo < 0 || n < lo) lo = n; return lo; };
    auto shouldBeActive = [&]
    {
        switch (s.trigger)
        {
            case 1:  return lastKey >= 0;
            case 2:  return hostRunning;
            default: return ! held.empty();
        }
    };
    auto transposeShift = [&]
    {
        int k = lowestHeld();
        if (k < 0) k = s.trigger == 1 ? lastKey : -1;
        return k < 0 ? 0 : 12 * ((int) std::floor (k / 12.0) - 5);   // C4..B4 = as shown, other octaves move the chords
    };

    auto runRange = [&] (int a, int b)
    {
        if (b <= a) return;
        const double ba = beatAt (a), bb = beatAt (b);
        for (auto it = playing.begin(); it != playing.end();)
        {
            if (it->offBeat < bb) { noteOff (out, it->note, std::max (a, posAt (it->offBeat))); it = playing.erase (it); }
            else ++it;
        }
        if (! active) return;

        const int shift = transposeShift();
        const long long c0 = (long long) std::floor ((ba - origin) / T) - 1, c1 = (long long) std::floor ((bb - origin) / T) + 1;
        for (long long c = c0; c <= c1; ++c)
            for (int k = 0; k < count; ++k)
            {
                const double L = starts[k + 1] - starts[k];
                const double slotStart = origin + (double) c * T + starts[k];
                if (slotStart > bb || slotStart + L < ba - 1.0) continue;
                const auto hits = chordRhythmHits (s.rhythm, L, s.swing);
                const auto& notes = slotNotes[k];
                for (size_t h = 0; h < hits.size(); ++h)
                    for (size_t j = 0; j < notes.size(); ++j)
                    {
                        const uint32_t hh = hash3 ((uint32_t) (c * 16 + k), (uint32_t) h, (uint32_t) j);
                        const double jitter = s.humanize * 0.025 * hashUni (hh);
                        double t = slotStart + hits[h].first + (double) j * s.strum * 0.083 + jitter;
                        const double off = slotStart + hits[h].first + std::max (0.05, hits[h].second * s.gate);
                        if (catchUp && t < ba && off > ba + 0.1) t = ba;   // key pressed in the middle of a chord: join in
                        if (t < ba || t >= bb || t < origin - 1e-9) continue;
                        if (off <= t) continue;
                        const int note = notes[j] + shift;
                        if (note < 0 || note > 127) continue;
                        for (auto it = playing.begin(); it != playing.end();)
                        {
                            if (it->note == note) { noteOff (out, note, posAt (t)); it = playing.erase (it); }
                            else ++it;
                        }
                        float vel = s.velocity + (h == 0 ? 0.06f : 0.0f) + s.humanize * 0.2f * (hashUni (hh * 7u + 3u) * 2.0f - 1.0f);
                        noteOn (out, note, vel, posAt (t));
                        playing.push_back ({ note, off });
                    }
            }
    };

    auto updateActive = [&] (int pos)
    {
        const bool want = shouldBeActive();
        if (want && ! active)
        {
            active = true;
            catchUp = true;
            if (! hostRunning)
            {
                origin = beatAt (pos);
                // the first chord starts exactly on the key press
            }
        }
        else if (! want && active)
        {
            active = false;
            for (auto& p : playing) noteOff (out, p.note, pos);
            playing.clear();
        }
    };

    updateActive (0);
    int cursor = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, numSamples - 1, meta.samplePosition);
        if (m.isNoteOnOrOff())
        {
            runRange (cursor, pos);
            catchUp = false;
            cursor = pos;
            const int note = m.getNoteNumber();
            held.erase (std::remove (held.begin(), held.end(), note), held.end());
            if (m.isNoteOn())
            {
                held.push_back (note);
                if (s.trigger == 1)
                {
                    if (note == lastKey && held.size() == 1 && active) lastKey = -1;   // same key again: stop (latch)
                    else lastKey = note;
                }
            }
            const bool wasActive = active;
            updateActive (pos);
            if (wasActive && active && m.isNoteOn() && s.trigger == 0 && held.size() == 1 && ! hostRunning)
                origin = beatAt (pos);   // new phrase: restart the progression
        }
        else out.addEvent (m, pos);
    }
    runRange (cursor, numSamples);
    catchUp = false;
    updateActive (numSamples - 1);

    // which chord is playing (for the UI)
    if (active)
    {
        const double b = beatAt (numSamples - 1) - origin;
        const double ph = b - std::floor (b / T) * T;
        int slot = 0;
        for (int k = 0; k < count; ++k) if (ph >= starts[k]) slot = k;
        currentSlot = slot;
        lastSlot = slot;
    }
    else currentSlot = -1;

    if (! hostRunning) internalBeat = startBeat + numSamples * bps;
    expectedBeat = hostRunning ? startBeat + numSamples * bps : -1.0;

    midi.swapWith (out);
    if (! previewOut.isEmpty()) midi.addEvents (previewOut, 0, numSamples, 0);
}

} // namespace pyra
