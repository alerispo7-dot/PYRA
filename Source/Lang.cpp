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
            "This build: phases 1-4 (sound engine, modulation, interface, FX, ANALOG, ARP, 500 presets).\n"
            "Next build: the CHAOS / FOCUS / MOTION knobs.\n\n"
            "Quick tips\n"
            "- Double-click a knob to reset it, right-click for options.\n"
            "- Drag ENV / LFO / MACRO badges onto knobs to modulate them.\n"
            "- Drag the window corner to resize.";

        const char* aboutIt =
            "PYRA - sintetizzatore wavetable\n\n"
            "Questa versione: fasi 1-4 (motore sonoro, modulazione, interfaccia, FX, ANALOG, ARP, 500 preset).\n"
            "Prossima versione: le manopole CHAOS / FOCUS / MOTION.\n\n"
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

                // ---- phase 4 (presets)
                { "SAVE", "SALVA" }, { "Previous preset", "Preset precedente" }, { "Next preset", "Preset successivo" },
                { "Save the current sound as your own preset", "Salva il suono attuale come tuo preset" },
                { "Start from the default sound", "Riparti dal suono iniziale" }, { "(user)", "(tuo)" }, { "(edit)", "(modificato)" },
                { "e.g. dark, trap, my song", "es. scuro, trap, la mia canzone" }, { "User", "Utente" },
                { "SAVE AS NEW", "SALVA COME NUOVO" }, { "OVERWRITE", "SOVRASCRIVI" }, { "CANCEL", "ANNULLA" },
                { "SAVE PRESET", "SALVA PRESET" }, { "NAME", "NOME" }, { "CATEGORY", "CATEGORIA" },
                { "TAGS (separated by commas)", "TAG (separati da virgole)" },
                { "Saved in Documents/PYRA/Presets - it appears in the browser right away.",
                  "Salvato in Documenti/PYRA/Presets: compare subito nel browser." },
                { "Search presets, categories, tags...", "Cerca preset, categorie, tag..." },
                { "TAGS...", "TAG..." }, { "RENAME", "RINOMINA" }, { "DELETE", "ELIMINA" },
                { "SAVE CURRENT SOUND", "SALVA IL SUONO ATTUALE" }, { "OPEN FOLDER", "APRI CARTELLA" }, { "REFRESH", "AGGIORNA" },
                { "All Presets", "Tutti i preset" }, { "Favourites", "Preferiti" }, { "My Presets", "I miei preset" },
                { "CATEGORIES", "CATEGORIE" }, { "MY TAGS", "I MIEI TAG" }, { "TAGS", "TAG" },
                { "Factory preset", "Preset di fabbrica" }, { "Your preset", "Tuo preset" }, { "user", "tuo" },
                { "Click a preset to load it. Click the star to add it to your favourites.",
                  "Clicca un preset per caricarlo. Clicca la stellina per aggiungerlo ai preferiti." },
                { "My tags", "I miei tag" }, { "Your own tags for", "I tuoi tag per" },
                { "(separated by commas). They never change the sound or your projects.",
                  "(separati da virgole). Non cambiano mai il suono n\xc3\xa9 i tuoi progetti." },
                { "OK", "OK" }, { "Rename preset", "Rinomina preset" },
                { "Projects that use this preset keep sounding exactly the same.",
                  "I progetti che usano questo preset continuano a suonare esattamente uguali." },
                { "Delete preset", "Elimina preset" }, { "will be moved to the Trash.", "verr\xc3\xa0 spostato nel Cestino." },
                { "Bass", "Bassi" }, { "Lead", "Lead" }, { "Pad", "Pad" }, { "Keys", "Tastiere" }, { "Pluck", "Pluck" },
                { "Strings", "Archi" }, { "Brass & Winds", "Ottoni e fiati" }, { "Guitar", "Chitarre" }, { "Arp", "Arpeggi" },
                { "Sequence", "Sequenze" }, { "Chords", "Accordi" }, { "Drums", "Batteria" }, { "FX", "FX" },
                // ---- phase 3
                { "DISTORTION", "DISTORSIONE" }, { "RESONATOR", "RISONATORE" }, { "CHORUS", "CHORUS" }, { "FLANGER", "FLANGER" },
                { "PHASER", "PHASER" }, { "TAPE", "NASTRO" }, { "DELAY", "DELAY" }, { "REVERB", "RIVERBERO" },
                { "COMPRESSOR", "COMPRESSORE" }, { "SIGNAL CHAIN", "CATENA DEL SEGNALE" },
                { "Turn on only what you need: switched-off effects use no CPU.", "Accendi solo quello che ti serve: gli effetti spenti non usano CPU." },
                { "Tone", "Tono" }, { "Note", "Nota" }, { "Bright", "Brillantezza" }, { "Depth", "Profondit\xc3\xa0" },
                { "Feedback", "Feedback" }, { "Frequency", "Frequenza" }, { "Stages", "Stadi" }, { "Wow", "Wow" },
                { "Flutter", "Flutter" }, { "Saturation", "Saturazione" }, { "Time", "Tempo" }, { "Low Cut", "Taglio bassi" },
                { "High Cut", "Taglio alti" }, { "Size", "Dimensione" }, { "Pre-Delay", "Pre-delay" }, { "Damping", "Smorzamento" },
                { "Shimmer", "Shimmer" }, { "Low Freq", "Freq. bassi" }, { "Low Gain", "Guad. bassi" }, { "Mid Freq", "Freq. medi" },
                { "Mid Gain", "Guad. medi" }, { "Mid Q", "Q medi" }, { "High Freq", "Freq. alti" }, { "High Gain", "Guad. alti" },
                { "Threshold", "Soglia" }, { "Ratio", "Rapporto" }, { "Makeup", "Guadagno" },
                { "Soft Clip", "Soft clip" }, { "Hard Clip", "Hard clip" }, { "Tube", "Valvola" }, { "Fold", "Fold" },
                { "Diode", "Diodo" }, { "Overdrive", "Overdrive" }, { "Bitcrush", "Bitcrush" }, { "Downsample", "Downsample" },
                { "Single", "Singola" }, { "Octaves", "Ottave" }, { "Fifth", "Quinta" }, { "Major", "Maggiore" }, { "Minor", "Minore" },
                { "Power", "Power" }, { "Low Pass", "Passa basso" }, { "High Pass", "Passa alto" }, { "Band Pass", "Passa banda" },
                { "Digital", "Digitale" }, { "Tape Echo", "Eco a nastro" }, { "Ping-Pong", "Ping-pong" },
                { "Room", "Stanza" }, { "Hall", "Sala" },
                { "MOVEMENT", "MOVIMENTO" }, { "CIRCUIT", "CIRCUITO" }, { "NOISE & HUM", "RUMORE E RONZIO" },
                { "VINTAGE CONVERTER", "CONVERTITORE VINTAGE" }, { "Tolerance", "Tolleranza" }, { "Noise Floor", "Rumore di fondo" },
                { "Hum", "Ronzio" }, { "Bits", "Bit" }, { "Sample Rate", "Frequenza camp." },
                { "Turns a clean digital synth into a worn analog one. At 0 nothing changes: the sound stays clean and digital.",
                  "Trasforma un synth digitale pulito in uno analogico vissuto. A 0 non cambia nulla: il suono resta pulito e digitale." },
                { "AGE raises drift, voice spread, tolerance, circuit saturation and a little noise and hum together. Each knob on the right also works on its own.",
                  "AGE alza insieme drift, variazione voci, tolleranza, saturazione del circuito e un po' di rumore e ronzio. Ogni manopola a destra funziona anche da sola." },
                { "Drift: slow wandering pitch. Voice Spread: every note slightly different. Tolerance: oscillators and filters never perfectly matched.",
                  "Drift: intonazione che oscilla lentamente. Variazione voci: ogni nota un po' diversa. Tolleranza: oscillatori e filtri mai perfettamente uguali." },
                { "Warm, slightly uneven saturation of the voice circuit.", "Saturazione calda e leggermente irregolare del circuito." },
                { "Background hiss and mains hum, only while notes are sounding.", "Fruscio di fondo e ronzio di rete, solo mentre le note suonano." },
                { "Lower bit depth and sample rate, like 80s samplers. Not part of AGE.", "Meno bit e frequenza di campionamento pi\xc3\xb9 bassa, come i campionatori anni '80. Non fa parte di AGE." },
                { "STEPS", "STEP" }, { "LOCK", "BLOCCO" }, { "NOTE", "NOTA" }, { "VELOCITY", "VELOCITY" }, { "GATE", "GATE" },
                { "ON", "ATTIVO" }, { "TIE", "LEGATO" }, { "ARP ON", "ARP ATTIVO" }, { "Latch", "Latch" }, { "Mode", "Modalit\xc3\xa0" },
                { "Step Rate", "Divisione" }, { "Scale", "Scala" }, { "Key", "Tonica" }, { "Gate", "Gate" }, { "Swing", "Swing" },
                { "Steps", "Step" }, { "Step Vel", "Vel. step" }, { "RANDOM", "RANDOM" }, { "CLEAR", "AZZERA" },
                { "UNLOCK ALL", "SBLOCCA TUTTO" }, { "DRAG MIDI", "TRASCINA MIDI" },
                { "New random pattern. Locked steps stay as they are.", "Nuovo pattern casuale. Gli step bloccati restano come sono." },
                { "Drag onto a track in your DAW to get the pattern as MIDI. Click to save a .mid file.",
                  "Trascinalo su una traccia della DAW per avere il pattern in MIDI. Clic per salvare un file .mid." },
                { "Save arp pattern as MIDI", "Salva il pattern dell'arp come MIDI" },
                { "NOTE = scale steps added to the played note (Sequencer mode plays them as a melody). Drag to edit, double-click to reset. Locked steps survive RANDOM.",
                  "NOTA = gradi della scala aggiunti alla nota suonata (la modalit\xc3\xa0 Sequencer li suona come melodia). Trascina per modificare, doppio clic per azzerare. Gli step bloccati non cambiano con RANDOM." },
                { "Up", "Su" }, { "Down", "Gi\xc3\xb9" }, { "Up-Down", "Su-Gi\xc3\xb9" }, { "Down-Up", "Gi\xc3\xb9-Su" },
                { "As Played", "Come suonato" }, { "Chord", "Accordo" }, { "Sequencer", "Sequencer" },
                { "Chromatic", "Cromatica" }, { "Harmonic Minor", "Minore armonica" }, { "Melodic Minor", "Minore melodica" },
                { "Major Pentatonic", "Pentatonica maggiore" }, { "Minor Pentatonic", "Pentatonica minore" },
                { "Whole Tone", "Esatonale" }, { "Diminished", "Diminuita" }, { "Phrygian Dominant", "Frigia dominante" },
                { "Hungarian Minor", "Minore ungherese" }, { "Arabic", "Araba" },

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
