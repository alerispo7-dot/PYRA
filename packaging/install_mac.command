#!/bin/bash
# PYRA - installazione su macOS / installation on macOS
cd "$(dirname "$0")"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/PYRA.vst3" "$AU/PYRA.component"
cp -R PYRA.vst3 "$VST3/"
cp -R PYRA.component "$AU/"
xattr -dr com.apple.quarantine "$VST3/PYRA.vst3" "$AU/PYRA.component" 2>/dev/null
if [ -d PYRA.app ]; then
  rm -rf /Applications/PYRA.app
  cp -R PYRA.app /Applications/ && xattr -dr com.apple.quarantine /Applications/PYRA.app 2>/dev/null
fi
killall -9 AudioComponentRegistrar 2>/dev/null   # Logic: refresh the Audio Unit list
echo ""
echo "PYRA installato / installed:"
echo "  VST3 -> $VST3/PYRA.vst3"
echo "  AU   -> $AU/PYRA.component"
echo "  App  -> /Applications/PYRA.app"
echo ""
echo "Riapri la DAW e fai una nuova scansione dei plugin. / Restart your DAW and rescan plugins."
