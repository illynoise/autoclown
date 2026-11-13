#!/usr/bin/env bash
# =============================================================================
#  Auto Clown — macOS release build: universal binary, tests, validation,
#  code signing (hardened runtime), installer package, notarization, stapling.
#
#  Requirements: Xcode 15+ command line tools, CMake >= 3.22, a "Developer ID
#  Application" and a "Developer ID Installer" certificate in the keychain, and a
#  notarytool keychain profile:
#     xcrun notarytool store-credentials "AutoClownNotary" \
#           --apple-id "<you@example.com>" --team-id "<TEAMID>" --password "<app-specific-pw>"
#
#  Usage:
#     DEV_ID_APP="Developer ID Application: Your Company (TEAMID)" \
#     DEV_ID_INSTALLER="Developer ID Installer: Your Company (TEAMID)" \
#     NOTARY_PROFILE="AutoClownNotary" \
#     ./scripts/build_sign_notarize.sh
#
#  Flags (env):  SKIP_SIGN=1  SKIP_NOTARIZE=1  SKIP_PLUGINVAL=1  PLUGINVAL=/path/to/pluginval
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"
VERSION="$(grep -E '^project\(AutoClown VERSION' "$ROOT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
PRODUCT="Auto Clown"
ART="$BUILD/AutoClown_artefacts/Release"
VST3="$ART/VST3/$PRODUCT.vst3"
AU="$ART/AU/$PRODUCT.component"
ENTITLEMENTS="$ROOT/scripts/entitlements.plist"
# PLACEHOLDERS — must match AC_MANUFACTURER_CODE / AC_PLUGIN_CODE in CMakeLists.txt
AU_TYPE="aumf"; AU_SUBTYPE="AcLn"; AU_MANUFACTURER="Ycmp"

log() { printf '\n\033[1;31m🎪 %s\033[0m\n' "$*"; }
[[ "$(uname)" == "Darwin" ]] || { echo "This script must run on macOS."; exit 1; }

# -----------------------------------------------------------------------------
log "1/8 Configure + build (universal arm64;x86_64, macOS 12+)"
cmake -S "$ROOT" -B "$BUILD" -G Xcode \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DAUTOCLOWN_BUILD_TESTS=ON
cmake --build "$BUILD" --config Release --parallel

for b in "$VST3/Contents/MacOS/$PRODUCT" "$AU/Contents/MacOS/$PRODUCT"; do
  lipo -info "$b"
  lipo -info "$b" | grep -q "x86_64 arm64\|arm64 x86_64" || { echo "Not universal: $b"; exit 1; }
done

# -----------------------------------------------------------------------------
log "2/8 DSP unit tests + CPU benchmark"
"$BUILD/tests/Release/AutoClownTests"
"$BUILD/tests/Release/AutoClownBench" | tee "$BUILD/cpu-benchmark.txt"

# -----------------------------------------------------------------------------
log "3/8 Sign bundles (hardened runtime, secure timestamp)"
if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
  : "${DEV_ID_APP:?Set DEV_ID_APP to your 'Developer ID Application: …' identity}"
  for bundle in "$VST3" "$AU"; do
    codesign --force --deep --timestamp --options runtime \
             --entitlements "$ENTITLEMENTS" --sign "$DEV_ID_APP" "$bundle"
    codesign --verify --deep --strict --verbose=2 "$bundle"
  done
fi

# -----------------------------------------------------------------------------
log "4/8 Install locally for validation"
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/Components/"$PRODUCT.component" ~/Library/Audio/Plug-Ins/VST3/"$PRODUCT.vst3"
cp -R "$AU" ~/Library/Audio/Plug-Ins/Components/
cp -R "$VST3" ~/Library/Audio/Plug-Ins/VST3/
killall -9 AudioComponentRegistrar 2>/dev/null || true   # force AU cache refresh

# -----------------------------------------------------------------------------
log "5/8 auval (Audio Unit validation)"
auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUFACTURER" | tee "$BUILD/auval.txt"
grep -q "AU VALIDATION SUCCEEDED" "$BUILD/auval.txt" || { echo "auval failed"; exit 1; }

# -----------------------------------------------------------------------------
log "6/8 pluginval (strictness 10) on VST3 and AU"
if [[ "${SKIP_PLUGINVAL:-0}" != "1" ]]; then
  PV="${PLUGINVAL:-$BUILD/pluginval.app/Contents/MacOS/pluginval}"
  if [[ ! -x "$PV" ]]; then
    curl -sSfL -o "$BUILD/pluginval.zip" https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip
    (cd "$BUILD" && unzip -o -q pluginval.zip)
  fi
  "$PV" --strictness-level 10 --validate-in-process --output-dir "$BUILD/pluginval" \
        --validate ~/Library/Audio/Plug-Ins/VST3/"$PRODUCT.vst3"
  "$PV" --strictness-level 10 --validate-in-process --output-dir "$BUILD/pluginval" \
        --validate ~/Library/Audio/Plug-Ins/Components/"$PRODUCT.component"
fi

# -----------------------------------------------------------------------------
log "7/8 Build signed installer package"
rm -rf "$DIST" && mkdir -p "$DIST/root/Library/Audio/Plug-Ins/Components" "$DIST/root/Library/Audio/Plug-Ins/VST3"
cp -R "$AU" "$DIST/root/Library/Audio/Plug-Ins/Components/"
cp -R "$VST3" "$DIST/root/Library/Audio/Plug-Ins/VST3/"
PKG="$DIST/AutoClown-$VERSION.pkg"
pkgbuild --root "$DIST/root" --identifier "com.yourcompany.autoclown.pkg" --version "$VERSION" \
         --install-location / "$DIST/AutoClown-component.pkg"
if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
  : "${DEV_ID_INSTALLER:?Set DEV_ID_INSTALLER to your 'Developer ID Installer: …' identity}"
  productbuild --package "$DIST/AutoClown-component.pkg" --sign "$DEV_ID_INSTALLER" --timestamp "$PKG"
  pkgutil --check-signature "$PKG"
else
  productbuild --package "$DIST/AutoClown-component.pkg" "$PKG"
fi

# -----------------------------------------------------------------------------
log "8/8 Notarize + staple"
if [[ "${SKIP_SIGN:-0}" != "1" && "${SKIP_NOTARIZE:-0}" != "1" ]]; then
  : "${NOTARY_PROFILE:?Set NOTARY_PROFILE to a notarytool keychain profile}"
  xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait --timeout 30m
  xcrun stapler staple "$PKG"
  xcrun stapler validate "$PKG"
  spctl --assess --type install --verbose=4 "$PKG"
fi

log "Done: $PKG"
