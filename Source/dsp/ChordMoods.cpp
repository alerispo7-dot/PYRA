// CHORDS emotions: each mood is a set of progressions (roman numerals) and a way of playing them.
#include "Chords.h"
#include "../Params.h"

namespace pyra
{
const juce::StringArray& chordMoodNames()
{
    static juce::StringArray a { "Any", "Happy", "Euphoric", "Hopeful", "Sad", "Melancholic", "Nostalgic", "Lonely", "Romantic",
                                 "Sensual", "Dreamy", "Ethereal", "Peaceful", "Chill / Lo-fi", "Jazzy", "Bluesy / Funky", "Epic",
                                 "Heroic", "Angry / Aggressive", "Dark", "Evil", "Spooky", "Mysterious", "Tense" };
    return a;
}

namespace
{
    // scale indices (choices::scales)
    enum { Major = 1, Minor = 2, HarmMinor = 3, MelMinor = 4, Dorian = 5, Phrygian = 6, Lydian = 7, Mixolydian = 8,
           Locrian = 9, PhrygDom = 15, HungMinor = 16 };
    // rhythms: 0 Sustain, 1 1/4 Pulse, 2 1/8 Pulse, 3 Offbeats, 4 Syncopated, 5 Charleston, 6 Stabs
    // voicings: 0 Close, 1 Open, 2 Spread, 3 Drop 2 / sizes: 0 Triads, 1 Sevenths, 2 Ninths, 3 Sus, 4 Power, 5 Mixed
    // lengths: 0 4 Bars, 1 2 Bars, 2 1 Bar, 3 1/2, 4 1/4

