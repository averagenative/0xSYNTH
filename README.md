# 0xSYNTH

Multi-engine synthesizer — subtractive, FM (4-operator, 8 algorithms), and wavetable synthesis with 15 audio effects, sample playback, and MIDI CC learn.

Ships as a **CLAP plugin**, **VST3 plugin**, and **standalone app** with GTK 4 GUI. Pure C (C99/C11), real-time safe, cross-platform (Linux/macOS/Windows).

## Build

```bash
# Linux (requires GTK 4 dev packages for GUI)
sudo apt install libgtk-4-dev   # Debian/Ubuntu
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure

# Windows cross-compile (headless, no GUI)
mkdir build-win64 && cd build-win64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64.cmake \
         -DBUILD_PLUGINS=OFF -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON
make -j$(nproc)
```

## Build Output

| Target | File | Size |
|--------|------|------|
| Standalone | `0xsynth` | ~2.5 MB |
| CLAP plugin | `0xSYNTH.clap` | ~284 KB |
| VST3 plugin | `0xSYNTH.vst3` | ~483 KB |

## Run

```bash
# Standalone with GTK GUI
./build/0xsynth

# Headless (MIDI keyboard only)
./build/0xsynth --headless --preset presets/factory/SuperSaw.json

# List devices
./build/0xsynth --list-audio
./build/0xsynth --list-midi

# CLAP/VST3: copy to your DAW's plugin folder
cp build/0xSYNTH.clap ~/.clap/
cp build/0xSYNTH.vst3 ~/.vst3/
```

## Synthesis Engines

- **Subtractive** — Saw/square/triangle/sine oscillators, biquad filter (LP/HP/BP), ADSR envelopes, LFO, 7-voice unison with stereo detune
- **FM** — 4 operators, 8 algorithms, per-operator envelopes and self-feedback
- **Wavetable** — 4 built-in banks (Analog, Harmonics, PWM, Formant), position morphing with envelope/LFO modulation
- **Sampler** — WAV/FLAC/MP3 loading, Hermite interpolation, pitch shifting

## Effects (15 types)

Filter, Delay (BPM sync), Reverb (Freeverb), Overdrive, Fuzz, Chorus, Bitcrusher, Compressor, Phaser, Flanger, Tremolo, Ring Mod, Tape Saturation, Shimmer Reverb

3 effect slots in series on the master bus.

## Presets

59 factory presets in JSON format — editable, shareable, scriptable.

```bash
ls presets/factory/
# Bass.json, SuperSaw.json, FM Bell.json, DX Piano.json, ...
```

User presets saved to `~/.local/share/0xSYNTH/presets/` (Linux).

## Architecture

```
synth_api.h (public API — opaque handle)
    ├── Atomic params (continuous knobs)
    ├── Command queue (note on/off, preset load)
    └── Output events (peaks, voice activity)

Engine internals (private):
    oscillator → filter → envelope → LFO → voice manager → mixer → effects
```

All consumers (GTK GUI, CLAP/VST3 plugin, standalone) interact exclusively through `synth_api.h`. No direct struct access. Thread-safe by design.

## MIDI CC

Any parameter can be mapped to any MIDI CC. Learn mode: call `oxs_synth_midi_learn_start(param_id)`, send a CC, it auto-assigns. Mappings persist in presets.

## License

TBD
