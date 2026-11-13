#!/usr/bin/env bash
# One-command local build for your own Mac: universal VST3 + AU, ad-hoc signed,
# installed into ~/Library/Audio/Plug-Ins, plus zipped copies in ./dist.
# Needs: Xcode (or Command Line Tools) and CMake  (brew install cmake).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
command -v cmake >/dev/null || { echo "Install CMake first:  brew install cmake"; exit 1; }

cmake -S . -B build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DAUTOCLOWN_BUILD_TESTS=OFF
cmake --build build-mac --config Release --target AutoClown_VST3 AutoClown_AU --parallel

ART="build-mac/AutoClown_artefacts/Release"
VST3="$ART/VST3/Auto Clown.vst3"; AU="$ART/AU/Auto Clown.component"

# Ad-hoc signature: required for Apple Silicon to load the binaries (not notarized).
codesign --force --deep --sign - "$VST3"
codesign --force --deep --sign - "$AU"

mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components dist
rm -rf ~/Library/Audio/Plug-Ins/VST3/"Auto Clown.vst3" ~/Library/Audio/Plug-Ins/Components/"Auto Clown.component"
cp -R "$VST3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "$AU" ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar 2>/dev/null || true

(cd "$ART/VST3" && ditto -c -k --keepParent "Auto Clown.vst3" "$ROOT/dist/AutoClown-VST3-mac.zip")
(cd "$ART/AU" && ditto -c -k --keepParent "Auto Clown.component" "$ROOT/dist/AutoClown-AU-mac.zip")

echo "Installed. Checking the Audio Unit:"
auval -v aumf AcLn Ycmp | tail -3
echo "Zips in $ROOT/dist. Rescan plug-ins in your DAW (Logic: Settings > Plug-in Manager)."
