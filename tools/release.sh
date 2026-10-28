#!/bin/bash
# Build, sign (ad hoc), run every test, make the installer. Usage: bash tools/release.sh
set -e
cd "$(dirname "$0")/.."
CM=~/dev/cm/bin/cmake; T=${TMPDIR:-/tmp}
$CM --build build-univ --config Release --target AeolaDrone_VST3 AeolaDrone_AU AeolaDrone_Standalone AeolaPluginTest -j4 2>&1 | grep -E "error|FAILED" && exit 1 || true
R=build-univ/AeolaDrone_artefacts/Release
for b in "$R/AU/Aeola.component" "$R/VST3/Aeola.vst3" "$R/Standalone/Aeola.app"; do codesign --force --deep -s - "$b" >/dev/null 2>&1; done
fail=0
build-univ/AeolaPluginTest_artefacts/Release/AeolaPluginTest 2>&1 | tail -1 | grep -q "ALL TESTS PASSED" || { echo "plugin_test FAILED"; fail=1; }
for t in sky_test guard_test mod_test engine_test filter_test timbre_test; do
  clang++ -std=c++17 -O2 -I Source tests/$t.cpp -o "$T/$t"
  (cd tests && "$T/$t" 2>&1 | tail -1 | grep -q "ALL TESTS PASSED") && echo "$t ok" || { echo "$t FAILED"; fail=1; }
done
[ $fail = 0 ] || exit 1
bash installer/build_installer.sh 2>&1 | tail -1
