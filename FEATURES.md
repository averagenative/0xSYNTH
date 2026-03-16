# 0xSYNTH Features

## Synthesis Engines
- **Subtractive** — Saw/square/triangle/sine oscillators, dual-osc with mix/detune, 1-7 voice unison with stereo spread
- **FM (Frequency Modulation)** — 4-operator with 8 algorithms, per-operator ADSR envelopes, self-feedback
- **Wavetable** — 4 built-in banks (Analog, Harmonics, PWM, Formant), position morphing with envelope/LFO modulation, custom .wav import
- **Sampler** — WAV/FLAC/MP3 loading, Hermite interpolation, pitch shifting

## Oscillator Features
- **Dual oscillators** — Independent waveform selection, mix control, osc2 detune
- **Sub-oscillator** — Square or sine, -1 or -2 octaves below fundamental, level control
- **Noise oscillator** — White or pink noise with level control, mixed into oscillator output
- **Unison** — 1-7 voices with stereo spread and detune control
- **Wavetable import** — Load any .wav file as a custom wavetable bank (up to 4 user banks)

## Filters
- **7 filter types** — Lowpass, Highpass, Bandpass, Notch, Ladder (24dB/oct), Comb, Formant
- **Dual filters** — Independent type/cutoff/resonance/envelope depth per filter
- **Filter routing** — Serial (filter1 → filter2) or Parallel (mixed 50/50)
- **Cutoff smoothing** — Zipper-free parameter changes
- **Resonance** — Up to 20x with state clamping for stability

## Modulation
- **Modulation matrix** — 8 routing slots, any source → any parameter with bipolar depth
- **Mod sources** — LFO 1/2/3, Amp Envelope, Filter Envelope, Mod Wheel, Velocity, Aftertouch, Key Track, Macro 1-4
- **3 LFOs** — Independent waveform (sine/tri/square/saw), rate, depth, destination (pitch/filter/amp), BPM sync
- **ADSR envelopes** — Amp and Filter envelopes, interactive drag-to-adjust graph
- **4 Macro controls** — User-assignable performance knobs, routable via mod matrix

## Effects
- **3 effect slots** in series, 15 effect types:
  Filter, Delay (BPM sync), Reverb (Freeverb), Overdrive, Fuzz, Chorus,
  Bitcrusher, Compressor, Phaser, Flanger, Tremolo, Ring Mod, Tape Saturation, Shimmer Reverb
- **Per-effect bypass and mix** — 8 configurable parameters per slot

## Performance
- **Pitch bend wheel** — Snap-back or hold mode, ±1-24 semitone range, arrow key control
- **Modulation wheel** — MIDI CC1, available as mod matrix source
- **Mono/Legato mode** — Glide between notes without envelope retrigger
- **Polyphony** — 1-16 voices, 4 steal modes (oldest/quietest/lowest/highest)
- **MIDI CC Learn** — Map any CC to any parameter, persists in presets
- **Arpeggiator** — Up/Down/Up-Down/Random/As-Played modes, rate sync, gate control, 1-4 octave range, glowing enable button
- **MIDI note output** — QWERTY/virtual keyboard notes sent to DAW for recording (CLAP plugin)

## Recording
- **WAV recording** — 16/24/32-bit, real-time streaming to disk
- **FLAC recording** — Verbatim encoder, 16/24-bit, no external dependencies
- **MP3 recording** — Shine fixed-point encoder, 128-320 kbps, no external dependencies
- **Timestamped filenames** — `0xsynth_2026-03-16_143812.wav`
- **Format settings** — Selectable in Settings panel (gear icon)
- **Disk space monitoring** — Warns when < 500 MB free

## Presets
- **70 factory presets** — Subtractive, FM, and Wavetable across bass, lead, pad, keys, FX categories
- **User presets** — Save/load JSON files, overwrite confirmation
- **Session persistence** — Auto-saves theme, window size/position, octave, keyboard visibility every 30s
- **Randomize** — Generates random patches with guaranteed audible output

## GUI
- **ImGui + SDL2 + OpenGL** — Same rendering in standalone and plugin
- **7 themes** — Dark, Hacker, Midnight, Amber, Vaporwave, Neon, Light (Ctrl+T to cycle)
- **Oscilloscope** — Real-time waveform display, pinned at top, always visible
- **Custom knobs** — 0x808-style arc knobs with indicator line, shift-drag for precision, double-click to reset
- **Interactive envelopes** — Click and drag ADSR control points on the graph
- **Virtual keyboard** — 3-octave piano with QWERTY mapping, octave shift, C-note labels
- **Pitch/Mod wheels** — Physical-style grooved wheels next to keyboard
- **Accent collapsing headers** — Color-coded left edge bars for visual hierarchy
- **Arp glow toggle** — Press-in button with red pulse when active
- **Mode-aware UI** — Only shows relevant sections for current synthesis mode
- **Preset browser** — Dropdown with scroll-wheel, user patches section
- **Custom window chrome** — Borderless with drag, minimize, maximize, close (Windows)
- **Pinned toolbar** — Preset selector, Randomize, Reset, Theme, REC always visible at top
- **Settings panel** — Theme, recording format/quality via gear icon

## Plugin Formats
- **CLAP** — First-class, all parameters automatable, embedded ImGui GUI, MIDI output
- **VST3** — Compatibility layer via CPLUG
- **Standalone** — miniaudio audio, ALSA MIDI (Linux), zero external dependencies on Windows

## Architecture
- **Pure C engine** (C99/C11 for atomics), C++ for ImGui GUI only
- **Opaque API** — `synth_api.h` is the sole public interface
- **Thread-safe** — Atomic params, lock-free SPSC command/output queues
- **Real-time safe** — Zero allocations in audio path
- **Pre-commit hooks** — Build + test + API boundary check on every commit
- **11 test suites** — Foundation, subtractive, FM, wavetable, effects, presets, UI layout, sampler, MIDI CC, benchmark, preset round-trip
- **Cross-platform** — Linux, macOS, Windows (MinGW cross-compile from WSL)

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
- Oversampling (2x/4x) for aliasing reduction
- MPE support (per-note pitch bend, pressure, slide)
- Wavetable import UI (file picker — backend already implemented)
- SoundFont (SF2) support
- macOS/Linux standalone testing
- Windows installer
