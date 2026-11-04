#!/bin/bash
# Build VOXPAL on macOS and install VST3 + AU into the user plugin folders.
# Usage:  ./scripts/build-mac.sh [/path/to/aax-sdk]
set -e
cd "$(dirname "$0")/.."
AAX=""
if [ -n "$1" ]; then AAX="-DAAX_SDK_PATH=$1"; fi
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 $AAX
cmake --build build --config Release -- -quiet
ART="build/VocalChain_artefacts/Release"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/"VOXPAL.vst3" ~/Library/Audio/Plug-Ins/Components/"VOXPAL.component"
cp -R "$ART/VST3/VOXPAL.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "$ART/AU/VOXPAL.component" ~/Library/Audio/Plug-Ins/Components/
# ad-hoc sign so Gatekeeper and Logic's AU validation accept a local build
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/"VOXPAL.vst3"
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/"VOXPAL.component"
if [ -d "$ART/AAX/VOXPAL.aaxplugin" ]; then
  echo "AAX built at $ART/AAX/VOXPAL.aaxplugin  (needs PACE signing before release Pro Tools loads it)"
fi
# ask Logic/GarageBand to re-scan AUs
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Installed. Rescan plugins in Ableton; Logic will pick up the AU on next launch."
echo "AU validation:  auval -v aufx Vxpl Nsta"
