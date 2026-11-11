#!/bin/bash
# Builds "Install Nimbus.pkg" and wraps it with the manual in a .dmg.
# Usage: installer/make_installer.sh <version>   (run from the repo root on macOS, after building)
set -euo pipefail
VERSION="$1"
ART=build/Nimbus_artefacts/Release
WORK=installer/work
rm -rf "$WORK"; mkdir -p "$WORK"/{stage/vst3,stage/au,stage/app,pkgs,res,dmg}

cp -R "$ART/VST3/Nimbus.vst3"         "$WORK/stage/vst3/"
cp -R "$ART/AU/Nimbus.component"      "$WORK/stage/au/"
cp -R "$ART/Standalone/Nimbus.app"    "$WORK/stage/app/"

build_component () {  # name identifier install-location [scripts]
  local name=$1 id=$2 loc=$3 scripts=${4:-}
  pkgbuild --analyze --root "$WORK/stage/$name" "$WORK/$name.plist"
  plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$name.plist"
  local extra=()
  [ -n "$scripts" ] && extra=(--scripts "$scripts")
  pkgbuild --root "$WORK/stage/$name" --component-plist "$WORK/$name.plist" \
    --identifier "$id" --version "$VERSION" --install-location "$loc" ${extra[@]+"${extra[@]}"} "$WORK/pkgs/$name.pkg"
}
chmod +x installer/scripts/postinstall
build_component vst3 com.javi.nimbus.vst3 /Library/Audio/Plug-Ins/VST3 installer/scripts
build_component au   com.javi.nimbus.au   /Library/Audio/Plug-Ins/Components
build_component app  com.javi.nimbus.app  /Applications

sed "s/__VERSION__/$VERSION/g" installer/distribution.xml > "$WORK/distribution.xml"
for f in installer/resources/*.html; do sed "s/__VERSION__/$VERSION/g" "$f" > "$WORK/res/$(basename "$f")"; done

productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" \
  --package-path "$WORK/pkgs" "$WORK/dmg/Install Nimbus.pkg"

cp "docs/Nimbus Owner's Manual.pdf" "$WORK/dmg/"
cp installer/READ_ME_FIRST.txt "$WORK/dmg/Read Me First.txt"

mkdir -p dist
hdiutil create -volname "Nimbus $VERSION" -srcfolder "$WORK/dmg" -ov -format UDZO "dist/Nimbus-$VERSION-macOS.dmg"
echo "Built dist/Nimbus-$VERSION-macOS.dmg"
