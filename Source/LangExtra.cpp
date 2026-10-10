// Italian strings added in version 0.6 (chords, glitch, undo, random, portamento...).
#include "Lang.h"
#include <map>

namespace pyra
{
namespace lang
{
const std::map<juce::String, const char*>& italianExtra()
{
    static const std::map<juce::String, const char*> m {
        // tabs and general
        { "CHORDS", "ACCORDI" }, { "MATRIX", "MATRICE" }, { "PRESET", "PRESET" },
        { "Undo (go back one change)", "Annulla (torna indietro di una modifica)" },
        { "Redo (go forward again)", "Ripeti (torna avanti)" },
        { "New random values for this page. Right-click (or Shift-click) for a small variation instead. Undo goes back.",
          "Valori casuali nuovi per questa pagina. Clic destro (o Maiusc+clic) per una piccola variazione. Con Annulla torni indietro." },
        { "New random values", "Valori casuali nuovi" },
        { "Small variation (keeps the character)", "Piccola variazione (mantiene il carattere)" },
        { "Type a value...", "Scrivi un valore..." },
        { "Type a value and press Enter (e.g. 2.5k, 120 ms, -6). Esc cancels.",
          "Scrivi un valore e premi Invio (es. 2.5k, 120 ms, -6). Esc annulla." },

        // synth page
        { "VOICE & PORTAMENTO", "VOCE E PORTAMENTO" }, { "Only legato", "Solo legato" }, { "Portamento", "Portamento" },
        { "Constant Time: every slide takes the same time. Constant Rate: bigger jumps take longer.",
          "Tempo costante: ogni scivolata dura uguale. Velocit\xc3\xa0 costante: i salti pi\xc3\xb9 grandi durano di pi\xc3\xb9." },
        { "Portamento: notes slide into each other. 0 = off. Works in Poly, Mono and Legato.",
          "Portamento: le note scivolano una nell'altra. 0 = spento. Funziona in Poly, Mono e Legato." },
        { "Slide only when you play a note while another is still held",
          "Scivola solo quando suoni una nota mentre un'altra \xc3\xa8 ancora premuta" },
        { "Constant Time", "Tempo costante" }, { "Constant Rate", "Velocit\xc3\xa0 costante" },

        // presets / samples
        { "Sample Bank", "Banca campioni" }, { "SAMPLE", "CAMPIONE" }, { "From the sample bank:", "Dalla banca campioni:" },
        { "Plays a recorded instrument together with the oscillators", "Suona uno strumento registrato insieme agli oscillatori" },
        { "Recorded instrument (sample bank)", "Strumento registrato (banca campioni)" },
        { "Samples not found. Reinstall PYRA with the included installer.",
          "Campioni non trovati. Reinstalla PYRA con l'installer incluso." },
        { "Pick an instrument to layer it with the synth. Presets marked SAMPLE use it.",
          "Scegli uno strumento da sovrapporre al synth. I preset con la scritta CAMPIONE lo usano." },

        // chaos
        { "Random movement. Up to half it keeps the sound alive; towards the top it tears it apart (see MANGLE).",
          "Movimento casuale. Fino a met\xc3\xa0 rende il suono vivo; verso il massimo lo fa a pezzi (vedi MANGLE)." },
        { "Mangle", "Mangle" },
        { "How hard the top of the CHAOS knob breaks the sound: glitch repeats, reverse, tape stops, grains, octave jumps. At 0 chaos stays gentle.",
          "Quanto la parte alta di CHAOS stravolge il suono: ripetizioni glitch, reverse, tape stop, grani, salti d'ottava. A 0 il caos resta delicato." },

        // fx
        { "MAIN EFFECTS", "EFFETTI PRINCIPALI" }, { "GLITCH / GRANULAR / SHIFTER", "GLITCH / GRANULARE / SHIFTER" },
        { "GLITCH", "GLITCH" }, { "GRANULAR", "GRANULARE" }, { "SHIFTER", "SHIFTER" }, { "TIP", "CONSIGLIO" },
        { "Freeze", "Congela" }, { "STEP", "STEP" }, { "EFFECT TO PAINT", "EFFETTO DA DIPINGERE" },
        { "Orange = on the second FX page. Switched-off effects use no CPU.",
          "Arancione = nella seconda pagina FX. Gli effetti spenti non usano CPU." },
        { "Length of one step", "Durata di uno step" },
        { "Random effects on top of the pattern: 0 = only your steps, 100% = every step can glitch",
          "Effetti casuali sopra il pattern: 0 = solo i tuoi step, 100% = ogni step pu\xc3\xb2 glitchare" },
        { "Crossfade between steps: low = hard cuts, high = smoother", "Dissolvenza tra gli step: basso = tagli netti, alto = pi\xc3\xb9 morbido" },
        { "New random glitch pattern. Locked steps stay as they are.", "Nuovo pattern glitch casuale. Gli step bloccati restano come sono." },
        { "Click or drag on the steps to paint the chosen effect. Right-click = Off. The lock row keeps steps when you press RANDOM.",
          "Clicca o trascina sugli step per dipingere l'effetto scelto. Clic destro = spento. La riga dei lucchetti protegge gli step da RANDOM." },
        { "Turn CHAOS up to the top (with MANGLE) and these three effects come in by themselves: the sound gets cut, reversed, stretched and scattered.",
          "Alza CHAOS al massimo (con MANGLE) e questi tre effetti entrano da soli: il suono viene tagliato, rovesciato, stirato e sparpagliato." },
        { "Freeze: stops recording and keeps playing grains of what is in the buffer",
          "Congela: smette di registrare e continua a suonare grani di quello che c'\xc3\xa8 nel buffer" },
        { "Grain Size", "Grandezza grano" }, { "Density", "Densit\xc3\xa0" }, { "Spray", "Dispersione" }, { "Pitch", "Intonazione" },
        { "Rnd Pitch", "Into. casuale" }, { "Reverse", "Reverse" }, { "Stereo", "Stereo" },
        { "Repeat", "Ripeti" }, { "Tape Stop", "Tape stop" }, { "Slow", "Rallenta" }, { "Pitch Up", "Ottava su" }, { "Scratch", "Scratch" },
        { "Crush", "Crush" }, { "Freq Shift", "Freq shift" }, { "Ring Mod", "Ring mod" },

        // chords
        { "PROGRESSION", "PROGRESSIONE" }, { "CHORDS ON", "ACCORDI ATTIVI" }, { "Bass note", "Nota di basso" },
        { "Chords", "Accordi" }, { "Strum", "Strum" }, { "Humanize", "Umanizza" }, { "Complexity", "Complessit\xc3\xa0" },
        { "RANDOM ALL", "RANDOM TUTTO" }, { "Play", "Suona" }, { "Chord length", "Durata accordo" }, { "Rhythm", "Ritmo" },
        { "Chord type", "Tipo accordo" }, { "Voicing", "Disposizione" }, { "Inversion", "Rivolto" },
        { "click to add", "clicca per aggiungere" }, { "Length", "Durata" }, { "PLAY", "ASCOLTA" },
        { "Auto (scale)", "Auto (scala)" }, { "Root a semitone lower (b)", "Fondamentale un semitono sotto (b)" },
        { "Root a semitone higher (#)", "Fondamentale un semitono sopra (#)" }, { "of the chord length", "della durata accordo" },
        { "Save chords as MIDI", "Salva gli accordi come MIDI" },
        { "Drag onto a track in your DAW to get the chords as MIDI. Click to save a .mid file.",
          "Trascinalo su una traccia della DAW per avere gli accordi in MIDI. Clic per salvare un file .mid." },
        { "Progression: hold a key and the chords play in time. Chord per Key: every key you press plays the chord of the scale built on it.",
          "Progressione: tieni premuto un tasto e gli accordi suonano a tempo. Accordo per tasto: ogni tasto suona l'accordo della scala costruito su quella nota." },
        { "Hold Key: plays while you hold a key. Latch: keeps playing after you let go (same key again stops). DAW Play: plays whenever your DAW plays.",
          "Tieni tasto: suona finch\xc3\xa9 tieni premuto. Latch: continua anche quando lasci (lo stesso tasto di nuovo ferma). Play DAW: suona quando la DAW \xc3\xa8 in play." },
        { "How long each chord lasts (each card can be 1/4x, 1/2x, 1x or 2x of this)",
          "Quanto dura ogni accordo (ogni carta pu\xc3\xb2 essere 1/4x, 1/2x, 1x o 2x di questo)" },
        { "How the chords are played: held, pulsing, offbeats, syncopated...", "Come vengono suonati gli accordi: tenuti, pulsanti, in levare, sincopati..." },
        { "Triads, sevenths, ninths, sus, power chords, or a mix", "Triadi, settime, none, sus, power chord o un mix" },
        { "How the notes are spread: close together or wide apart", "Come sono distribuite le note: vicine o larghe" },
        { "Smooth: each chord picks the inversion closest to the one before (smooth voice leading)",
          "Morbido: ogni accordo sceglie il rivolto pi\xc3\xb9 vicino a quello prima (condotta delle voci fluida)" },
        { "For RANDOM: low = classic pop progressions, high = sevenths, ninths and borrowed chords",
          "Per RANDOM: basso = progressioni pop classiche, alto = settime, none e accordi presi in prestito" },
        { "How many chords in the progression", "Quanti accordi nella progressione" },
        { "Plays the notes of each chord one after another, like a guitar strum",
          "Suona le note di ogni accordo una dopo l'altra, come una pennata di chitarra" },
        { "New progression in the chosen key and scale. Locked chords stay.",
          "Nuova progressione nella tonalit\xc3\xa0 e scala scelte. Gli accordi bloccati restano." },
        { "Also picks a new key, scale, rhythm, voicing and length", "Sceglie anche tonalit\xc3\xa0, scala, ritmo, disposizione e durata nuove" },
        { "Chord per Key: every key you play becomes the chord of the scale that starts on it (white keys in C major / A minor).",
          "Accordo per tasto: ogni tasto che suoni diventa l'accordo della scala che parte da l\xc3\xac (tasti bianchi in Do maggiore / La minore)." },
        { "Hold any key to play the progression in time (C4-B4 = as shown, other octaves move it up or down). Turn ARP on to arpeggiate the chords.",
          "Tieni premuto un tasto per suonare la progressione a tempo (C4-B4 = come mostrato, le altre ottave la spostano). Accendi l'ARP per arpeggiare gli accordi." },
        { "CHORDS is off: switch it on (top left) to play the chords with your keys. PLAY on a card always works.",
          "ACCORDI \xc3\xa8 spento: accendilo (in alto a sinistra) per suonare gli accordi con i tasti. ASCOLTA su una carta funziona sempre." },
        { "Progression", "Progressione" }, { "Chord per Key", "Accordo per tasto" }, { "Hold Key", "Tieni tasto" }, { "DAW Play", "Play DAW" },
        { "Triads", "Triadi" }, { "Sevenths", "Settime" }, { "Ninths", "None" }, { "Sus", "Sus" }, { "Mixed", "Misto" },
        { "Close", "Strette" }, { "Open", "Aperte" }, { "Spread", "Larghe" }, { "Drop 2", "Drop 2" },
        { "Root", "Fondamentale" }, { "1st Inv", "1\xc2\xb0 rivolto" }, { "2nd Inv", "2\xc2\xb0 rivolto" },
        { "Sustain", "Tenuti" }, { "1/4 Pulse", "Pulsazione 1/4" }, { "1/8 Pulse", "Pulsazione 1/8" }, { "Offbeats", "In levare" },
        { "Syncopated", "Sincopato" }, { "Charleston", "Charleston" }, { "Stabs", "Stab" },
        { "Dim", "Dim" }, { "Aug", "Aum" }, { "Dorian", "Dorica" }, { "Phrygian", "Frigia" }, { "Lydian", "Lidia" },
        { "Mixolydian", "Misolidia" }, { "Locrian", "Locria" },
    };
    return m;
}
}
}
