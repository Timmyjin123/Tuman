#!/bin/bash
# Tuman installer: copies the plugins into the user plug-in folders and clears quarantine.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/Tuman.vst3" "$AU_DIR/Tuman.component"
cp -R "$HERE/Tuman.vst3" "$VST3_DIR/"
cp -R "$HERE/Tuman.component" "$AU_DIR/"
xattr -cr "$VST3_DIR/Tuman.vst3" "$AU_DIR/Tuman.component" 2>/dev/null || true

# make macOS re-read the Audio Unit list
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Tuman установлен:"
echo "  VST3: $VST3_DIR/Tuman.vst3"
echo "  AU:   $AU_DIR/Tuman.component"
echo "Проверка AU:"
auval -v aufx Tumn Tmjn 2>&1 | tail -2 || true
echo ""
echo "Теперь в Ableton: Settings -> Plug-Ins -> Rescan (удерживая Option — полное пересканирование)."
