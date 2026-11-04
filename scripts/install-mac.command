#!/bin/bash
# Double-click to install VOXPAL (VST3 + AU) from a downloaded build.
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/"VOXPAL.vst3" ~/Library/Audio/Plug-Ins/Components/"VOXPAL.component"
cp -R "VOXPAL.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "VOXPAL.component" ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"VOXPAL.vst3" ~/Library/Audio/Plug-Ins/Components/"VOXPAL.component" 2>/dev/null || true
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/"VOXPAL.vst3"
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/"VOXPAL.component"
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Installed VOXPAL. Rescan in Ableton; relaunch Logic."
read -n 1 -s -r -p "Press any key to close."
