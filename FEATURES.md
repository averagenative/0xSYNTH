# 0xSYNTH Features

## Synthesis Engines
- **Subtractive** — Saw/square/triangle/sine oscillators, dual-osc with mix/detune, 1-7 voice unison with stereo spread
- **FM (Frequency Modulation)** — 4-operator with 8 algorithms, per-operator ADSR envelopes, self-feedback
- **Wavetable** — 4 built-in banks (Analog, Harmonics, PWM, Formant), position morphing with envelope/LFO modulation
- **Sampler** — WAV/FLAC/MP3 loading, Hermite interpolation, pitch shifting

## Audio Processing
- **Biquad filter** — Lowpass/Highpass/Bandpass, cutoff smoothing, resonance up to 20
- **ADSR envelopes** — Amp and Filter envelopes, interactive drag-to-adjust graph
- **LFO** — Sine/triangle/square/saw, destinations: pitch/filter/amp, BPM sync
- **Effects chain** — 3 slots in series, 15 effect types:
  Filter, Delay (BPM sync), Reverb (Freeverb), Overdrive, Fuzz, Chorus,
  Bitcrusher, Compressor, Phaser, Flanger, Tremolo, Ring Mod, Tape Saturation, Shimmer Reverb
- **Arpeggiator** — Up/Down/Up-Down/Random/As-Played modes, rate sync, gate control, 1-4 octave range

## Performance
- **Pitch bend wheel** — Snap-back or hold mode, ±1-24 semitone range, arrow key control
- **Modulation wheel** — Controls LFO depth in real-time
- **Mono/Legato mode** — Glide between notes without envelope retrigger
- **Polyphony** — 1-16 voices, 4 steal modes (oldest/quietest/lowest/highest)
- **MIDI CC Learn** — Map any CC to any parameter, persists in presets

## Presets
- **70 factory presets** — Subtractive, FM, and Wavetable across bass, lead, pad, keys, FX categories
- **User presets** — Save/load JSON files, overwrite confirmation
- **Session persistence** — Auto-saves state on exit, restores on launch
- **Randomize** — Generates random patches with guaranteed audible output

## GUI
- **ImGui + SDL2 + OpenGL** — Same rendering in standalone and plugin
- **7 themes** — Dark, Hacker, Midnight, Amber, Vaporwave, Neon, Light (Ctrl+T to cycle)
- **Custom knobs** — 0x808-style arc knobs with indicator line, shift-drag for precision, double-click to reset
- **Interactive envelopes** — Click and drag ADSR control points on the graph
- **Virtual keyboard** — 3-octave piano with QWERTY mapping, octave shift, C-note labels
- **Pitch/Mod wheels** — Physical-style grooved wheels next to keyboard
- **Mode-aware UI** — Only shows relevant sections for current synthesis mode
- **Preset browser** — Dropdown with scroll-wheel, user patches section
- **Custom window chrome** — Borderless with drag, minimize, maximize, close (Windows)

## Plugin Formats
- **CLAP** — First-class, all parameters automatable, embedded ImGui GUI
- **VST3** — Compatibility layer via CPLUG
- **Standalone** — miniaudio audio, ALSA MIDI (Linux), zero external dependencies on Windows

## Architecture
- **Pure C engine** (C99/C11 for atomics), C++ for ImGui GUI only
- **Opaque API** — `synth_api.h` is the sole public interface
- **Thread-safe** — Atomic params, lock-free SPSC command/output queues
- **Real-time safe** — Zero allocations in audio path, 14.6x real-time performance
- **Pre-commit hooks** — Build + test + API boundary check on every commit
- **11 test suites** — Foundation, subtractive, FM, wavetable, effects, presets, UI layout, sampler, MIDI CC, benchmark, preset round-trip

## Keyboard Shortcuts
| Key | Action |
|-----|--------|
| Z-M, comma, period, / | Play lower octave |
| Q-P | Play upper octave |
| S,D,G,H,J,L,; | Lower octave sharps |
| 2,3,5,6,7,9,0 | Upper octave sharps |
| Left/Right arrow | Shift octave |
| Up/Down arrow | Pitch bend |
| Ctrl+T | Cycle theme |
| Ctrl+K | Toggle keyboard |
| Shift+Drag | Fine knob adjustment |
| Double-click knob | Reset to center |
| Scroll wheel | Cycle dropdown values |
| ESC | Close window |

## Future (Backlog)
- Modulation matrix (route mod wheel/LFO to any parameter)
- SoundFont (SF2) support
- Plugin embedded GUI for DAW window
- macOS/Linux standalone testing
- Windows installer
