#include "Lang.h"
#include <map>

namespace pyra
{
namespace lang
{
    namespace
    {
        const char* aboutEn =
            "PYRA - wavetable synthesizer\n\n"
            "This build: phase 1 + 2 (sound engine, modulation, interface).\n"
            "Next builds: FX + ANALOG + ARP, then the preset library (about 500 presets), then the CHAOS / FOCUS / MOTION knobs.\n\n"
            "Quick tips\n"
            "- Double-click a knob to reset it, right-click for options.\n"
            "- Drag ENV / LFO / MACRO badges onto knobs to modulate them.\n"
            "- Drag the window corner to resize.";

        const char* aboutIt =
            "PYRA - sintetizzatore wavetable\n\n"
            "Questa versione: fase 1 + 2 (motore sonoro, modulazione, interfaccia).\n"
            "Prossime versioni: FX + ANALOG + ARP, poi la libreria di preset (circa 500), poi le manopole CHAOS / FOCUS / MOTION.\n\n"
            "Consigli rapidi\n"
            "- Doppio clic su una manopola per azzerarla, clic destro per le opzioni.\n"
            "- Trascina i badge ENV / LFO / MACRO sulle manopole per modularle.\n"
            "- Trascina l'angolo della finestra per ridimensionarla.";

        const std::map<juce::String, const char*>& italian()
        {
            static const std::map<juce::String, const char*> m {
                // knobs
                { "Level", "Livello" }, { "Pan", "Pan" }, { "WT Pos", "Pos. WT" }, { "Octave", "Ottava" },
                { "Semi", "Semitoni" }, { "Fine", "Fine" }, { "Unison", "Unisono" }, { "Detune", "Detune" },
                { "Blend", "Miscela" }, { "Width", "Ampiezza" }, { "Phase", "Fase" }, { "Random", "Casuale" },
                { "Warp", "Warp" }, { "Output", "Uscita" }, { "Cutoff", "Taglio" }, { "Resonance", "Risonanza" },
                { "Drive", "Drive" }, { "Mix", "Mix" }, { "Key Track", "Key Track" }, { "Env 2", "Env 2" },
                { "Delay", "Ritardo" }, { "Attack", "Attacco" }, { "Hold", "Tenuta" }, { "Decay", "Decadimento" },
                { "Sustain", "Sostegno" }, { "Release", "Rilascio" }, { "Att Curve", "Curva Att" },
                { "Dec Curve", "Curva Dec" }, { "Rel Curve", "Curva Ril" }, { "Rate", "Velocit\xc3\xa0" },
                { "Fade In", "Entrata" }, { "Smooth", "Morbidezza" }, { "Voices", "Voci" }, { "Glide", "Glide" },
                { "Bend Up", "Bend Su" }, { "Bend Down", "Bend Gi\xc3\xb9" }, { "Vel > Amp", "Vel > Volume" },
                { "Transpose", "Trasposizione" }, { "Tune", "Accordatura" }, { "Volume", "Volume" },
                { "Bend Range", "Range Bend" }, { "Drift", "Drift" }, { "Voice Spread", "Variazione voci" },
                { "Color", "Colore" },

                // choices
                { "Poly", "Poly" }, { "Mono", "Mono" }, { "Legato", "Legato" }, { "Off", "Spento" },
                { "Filter 1", "Filtro 1" }, { "Filter 2", "Filtro 2" }, { "Filter 1+2", "Filtro 1+2" }, { "Direct", "Diretto" },
                { "Sine", "Sinusoide" }, { "Triangle", "Triangolo" }, { "Saw", "Dente di sega" }, { "Square", "Quadra" },
                { "White", "Bianco" }, { "Pink", "Rosa" }, { "Brown", "Marrone" }, { "Crackle", "Crepitio" }, { "Digital", "Digitale" },
                { "Serial", "Serie" }, { "Parallel", "Parallelo" },
                { "Saw Up", "Rampa su" }, { "Saw Down", "Rampa gi\xc3\xb9" }, { "Pulse", "Impulso" }, { "Stairs", "Scalini" },
                { "Exp Up", "Esp. su" }, { "Exp Down", "Esp. gi\xc3\xb9" }, { "Smooth Random", "Casuale morbido" },
                { "Retrig", "Riavvio" }, { "Free", "Libero" }, { "Envelope", "Inviluppo" },
                { "None", "Nessuno" }, { "Velocity", "Velocity" }, { "Note", "Nota" }, { "Mod Wheel", "Rotella mod" },
                { "Pressure", "Pressione" }, { "Pitch Bend", "Pitch Bend" }, { "Slide", "Slide" },

                // UI text
                { "Drag a mod source here to modulate. Right-click for options.",
                  "Trascina qui una sorgente di modulazione. Clic destro per le opzioni." },
                { "Reset to default", "Ripristina valore iniziale" },
                { "Remove modulation", "Rimuovi modulazione" },
                { "Click to show, drag onto a knob to modulate it", "Clic per mostrare, trascina su una manopola per modularla" },
                { "Previous / next wavetable", "Wavetable precedente / successiva" },
                { "FILTER", "FILTRO" },
                { "Serial: Filter 1 feeds Filter 2. Parallel: both filters side by side.",
                  "Serie: il Filtro 1 entra nel Filtro 2. Parallelo: i due filtri lavorano affiancati." },
                { "Env 1 always shapes the volume. Drag it onto a knob to also modulate that knob.",
                  "Env 1 controlla sempre il volume. Trascinalo su una manopola per modulare anche quella." },
                { "Env 1 = volume", "Env 1 = volume" },
                { "Drag ENV onto a knob", "Trascina ENV su una manopola" },
                { "Unipolar", "Unipolare" }, { "Sync", "Sync" },
                { "Retrig: restarts on every note. Free: keeps running. Envelope: plays once.",
                  "Riavvio: riparte a ogni nota. Libero: continua a girare. Inviluppo: suona una volta sola." },
                { "Drag onto any knob: one macro can move many knobs at once",
                  "Trascinala su qualsiasi manopola: una macro pu\xc3\xb2 muovere tante manopole insieme" },
                { "drag onto a knob to modulate it", "trascina su una manopola per modularla" },
                { "More mod sources: drag them onto a knob", "Altre sorgenti di modulazione: trascinale su una manopola" },
                { "Clear this slot", "Svuota questo slot" },
                { "MODULATION MATRIX", "MATRICE DI MODULAZIONE" },
                { "Tip: on the SYNTH page you can drag ENV, LFO, MACRO or any source badge straight onto a knob.",
                  "Consiglio: nella pagina SYNTH puoi trascinare ENV, LFO, MACRO o qualsiasi badge direttamente su una manopola." },
                { "SOURCE", "SORGENTE" }, { "AMOUNT", "QUANTIT\xc3\x80" }, { "DESTINATION", "DESTINAZIONE" },
                { "VOICE", "VOCE" }, { "ANALOG CHARACTER", "CARATTERE ANALOGICO" }, { "LANGUAGE", "LINGUA" },
                { "ABOUT", "INFO" }, { "Glide only when legato", "Glide solo in legato" }, { "MPE on", "MPE attivo" },
                { "Poly = chords. Mono = one note, retriggers. Legato = one note, slides without retriggering.",
                  "Poly = accordi. Mono = una nota alla volta, riparte. Legato = una nota, scivola senza ripartire." },
                { "Drift: oscillators wander slightly out of tune, like real hardware. Voice Spread: every note is a little different (pitch, filter, envelope). Both at 0 = clean digital sound. More analog tools arrive with the ANALOG section in phase 3.",
                  "Drift: gli oscillatori si stonano leggermente come nell'hardware vero. Variazione voci: ogni nota \xc3\xa8 un po' diversa (intonazione, filtro, inviluppo). Entrambi a 0 = suono digitale pulito. Altri strumenti analogici arrivano con la sezione ANALOG nella fase 3." },
                { "Coming in the next build", "In arrivo nella prossima versione" },
                { "PRESETS", "PRESET" }, { "SETTINGS", "IMPOSTAZIONI" },
                { "Distortion (several models), compressor, EQ", "Distorsione (pi\xc3\xb9 modelli), compressore, EQ" },
                { "Chorus, flanger, phaser, tape wow & flutter", "Chorus, flanger, phaser, wow & flutter a nastro" },
                { "Tape echo and delay, 3 reverbs (room, hall, shimmer)", "Tape echo e delay, 3 riverberi (stanza, sala, shimmer)" },
                { "Resonators and FX filter, all in a fixed, tidy chain", "Risonatori e filtro FX, in una catena fissa e ordinata" },
                { "Random button: a new arpeggio at every click", "Pulsante Random: un arpeggio nuovo a ogni clic" },
                { "Step editor up to 32 steps: note, velocity, length, tie", "Editor fino a 32 step: nota, velocity, durata, legatura" },
                { "Lock the steps you like, randomize the others", "Blocca gli step che ti piacciono, randomizza gli altri" },
                { "Scales and root note, modes, swing, drag MIDI to your DAW", "Scale e tonica, modalit\xc3\xa0, swing, trascina il MIDI nella DAW" },
                { "AGE knob: from brand new to 1978 synth", "Manopola AGE: da nuovo di fabbrica a synth del '78" },
                { "Circuit and tape saturation, component tolerance", "Saturazione circuito e nastro, tolleranza dei componenti" },
                { "Noise floor and hum, vintage converter (80s sampler grit)", "Rumore di fondo e ronzio, convertitore vintage (grana anni '80)" },
                { "Optional: at 0 everything stays clean and digital", "Opzionale: a 0 tutto resta pulito e digitale" },
                { "About 500 factory presets in categories and sub-categories", "Circa 500 preset divisi in categorie e sottocategorie" },
                { "Star your favourites, custom tags", "Stellina sui preferiti, tag personalizzati" },
                { "One-click save of your own sounds, like Serum", "Salvataggio dei tuoi suoni con un clic, come su Serum" },
            };
            return m;
        }

        Language& currentLang()
        {
            static Language l = settings().getIntValue ("language", (int) English) == (int) Italian ? Italian : English;
            return l;
        }
    }

    juce::PropertiesFile& settings()
    {
        static juce::PropertiesFile file = []
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "PYRA";
            o.folderName = "PYRA";
            o.filenameSuffix = "settings";
            o.osxLibrarySubFolder = "Application Support";
            o.millisecondsBeforeSaving = 500;
            return juce::PropertiesFile (o);
        }();
        return file;
    }

    Language current() { return currentLang(); }

    void set (Language l)
    {
        currentLang() = l;
        settings().setValue ("language", (int) l);
        settings().saveIfNeeded();
    }

    juce::String tr (const juce::String& english)
    {
        if (english == "ABOUT_TEXT")
            return juce::String::fromUTF8 (current() == Italian ? aboutIt : aboutEn);
        if (current() == English) return english;
        const auto& m = italian();
        auto it = m.find (english);
        return it != m.end() ? juce::String::fromUTF8 (it->second) : english;
    }
}
}