    std::vector<ChordMoodStyle> makeStyles()
    {
        std::vector<ChordMoodStyle> v (chordMoodNames().size());
        auto set = [&] (int i, ChordMoodStyle s) { v[(size_t) i] = std::move (s); };
        set (1, { { Major, Major, Major, Lydian, Mixolydian }, { 2, 3, 4, 1, 2 }, { 0, 1 }, { 0, 0, 5 }, { 2 }, { 4, 4, 8 }, 0.05f, 0.3f, 0.15f, 0.15f, 0.8f, 0,
                  { "I V vi IV", "I IV V IV", "I vi IV V", "IV I V vi", "I iii IV V", "I ii IV V", "I IV I V", "I V IV V", "I IV vi V" } });
        set (2, { { Minor, Major }, { 2, 3, 2, 4 }, { 1, 2 }, { 5, 3, 0 }, { 2 }, { 4, 8 }, 0.2f, 0.45f, 0.05f, 0.0f, 0.9f, 0,
                  { "vi IV I V", "i VI III VII", "i VII VI VII", "VI VII i i", "i III VII VI", "IV V vi I", "vi IV V V" } });
        set (3, { { Major, Major, Lydian }, { 1, 2, 3, 0 }, { 1, 2 }, { 0, 5, 3 }, { 2 }, { 4, 8 }, 0.15f, 0.4f, 0.2f, 0.05f, 0.8f, 0,
                  { "IV V vi I", "I V vi IV", "ii IV I V", "IV I V vi", "vi IV I V", "Isus4 I IV V", "IV V iii vi" } });
        set (4, { { Minor, Minor, Dorian, HarmMinor }, { 0, 0, 1 }, { 1, 2 }, { 0, 1, 5 }, { 1, 2 }, { 4 }, 0.2f, 0.5f, 0.35f, 0.0f, 0.7f, 0,
                  { "i VI III VII", "i iv VI v", "i VII VI VII", "i iv VII III", "VI iv i v", "i III VII iv", "i v VI iv", "i VI iv V" } });
        set (5, { { Minor, Dorian }, { 0, 1, 4 }, { 1, 2, 3 }, { 1, 2, 5 }, { 1, 2 }, { 4, 8 }, 0.35f, 0.65f, 0.3f, 0.1f, 0.7f, 0,
                  { "i7 VImaj7 IIImaj7 VII", "i7 iv7 VII7 IIImaj7", "VImaj7 v7 i7 i7", "i9 VImaj7 iv7 v7", "iv7 i7 VImaj7 V7" } });
        set (6, { { Major }, { 0, 1, 5 }, { 1, 2 }, { 1, 5 }, { 1, 2 }, { 4 }, 0.3f, 0.55f, 0.4f, 0.15f, 0.8f, 0,
                  { "I IV iv I", "Imaj7 iii7 IVmaj7 iv", "I iii IV iv", "IVmaj7 iii7 ii7 Imaj7", "I vi IV iv", "I V/vi vi IV" } });
        set (7, { { Minor, Dorian, Minor }, { 0, 0, 1 }, { 2, 1 }, { 2, 3, 5 }, { 0, 1 }, { 2, 4 }, 0.2f, 0.45f, 0.3f, 0.0f, 0.5f, 0,
                  { "i i VI VI", "i VII i VII", "i9 VImaj7 i9 VImaj7", "isus2 i VImaj7 VImaj7", "i iv i iv" } });
        set (8, { { Major, Major, Major }, { 0, 5, 1 }, { 2, 1 }, { 1, 5 }, { 2, 1 }, { 4, 8 }, 0.25f, 0.55f, 0.6f, 0.1f, 0.85f, 0,
                  { "I vi ii V", "Imaj7 vi7 ii7 V7", "I iii IV iv", "I V vi iii IV I IV V", "IV V iii vi", "I vi IV V" } });
        set (9, { { Dorian, Major, Minor }, { 4, 5, 0, 3 }, { 3, 1 }, { 2 }, { 2 }, { 4 }, 0.4f, 0.7f, 0.25f, 0.6f, 0.8f, 0,
                  { "ii9 V7 Imaj7 vi7", "Imaj7 vi9 ii9 V7", "IVmaj7 iii7 ii7 Imaj7", "i9 IV7 i9 IV7", "i7 iv9 VImaj7 V7" } });
        set (10, { { Lydian, Major, Major }, { 0, 0, 3 }, { 2 }, { 1, 2, 3 }, { 1, 2 }, { 4 }, 0.35f, 0.65f, 0.4f, 0.0f, 0.5f, 1,
                   { "Imaj7 II Imaj7 II", "Imaj9 iii7 IVmaj7 vi9", "IVmaj7 Imaj7 IVmaj7 V", "Isus2 vi7 IVmaj9 Isus2", "Imaj7 IVmaj7 Imaj7 IVmaj7" } });
        set (11, { { Lydian, Major }, { 0 }, { 2 }, { 2, 3 }, { 0, 1 }, { 2, 4 }, 0.35f, 0.6f, 0.5f, 0.0f, 0.3f, 1,
                   { "Imaj9 II Imaj9 II", "Isus2 IVmaj9 vi9 Isus2", "IVmaj9 V vi9 Imaj9", "Imaj7 IImaj7 Imaj7 IImaj7" } });
        set (12, { { Major, Major, Lydian }, { 0, 0, 1 }, { 1, 2 }, { 0, 3, 5 }, { 1, 0 }, { 2, 4 }, 0.15f, 0.4f, 0.45f, 0.0f, 0.6f, 0,
                   { "I IV I IV", "Iadd9 IVadd9 vi7 IVmaj7", "Imaj7 IVmaj7", "I V vi IV", "Isus2 I IVadd9 IV" } });
        set (13, { { Major, Dorian, Minor }, { 4, 4, 5, 3 }, { 3, 1 }, { 2, 1 }, { 2 }, { 4 }, 0.45f, 0.75f, 0.35f, 0.85f, 0.8f, 0,
                   { "IVmaj7 iii7 vi9 Imaj7", "ii7 V7 Imaj7 IVmaj7", "Imaj7 iii7 vi7 V7", "ii9 V9 Imaj9 vi9", "i9 iv9 VImaj7 V7" } });
        set (14, { { Major, Minor }, { 5, 4, 0 }, { 3, 1 }, { 1, 2 }, { 2 }, { 4, 8 }, 0.55f, 0.85f, 0.2f, 0.9f, 0.9f, 0,
                   { "ii7 V7 Imaj7 Imaj7", "Imaj7 vi7 ii7 V7", "iii7 vi7 ii7 V7", "iiø7 V7 i7 VImaj7", "Imaj7 IV7 iii7 VI7 ii7 V7 Imaj7 Imaj7" } });
        set (15, { { Mixolydian, Dorian }, { 4, 5, 3 }, { 0, 1 }, { 1 }, { 2 }, { 4, 8 }, 0.3f, 0.6f, 0.15f, 0.8f, 0.9f, 0,
                   { "I7 IV7 I7 V7", "I7 I7 IV7 IV7 I7 I7 V7 IV7", "i7 IV7 i7 IV7", "I7 bVII I7 IV7" } });
        set (16, { { Minor, Minor, Dorian }, { 0, 6, 2 }, { 1, 2 }, { 0, 4, 5 }, { 2, 1 }, { 4 }, 0.15f, 0.4f, 0.15f, 0.0f, 0.9f, 0,
                   { "i VI III VII", "i VI VII i", "VI VII i i", "i III VII IV", "i bVI bVII i", "i iv VI V" } });
        set (17, { { Major, Mixolydian }, { 6, 1, 0 }, { 1, 2 }, { 0, 4 }, { 2 }, { 4 }, 0.15f, 0.4f, 0.1f, 0.0f, 0.9f, 0,
                   { "I bVII IV I", "I bVI bVII I", "I V IV I", "IV V I I", "I IV V I", "bVI bVII I I" } });
        set (18, { { Phrygian, Minor, HarmMinor, PhrygDom }, { 6, 4, 2, 6 }, { 0 }, { 4, 4, 0 }, { 2, 3 }, { 4 }, 0.05f, 0.25f, 0.0f, 0.1f, 0.9f, -1,
                   { "i5 bII5 i5 bVII5", "i5 bVI5 bVII5 i5", "i5 i5 bVI5 bV5", "i bII i bII", "i5 bIII5 bII5 i5", "i iv bVI V" } });
        set (19, { { Minor, Phrygian, HarmMinor }, { 0, 0, 6 }, { 0, 1 }, { 0, 0, 1 }, { 1, 2 }, { 4 }, 0.2f, 0.5f, 0.1f, 0.0f, 0.8f, -1,
                   { "i bVI iv V", "i bII bVI V", "i iv bVI V", "i bVI bIII bVII", "i v bVI iv", "i bVI i bII" } });
        set (20, { { HarmMinor, HungMinor, PhrygDom, Locrian }, { 0, 6 }, { 0, 2 }, { 0, 1 }, { 1, 2 }, { 4 }, 0.3f, 0.6f, 0.1f, 0.0f, 0.8f, -1,
                   { "i bII i bII", "i #iv° i bII", "i V+ i bVI", "i iv° i V7", "i bVI+ iv V7" } });
        set (21, { { HarmMinor, Minor, HungMinor }, { 0, 6 }, { 2, 0 }, { 0 }, { 1, 2 }, { 4 }, 0.3f, 0.6f, 0.25f, 0.0f, 0.5f, 0,
                   { "i ii° i bVI", "i #iv° i bii", "i bvi i bvi", "i III+ iv bVI", "i bII+ i #vii°" } });
        set (22, { { Dorian, Phrygian, Lydian, Minor }, { 0, 3, 4 }, { 2, 1 }, { 3, 2 }, { 1, 2 }, { 4 }, 0.3f, 0.6f, 0.3f, 0.0f, 0.6f, 0,
                   { "i IV i IV", "i bII i bII", "isus2 bVIImaj7 isus2 bVIImaj7", "i II i II", "i7 IV7 bVImaj7 V" } });
        set (23, { { Minor, HarmMinor, Phrygian }, { 2, 1, 6 }, { 0, 3 }, { 0, 3 }, { 2, 3 }, { 4, 8 }, 0.2f, 0.5f, 0.0f, 0.0f, 0.8f, 0,
                   { "i bII i bII", "i V7 i V7", "isus4 i isus4 i", "#vii° i #vii° i", "i bVI V7 V7", "iv V7 i bII" } });
        v[0] = { { Minor, Major, Minor, Dorian }, { 0, 1, 2, 3, 4, 5, 6 }, { 0, 1, 2, 3 }, { 0, 1, 5 }, { 2 }, { 4 }, 0.1f, 0.8f, 0.3f, 0.3f, 0.7f, 0, {} };
        return v;
    }

