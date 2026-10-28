#!/bin/bash
# Builds dist/Aeola-<version>.pkg with selectable components (Audio Unit, VST3, Standalone).
# Usage: installer/build_installer.sh   (run after building + ad-hoc signing the bundles in build-univ/; AEOLA_BUILD overrides the dir)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=$(grep -m1 -oE 'VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | tail -1 | awk '{print $2}')
R=${AEOLA_BUILD:-build-univ}/AeolaDrone_artefacts/Release
OUT=dist; STAGE=$(mktemp -d); mkdir -p "$OUT"
for b in "$R/AU/Aeola.component" "$R/VST3/Aeola.vst3" "$R/Standalone/Aeola.app"; do
  codesign --verify --deep "$b" || { echo "unsigned/broken bundle: $b"; exit 1; }
done
mk() {  # name  identifier  source-bundle  install-location
  local root="$STAGE/root_$1"; mkdir -p "$root"; cp -R "$3" "$root/"
  pkgbuild --analyze --root "$root" "$STAGE/$1.plist" >/dev/null
  /usr/bin/python3 - "$STAGE/$1.plist" <<'P'
import plistlib, sys
p = sys.argv[1]; d = plistlib.load(open(p, 'rb'))
for e in d: e['BundleIsRelocatable'] = False; e['BundleOverwriteAction'] = 'upgrade'
plistlib.dump(d, open(p, 'wb'))
P
  # the product was called "Aeola Drone" and then "Aeola VST": remove the old bundles so the host does not see two copies with the same plugin ID
  local sc="$STAGE/scripts_$1" ext="${3##*.}"; mkdir -p "$sc"
  printf '#!/bin/sh\nrm -rf "%s/Aeola Drone.%s" "%s/Aeola VST.%s"\nexit 0\n' "$4" "$ext" "$4" "$ext" > "$sc/preinstall"; chmod +x "$sc/preinstall"
  pkgbuild --root "$root" --component-plist "$STAGE/$1.plist" --scripts "$sc" --identifier "$2" --version "$VERSION" --install-location "$4" "$STAGE/$1.pkg" >/dev/null
}
mk au         com.alexskepp.aeoladrone.au         "$R/AU/Aeola.component"            /Library/Audio/Plug-Ins/Components
mk vst3       com.alexskepp.aeoladrone.vst3       "$R/VST3/Aeola.vst3"               /Library/Audio/Plug-Ins/VST3
mk standalone com.alexskepp.aeoladrone.standalone "$R/Standalone/Aeola.app"          /Applications

mkdir -p "$STAGE/res"; cp LICENSE "$STAGE/res/license.txt"
cat > "$STAGE/res/welcome.html" <<H
<html><body style="font-family:-apple-system,Helvetica;font-size:13px"><h2>Aeola $VERSION</h2>
<p>A living drone instrument: a morphing additive sound engine, a ladder filter with drive, a modulation matrix, pure-interval tuning (North) and a Sky that plays notes from air traffic.</p>
<p><b>New in 1.6:</b> the aircraft overhead right now play it: higher flight, higher note; closer, louder and brighter. Every kind of aircraft has its own voice (freight, heavy jets, helicopters, business jets, turboprops, military). Listen at an airport or anywhere on earth. Simple mode shows just the sky; Advanced lets you shape every class, the tuning and the synth. Presets change the sound, never where you listen. The plug-in is called <b>Aeola</b>; this installer removes old <i>Aeola Drone</i> and <i>Aeola VST</i> files in /Library/Audio/Plug-Ins.</p>
<p><b>Choose your formats</b> on the next steps: Audio Unit (Logic, GarageBand, MainStage), VST3 (Ableton Live, Reaper, Bitwig, Cubase, ...) and the optional standalone app.</p>
<p>This is a <b>universal build</b>: it runs natively on Apple-silicon and Intel Macs (macOS 11 or later on Apple silicon, 10.13 or later on Intel). It is not notarized by Apple, see the Read Me.</p></body></html>
H
cat > "$STAGE/res/readme.html" <<H
<html><body style="font-family:-apple-system,Helvetica;font-size:13px"><h2>Read Me</h2>
<ul><li><b>Not notarized.</b> macOS may warn about an unidentified developer. Open the installer with right-click &rarr; Open, or allow it in System Settings &rarr; Privacy &amp; Security.</li>
<li>Installed to <code>/Library/Audio/Plug-Ins/Components</code> (AU), <code>/Library/Audio/Plug-Ins/VST3</code> (VST3) and <code>/Applications</code> (Standalone). Choosing "Install for me only" puts the plug-ins under your own <code>~/Library</code> instead.</li>
<li><b>VST2 is not offered.</b> Steinberg ended VST2 licensing; use VST3, which all current hosts support.</li>
<li>After installing, rescan plug-ins in your host. Logic: Settings &rarr; Plug-in Manager &rarr; Reset &amp; Rescan Selection.</li>
<li>Live traffic comes from adsb.lol (ODbL 1.0). Aeola is free software, GPL-3.0-or-later.</li>
<li>Uninstall: delete the files listed above.</li></ul></body></html>
H
cat > "$STAGE/distribution.xml" <<D
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Aeola $VERSION</title>
  <organization>com.alexskepp</organization>
  <domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="true"/>
  <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <welcome file="welcome.html"/><license file="license.txt"/><readme file="readme.html"/>
  <choices-outline>
    <line choice="au"/><line choice="vst3"/><line choice="standalone"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (AU)" description="For Logic Pro, GarageBand, MainStage and other Audio Unit hosts." start_selected="true"><pkg-ref id="com.alexskepp.aeoladrone.au"/></choice>
  <choice id="vst3" title="VST3" description="For Ableton Live, Reaper, Bitwig, Cubase, Studio One and other VST3 hosts." start_selected="true"><pkg-ref id="com.alexskepp.aeoladrone.vst3"/></choice>
  <choice id="standalone" title="Standalone app" description="Run Aeola on its own, without a host (optional)." start_selected="false"><pkg-ref id="com.alexskepp.aeoladrone.standalone"/></choice>
  <pkg-ref id="com.alexskepp.aeoladrone.au" version="$VERSION" onConclusion="none">au.pkg</pkg-ref>
  <pkg-ref id="com.alexskepp.aeoladrone.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
  <pkg-ref id="com.alexskepp.aeoladrone.standalone" version="$VERSION" onConclusion="none">standalone.pkg</pkg-ref>
</installer-gui-script>
D
productbuild --distribution "$STAGE/distribution.xml" --resources "$STAGE/res" --package-path "$STAGE" "$OUT/Aeola-$VERSION.pkg" >/dev/null
rm -rf "$STAGE"; ls -la "$OUT/Aeola-$VERSION.pkg"
