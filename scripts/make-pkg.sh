#!/bin/bash
# Build a macOS .pkg installer from the built bundles.
# Usage: ./scripts/make-pkg.sh [path/to/VocalChain_artefacts/Release]  (default: build/VocalChain_artefacts/Release)
# If DEVELOPER_ID_INSTALLER is set (e.g. "Developer ID Installer: Your Name (TEAMID)") the pkg is signed.
set -e
cd "$(dirname "$0")/.."
ART="${1:-build/VocalChain_artefacts/Release}"
VERSION=$(grep -m1 'project(VocalChain VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')
STAGE=$(mktemp -d)
mkdir -p "$STAGE/VST3/Library/Audio/Plug-Ins/VST3" "$STAGE/AU/Library/Audio/Plug-Ins/Components"
cp -R "$ART/VST3/VOXPAL.vst3" "$STAGE/VST3/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/VOXPAL.component" "$STAGE/AU/Library/Audio/Plug-Ins/Components/"
# ad-hoc sign the bundles unless a Developer ID application identity is provided
APPSIGN="${DEVELOPER_ID_APPLICATION:--}"
codesign --force --deep --options runtime -s "$APPSIGN" "$STAGE/VST3/Library/Audio/Plug-Ins/VST3/VOXPAL.vst3" || codesign --force --deep -s - "$STAGE/VST3/Library/Audio/Plug-Ins/VST3/VOXPAL.vst3"
codesign --force --deep --options runtime -s "$APPSIGN" "$STAGE/AU/Library/Audio/Plug-Ins/Components/VOXPAL.component" || codesign --force --deep -s - "$STAGE/AU/Library/Audio/Plug-Ins/Components/VOXPAL.component"
PKGS=$(mktemp -d)
pkgbuild --root "$STAGE/VST3" --identifier com.noahshawtaylor.voxpal.vst3 --version "$VERSION" --install-location / "$PKGS/vst3.pkg"
pkgbuild --root "$STAGE/AU"   --identifier com.noahshawtaylor.voxpal.au   --version "$VERSION" --install-location / "$PKGS/au.pkg"
OUTLINE='<line choice="vst3"/><line choice="au"/>'
CHOICES='<choice id="vst3" title="VST3 (Ableton Live and others)"><pkg-ref id="com.noahshawtaylor.voxpal.vst3"/></choice>
  <choice id="au" title="Audio Unit (Logic Pro, GarageBand)"><pkg-ref id="com.noahshawtaylor.voxpal.au"/></choice>'
REFS="<pkg-ref id=\"com.noahshawtaylor.voxpal.vst3\" version=\"$VERSION\">vst3.pkg</pkg-ref>
  <pkg-ref id=\"com.noahshawtaylor.voxpal.au\" version=\"$VERSION\">au.pkg</pkg-ref>"
if [ -d "$ART/AAX/VOXPAL.aaxplugin" ]; then
  mkdir -p "$STAGE/AAX/Library/Application Support/Avid/Audio/Plug-Ins"
  cp -R "$ART/AAX/VOXPAL.aaxplugin" "$STAGE/AAX/Library/Application Support/Avid/Audio/Plug-Ins/"
  pkgbuild --root "$STAGE/AAX" --identifier com.noahshawtaylor.voxpal.aax --version "$VERSION" --install-location / "$PKGS/aax.pkg"
  OUTLINE="$OUTLINE<line choice=\"aax\"/>"
  CHOICES="$CHOICES
  <choice id=\"aax\" title=\"Pro Tools (AAX)\"><pkg-ref id=\"com.noahshawtaylor.voxpal.aax\"/></choice>"
  REFS="$REFS
  <pkg-ref id=\"com.noahshawtaylor.voxpal.aax\" version=\"$VERSION\">aax.pkg</pkg-ref>"
fi
cat > "$PKGS/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>VOXPAL $VERSION</title>
  <options customize="allow" require-scripts="false" rootVolumeOnly="true"/>
  <choices-outline>$OUTLINE</choices-outline>
  $CHOICES
  $REFS
</installer-gui-script>
XML
mkdir -p dist
OUT="dist/VOXPAL $VERSION.pkg"
if [ -n "$DEVELOPER_ID_INSTALLER" ]; then
  productbuild --distribution "$PKGS/distribution.xml" --package-path "$PKGS" --sign "$DEVELOPER_ID_INSTALLER" "$OUT"
else
  productbuild --distribution "$PKGS/distribution.xml" --package-path "$PKGS" "$OUT"
fi
echo "Built: $OUT"
