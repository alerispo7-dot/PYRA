# PYRA

Sintetizzatore wavetable gratuito per Mac e Windows (VST3, AU per Logic, standalone).

## Scaricare la versione pronta
Apri la scheda **Actions** del repository, scegli l'ultima build verde e scarica
`PYRA-macOS` o `PYRA-Windows` in fondo alla pagina. Dentro lo zip trovi le istruzioni
di installazione (INSTALL_MAC.txt / INSTALL_WINDOWS.txt).

## Cosa c'è in questa versione (fase 1 + 2)
- 3 oscillatori wavetable (28 tabelle incluse, band-limited), unison fino a 16 voci
  con detune / blend / width, 9 modalità di warp, FM tra oscillatori
- Sub oscillatore e generatore di rumore (5 tipi)
- 2 filtri (14 tipi: LP/HP/BP 12-24, ladder, notch, peak, all pass, comb, formant, phaser),
  in serie o in parallelo, con drive, mix, key track
- 4 inviluppi DAHDSR con curve, 4 LFO (11 forme, sync al tempo, retrig / free / one-shot)
- Matrice di modulazione a 16 slot + drag & drop delle sorgenti sulle manopole
- 4 macro, Poly / Mono / Legato, glide, pitch bend, mod wheel, aftertouch, MPE
- Drift analogico e variazione tra le voci (a 0 il suono resta digitale e pulito)
- Interfaccia ridimensionabile, inglese / italiano

## In arrivo
FX + ANALOG + ARP → libreria di ~500 preset con preferiti e tag → manopole CHAOS / FOCUS / MOTION.

## Compilare da sé
```
git clone --recursive <url del repository>
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target PYRA_All
```
Usa [JUCE](https://juce.com) 8 (incluso come submodule).
