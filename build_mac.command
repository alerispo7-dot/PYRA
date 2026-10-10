#!/bin/bash
# PYRA - compila e installa il plugin direttamente su questo Mac (Apple Silicon o Intel).
# Uso: clic destro su questo file > Apri.
set -e
cd "$(dirname "$0")"
echo "=== PYRA: compilazione su Mac ==="

# 1. Strumenti di sviluppo Apple (compilatore + git)
if ! xcode-select -p >/dev/null 2>&1; then
  echo ""
  echo "Mancano gli Strumenti da riga di comando di Apple."
  echo "Si apre una finestra: clicca 'Installa', aspetta la fine, poi riapri questo file."
  xcode-select --install || true
  read -p "Premi Invio per chiudere..."
  exit 0
fi

# 2. CMake
CMAKE="$(command -v cmake || true)"
[ -z "$CMAKE" ] && [ -x /Applications/CMake.app/Contents/bin/cmake ] && CMAKE=/Applications/CMake.app/Contents/bin/cmake
[ -z "$CMAKE" ] && [ -x /opt/homebrew/bin/cmake ] && CMAKE=/opt/homebrew/bin/cmake
if [ -z "$CMAKE" ]; then
  echo ""
  echo "Manca CMake. Scaricalo da https://cmake.org/download/ (file 'macOS 10.13 or later', .dmg),"
  echo "trascina CMake nella cartella Applicazioni, poi riapri questo file."
  open "https://cmake.org/download/"
  read -p "Premi Invio per chiudere..."
  exit 0
fi

# 3. JUCE (il framework audio), scaricato una sola volta
if [ ! -f JUCE/CMakeLists.txt ]; then
  echo "Scarico JUCE 8.0.4 (una sola volta, ~1 minuto)..."
  rm -rf JUCE
  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
fi

# 4. Compilazione (5-10 minuti la prima volta)
echo "Compilo PYRA... (la prima volta ci vogliono alcuni minuti)"
# Universal: works both in apps running natively on Apple Silicon and in apps running under Rosetta (Intel)
"$CMAKE" -B build-mac -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
"$CMAKE" --build build-mac --target PYRA_All -j"$(sysctl -n hw.ncpu)"

# 5. Installazione
ART=build-mac/PYRA_artefacts/Release
VST3="$HOME/Library/Audio/Plug-Ins/VST3"; AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/PYRA.vst3" "$AU/PYRA.component" /Applications/PYRA.app
cp -R "$ART/VST3/PYRA.vst3" "$VST3/"
cp -R "$ART/AU/PYRA.component" "$AU/"
cp -R "$ART/Standalone/PYRA.app" /Applications/
for b in "$VST3/PYRA.vst3" "$AU/PYRA.component" /Applications/PYRA.app; do
  codesign --force --deep --sign - "$b" >/dev/null 2>&1 || true
  xattr -dr com.apple.quarantine "$b" 2>/dev/null || true
done
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Architetture: $(lipo -archs "$VST3/PYRA.vst3/Contents/MacOS/PYRA" 2>/dev/null)"
echo "=== FATTO ==="
echo "VST3 -> $VST3/PYRA.vst3   (FL Studio, Ableton...)"
echo "AU   -> $AU/PYRA.component   (Logic)"
echo "App  -> Applicazioni/PYRA.app   (si apre anche senza DAW)"
echo ""
echo "Riapri la DAW e fai una nuova scansione dei plugin."
open /Applications/PYRA.app || true
read -p "Premi Invio per chiudere..."