    struct Chord { int semis = 0; int quality = QMaj; };

    /** One roman numeral: [b|#] I..VII (upper = major, lower = minor) [° + ø] [7 maj7 9 maj9 add9 sus2 sus4 5].
        minorKey: numerals count from the natural minor scale (VI = Ab in C minor, bVI means the same). */
    bool parseRoman (juce::String t, Chord& c, bool minorKey)
    {
        t = t.trim();
        if (const int slash = t.indexOfChar ('/'); slash > 0) t = t.substring (0, slash);   // secondary dominants: play the chord itself
        int acc = 0;
        while (t.startsWithChar ('b')) { acc -= 1; t = t.substring (1); }   // no numeral starts with b
        while (t.startsWithChar ('#')) { acc += 1; t = t.substring (1); }
        static const char* numerals[] = { "VII", "VI", "IV", "V", "III", "II", "I" };
        static const int degreeOf[] = { 6, 5, 3, 4, 2, 1, 0 };
        int degree = -1; bool upper = true; int used = 0;
        for (int k = 0; k < 7; ++k)
            if (t.toUpperCase().startsWith (numerals[k]))
            {
                degree = degreeOf[k]; used = (int) strlen (numerals[k]);
                upper = juce::CharacterFunctions::isUpperCase (t[0]);
                break;
            }
        if (degree < 0) return false;
        static const int majorSemis[] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int minorSemis[] = { 0, 2, 3, 5, 7, 8, 10 };
        if (minorKey && acc < 0 && (degree == 2 || degree == 5 || degree == 6)) acc = 0;   // bIII / bVI / bVII are already flat in minor
        c.semis = (minorKey ? minorSemis : majorSemis)[degree] + acc;
        auto rest = t.substring (used);
        int q = upper ? QMaj : QMin;
        const auto degreeSign = juce::String::fromUTF8 ("\xc2\xb0"), slashO = juce::String::fromUTF8 ("\xc3\xb8");
        if (rest.startsWith (degreeSign) || rest.startsWithChar ('o')) { q = QDim; rest = rest.substring (1); }
        else if (rest.startsWithChar ('+')) { q = QAug; rest = rest.substring (1); }
        if (rest.contains (slashO) || rest.containsIgnoreCase ("h7")) q = QHalfDim;
        else if (rest.containsIgnoreCase ("maj9")) q = QMaj9;
        else if (rest.containsIgnoreCase ("maj7")) q = QMaj7;
        else if (rest.containsIgnoreCase ("add9")) q = QAdd9;
        else if (rest.containsIgnoreCase ("sus2")) q = QSus2;
        else if (rest.containsIgnoreCase ("sus4")) q = QSus4;
        else if (rest.contains ("9")) q = upper ? QAdd9 : QMin9;
        else if (rest.contains ("7")) q = q == QDim ? QHalfDim : (upper ? QDom7 : QMin7);
        else if (rest.contains ("5")) q = QPower;
        c.quality = q;
        return true;
    }

