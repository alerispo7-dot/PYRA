# PYRA

Sintetizzatore wavetable gratuito per Mac e Windows (VST3, AU per Logic, standalone).

## Scaricare la versione pronta
Apri la scheda **Actions** del repository, scegli l'ultima build verde e scarica
`PYRA-macOS` o `PYRA-Windows` in fondo alla pagina. Dentro lo zip trovi le istruzioni
di installazione (INSTALL_MAC.txt / INSTALL_WINDOWS.txt).

## Cosa c'è in questa versione (fasi 1-4)
- 3 oscillatori wavetable (28 tabelle incluse, band-limited), unison fino a 16 voci
  con detune / blend / width, 9 modalità di warp, FM tra oscillatori
- Sub oscillatore e generatore di rumore (5 tipi)
- 2 filtri (14 tipi: LP/HP/BP 12-24, ladder, notch, peak, all pass, comb, formant, phaser),
  in serie o in parallelo, con drive, mix, key track
- 4 inviluppi DAHDSR con curve, 4 LFO (11 forme, sync al tempo, retrig / free / one-shot)
- Matrice di modulazione a 16 slot + drag & drop delle sorgenti sulle manopole
- 4 macro, Poly / Mono / Legato, glide, pitch bend, mod wheel, aftertouch, MPE
- Drift analogico e variazione tra le voci (a 0 il suono resta digitale e pulito)
- **FX** in catena fissa: distorsione (8 tipi, oversampling), risonatori ad accordo, filtro, chorus,
  flanger, phaser, nastro (wow & flutter), delay / tape echo / ping-pong, riverbero (stanza, sala, shimmer),
  EQ a 3 bande, compressore
- **ANALOG**: manopola AGE, drift, variazione voci, tolleranza, saturazione del circuito, rumore di fondo,
  ronzio, convertitore vintage (tutto a 0 = suono digitale pulito)
- **ARP**: 8 modalità (incluso Sequencer), 20 scale, editor a 32 step (nota, velocity, gate, on, legato,
  blocco), RANDOM che rispetta gli step bloccati, swing, latch, sync con la DAW, trascinamento MIDI nella DAW
- **500 preset** in 13 categorie e 70 sottocategorie (bassi, lead, pad, tastiere, pluck, archi, ottoni e fiati,
  chitarre, arpeggi, sequenze, accordi, batteria, FX), con volume già bilanciato
- **Preferiti** (stellina), **tag personali**, ricerca, salvataggio dei tuoi suoni con un clic
  (Documenti/PYRA/Presets). Rinominare un preset non cambia mai i progetti già salvati.
- Interfaccia ridimensionabile, inglese / italiano

## In arrivo
Manopole CHAOS / FOCUS / MOTION.

## Compilare da sé
```
git clone --recursive <url del repository>
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target PYRA_All
```
Usa [JUCE](https://juce.com) 8 (incluso come submodule).

## Rigenerare i preset di fabbrica
```
python3 Tools/make_presets.py Resources/FactoryPresets.raw.json
PyraRenderTest --normalize Resources/FactoryPresets.raw.json Resources/FactoryPresets.json
gzip -9 -c Resources/FactoryPresets.json > Resources/FactoryPresets.json.gz
```
