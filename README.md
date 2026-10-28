# Aeola

An instrument that plays the aircraft flying over a place, live. Higher flight, higher note; closer, louder. Notes are kept in a key and scale so it stays musical.

Free and open source (GPL-3.0). Download, screenshots and details: https://orgfwd.github.io/aeola-vst/

Formats: VST3, Audio Unit, Standalone. macOS now; no Windows build yet.

## Build

Needs [JUCE](https://juce.com) 8 next to this folder (`../JUCE`) and CMake 3.22 or later.

    cmake -S . -B build-univ -DCMAKE_BUILD_TYPE=Release
    cmake --build build-univ --config Release --target AeolaDrone_VST3 AeolaDrone_AU AeolaDrone_Standalone AeolaPluginTest

`bash tools/release.sh` builds, signs ad hoc, runs every test and makes the installer (`dist/`). The test programs are in `tests/`.

The internal target name `AeolaDrone` and the bundle ids `com.alexskepp.aeoladrone.*` are kept on purpose so old projects and presets still load.

## Docs

- `docs/SPEC.md` current spec, `CHANGELOG.md`, `docs/BACKLOG.md` what is open.

## Credits

Live traffic: [adsb.lol](https://adsb.lol) (ODbL 1.0), optional backup [adsb.fi](https://adsb.fi) (personal, non-commercial use). Coastlines: [Natural Earth](https://www.naturalearthdata.com) (public domain). Built with [JUCE](https://juce.com) under the GPL.

Feedback: aeola@skepp.se