    int triadQualityFromScale (const std::vector<int>& st, int degree)
    {
        const int n = (int) st.size();
        auto tone = [&] (int d) { return st[(size_t) (d % n)] + 12 * (d / n); };
        const int r = tone (degree), third = tone (degree + 2) - r, fifth = tone (degree + 4) - r;
        if (third == 4 && fifth == 7) return QMaj;
        if (third == 3 && fifth == 7) return QMin;
        if (third == 3 && fifth == 6) return QDim;
        if (third == 4 && fifth == 8) return QAug;
        return -1;
    }
}

const ChordMoodStyle& chordMoodStyle (int mood)
{
    static const std::vector<ChordMoodStyle> styles = makeStyles();
    return styles[(size_t) juce::jlimit (0, (int) styles.size() - 1, mood)];
}

void ChordPattern::randomizeMood (int mood, int count, int scale, float complexity, uint32_t seed)
{
    const auto& style = chordMoodStyle (mood);
    if (mood <= 0 || style.progressions.empty()) { randomize (count, scale, complexity, 0, seed); return; }

    Rng r; r.seed (seed * 2246822519u + 0x5eedu);
    r.next();
    count = juce::jlimit (1, kMaxChordSlots, count);
    auto st = choices::scaleSteps (scale);
    if (st.size() != 7) st = { 0, 2, 4, 5, 7, 9, 11 };

    // a progression is in a minor key when its tonic is written "i"; prefer the ones that match the scale (minor / major third)
    auto isMinorText = [] (const juce::String& text)
    {
        for (auto& tok : juce::StringArray::fromTokens (text, " ", ""))
        {
            auto t = tok.trimCharactersAtStart ("b#");
            if (t.startsWith ("i") && ! t.startsWith ("ii") && ! t.startsWith ("iv")) return true;
            if (t.startsWith ("I") && ! t.startsWith ("II") && ! t.startsWith ("IV")) return false;
        }
        return false;
    };
    const bool scaleMinor = st[2] == 3;
    juce::StringArray fitting;
    for (auto* p : style.progressions)
        if (isMinorText (juce::String::fromUTF8 (p)) == scaleMinor) fitting.add (juce::String::fromUTF8 (p));
    if (fitting.isEmpty())
        for (auto* p : style.progressions) fitting.add (juce::String::fromUTF8 (p));

    auto pickProgression = [&]
    {
        std::vector<Chord> out;
        const auto text = fitting[(int) (r.next() % (uint32_t) fitting.size())];
        const bool minorKey = isMinorText (text);
        for (auto& tok : juce::StringArray::fromTokens (text, " ", ""))
        {
            Chord c;
            if (parseRoman (tok, c, minorKey)) out.push_back (c);
        }
        return out;
    };
    auto prog = pickProgression();
    if (prog.empty()) { randomize (count, scale, complexity, 0, seed); return; }
    // more slots than the progression: repeat it, sometimes answering with a second progression of the same mood
    if (count > (int) prog.size())
    {
        auto second = r.uni() < 0.45f ? pickProgression() : prog;
        if (second.empty()) second = prog;
        std::vector<Chord> full;
        for (int k = 0; (int) full.size() < count; ++k)
        {
            const auto& src = (k / (int) prog.size()) % 2 == 0 ? prog : second;
            full.push_back (src[(size_t) (k % (int) src.size())]);
        }
        prog = full;
    }

    for (int k = 0; k < count; ++k)
    {
        auto& s = slots[(size_t) k];
        if (s.lock.load()) continue;
        s.empty = false;
        const auto& c = prog[(size_t) k];
        const int pc = ((c.semis % 12) + 12) % 12;
        int degree = 0, shift = 0;
        bool exact = false;
        for (int d = 0; d < 7; ++d)
            if (st[(size_t) d] == pc) { degree = d; exact = true; break; }
        if (! exact)
        {
            for (int d = 6; d >= 0; --d)
                if (st[(size_t) d] < pc) { degree = d; shift = pc - st[(size_t) d]; break; }
        }
        int q = c.quality;
        // a plain triad that the scale already gives: leave it on Auto, so the SIZE setting can colour it
        if (exact && (q == QMaj || q == QMin || q == QDim || q == QAug) && triadQualityFromScale (st, degree) == q) q = QAuto;
        // a little extra colour with complexity
        if (q == QAuto && r.uni() < complexity * 0.35f)
        {
            const int tri = triadQualityFromScale (st, degree);
            if (tri == QMin) q = r.uni() < 0.6f ? QMin7 : QMin9;
            else if (tri == QMaj) q = degree == 4 ? QDom7 : (r.uni() < 0.5f ? QMaj7 : QAdd9);
        }
        int len = 2;
        if (r.uni() < complexity * 0.2f) len = r.uni() < 0.5f ? 1 : 3;
        s.degree = degree; s.shift = shift; s.quality = q; s.length = len;
    }
    ++changeCounter;
}

} // namespace pyra
