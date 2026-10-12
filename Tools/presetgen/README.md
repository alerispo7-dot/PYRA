# Factory bank generator (v0.9)

Builds the 2000 new factory presets from recipes, keeps the 540 earlier ones (moved into the new
categories), renders everything, levels it and replaces the presets that sound too much like a sibling.

```
cmake --build build-linux --target PyraRenderTest
build-linux/PyraRenderTest_artefacts/Release/PyraRenderTest --choices 2>/dev/null > Tools/params.json
python3 Tools/presetgen/qa.py Tools/presetgen/legacy_540.json <work dir> Resources/FactoryPresets.json 4
gzip -9 -n -c Resources/FactoryPresets.json > Resources/FactoryPresets.json.gz
```

* `core.py` – preset builder (`P`), helpers (reverb, delay, vibrato, macros...), the *characters*
  (clean, vintage, digital, wide, dark, bright, lofi, fx) and the name maker.
* `r_*.py` – recipes ("archetypes") per category / sub-category, registered with `@arch`.
* `build.py` – how many presets each sub-category gets (`LAYOUT`), the mapping of the earlier bank.
* `qa.py` – renders with `PyraRenderTest --normalize` (loudness + a 12 x 16 band spectral fingerprint),
  rebuilds near-duplicates with another seed, gives near-duplicate earlier presets a new colour,
  rescues presets that are too quiet, and orders each sub-category from realistic to synthetic,
  from simple starters to the heavily modulated ones.
* `legacy_540.json` – the bank as it was in v0.8 (input for the mapping of the earlier presets).
