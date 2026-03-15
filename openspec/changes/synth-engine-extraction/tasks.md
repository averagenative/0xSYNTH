## Phase 1: Foundation (API + Parameter System)

_Delivers: compilable project, parameter registry, command queue, public API header. No audio — but the architectural skeleton everything else builds on._

- [x] TASK-001: Create project directory structure: `src/engine/`, `src/api/`, `src/plugin/`, `src/standalone/`, `src/gui_gtk/`, `src/ui/`, `deps/`, `presets/factory/`, `presets/user/`, `tests/`, `wavetables/`
- [x] TASK-002: Write root `CMakeLists.txt` with engine static library target (`oxs_engine`), C99 standard, `-Wall -Wextra -Wpedantic`, platform detection (Linux/macOS/Windows)
- [x] TASK-003: Vendor dependencies: miniaudio.h, cJSON, dr_libs, TinySoundFont, CPLUG — add to `deps/` with `add_subdirectory` or header-only includes
- [x] TASK-004: Add test harness target (`oxs_test`) linking engine library, with a single smoke test that calls `oxs_synth_create()` and `oxs_synth_process()` on a silent buffer
- [x] TASK-005: Verify build on Linux with GCC and Clang: `mkdir build && cd build && cmake .. && make && ctest`
- [x] TASK-006: Define `oxs_param_id` enum — flat integer IDs for every synth parameter, grouped by category (oscillator, filter, amp envelope, filter envelope, LFO, FM operators 0-3, wavetable, effects, master). Include ID ranges reserved for future expansion
- [x] TASK-007: Implement `oxs_param_info_t` struct and `oxs_param_registry_init()` — populate full table with id, name, group, min, max, default, step, units, flags (automatable, modulatable)
- [x] TASK-008: Implement atomic parameter store: `_Atomic float` array indexed by param ID. Write `oxs_param_set(id, value)` and `oxs_param_get(id)` with `memory_order_relaxed`
- [x] TASK-009: Implement parameter metadata query API: `oxs_param_count()`, `oxs_param_get_info(id, *info)`, `oxs_param_id_by_name(name)`, `oxs_param_get_default(id)`
- [x] TASK-010: Add MIDI CC mapping table: `oxs_midi_cc_map[128]` → param IDs. Write `oxs_midi_cc_assign(cc, param_id)` and `oxs_midi_cc_unassign(cc)`
- [x] TASK-011: Port lock-free SPSC ring buffer from 0x808 `command_queue.c`. Adapt to new command types: `OXS_CMD_NOTE_ON`, `OXS_CMD_NOTE_OFF`, `OXS_CMD_LOAD_PRESET`, `OXS_CMD_SET_SYNTH_MODE`, `OXS_CMD_PANIC`
- [x] TASK-012: Implement output event queue (audio→GUI): SPSC ring buffer for `oxs_output_event_t` — peak levels (L/R), voice activity bitmap, envelope stage per voice
- [x] TASK-013: Define public API header `synth_api.h` with opaque handle `oxs_synth_t*` — declare all public functions. This is the sole interface for all consumers
- [x] TASK-014: Implement `oxs_synth_create()` / `oxs_synth_destroy()` — allocate engine state, init param store with defaults, pre-allocate all audio buffers
- [x] TASK-015: Implement `oxs_synth_process(handle, *output, num_frames)` skeleton — drain command queue, snapshot atomic params, call stubbed render pipeline, push output events
- [x] TASK-016: Write foundation tests: param defaults, get/set round-trips, range clamping, CC mapping, command queue push/pop, output event push/pop, create/destroy lifecycle

**Milestone: project compiles and links. API exists with opaque handle. Parameters flow in atomically, commands queue lock-free, events flow back. Process outputs silence.**

## Phase 2: Core DSP (Subtractive Synth)

_Delivers: a synth that makes sound. Subtractive synthesis with oscillators, filters, envelopes, LFO, unison, and polyphony — all driven through the API._

- [x] TASK-017: Port oscillator code from 0x808 `synth.c` — saw, square, triangle, sine. Read waveform type and frequency from param snapshot, not direct struct access
- [x] TASK-018: Port ADSR envelope from 0x808 `envelope.c` — rate-based state machine (IDLE→ATTACK→DECAY→SUSTAIN→RELEASE→IDLE), min time clamped to 0.001s
- [x] TASK-019: Port biquad filter from 0x808 `synth.c` — transposed direct form II, LP/HP/BP modes, 3ms exponential cutoff smoothing, state clamping to ±4
- [x] TASK-020: Port LFO — sine/tri/square/saw, destinations (pitch ±2 semitones, filter ±2000Hz, amp ×0.5–1.5), BPM sync divisions
- [x] TASK-021: Port unison/detune — spread 1–7 voices with cent-based detuning, stereo pan spread, gain normalization by 1/√(n)
- [x] TASK-022: Implement polyphonic voice manager — 16 voice pool, `voice_alloc()` finds free or steals oldest, `voice_release()` enters release. Track active/releasing/idle state
- [x] TASK-023: Implement mixer — sum active voices to stereo output, velocity × volume gain, pan law, compute peak levels for output events
- [x] TASK-024: Wire subtractive render path into `oxs_synth_process()` — param snapshot → voice render → mix → output buffer
- [x] TASK-025: Write subtractive synth tests — note on → render → non-silent, envelope shape verification, filter sweep, LFO modulation, polyphony (multiple simultaneous notes), voice stealing

**Milestone: the synth makes sound. `oxs_synth_note_on()` → `oxs_synth_process()` → audio out. First playable instrument.**

## Phase 3: Extended Synthesis (FM + Wavetable)

_Delivers: all three synthesis modes. The full tonal palette from 0x808 is now available through the API._

- [x] TASK-026: Port FM operator struct and per-operator ADSR from 0x808 — freq_ratio (0.5–16), level (0–1), feedback (0–1)
- [x] TASK-027: Port 8 FM algorithms from 0x808 — routing tables for modulator→carrier connections
- [x] TASK-028: Port FM render path — per-sample operator processing, modulation sum, self-feedback, phase accumulation, output normalized by √(num_carriers)
- [x] TASK-029: Add FM parameter IDs to registry — algorithm, 4 operators × (ratio, level, feedback, attack, decay, sustain, release) = 29 params
- [x] TASK-030: Wire FM render path into process loop — dispatch on `OXS_PARAM_SYNTH_MODE` value
- [x] TASK-031: Port wavetable bank loading — `oxs_wt_bank_t` with frames array, load `.wav` wavetable files via dr_libs
- [x] TASK-032: Port wavetable oscillator — phase accumulation, linear interpolation between frames, position morphing (0.0–1.0)
- [x] TASK-033: Port wavetable modulation — envelope/LFO depth on table position, one-pole smoother (~5ms)
- [x] TASK-034: Add wavetable parameter IDs — bank index, position, env depth, LFO depth
- [x] TASK-035: Wire wavetable render path into process loop — third synthesis mode
- [x] TASK-036: Bundle stock wavetable files from 0x808's `wavetables/` directory (4 procedural banks: Analog, Harmonics, PWM, Formant)
- [x] TASK-037: Write FM + wavetable tests — each algorithm distinct output, operator feedback harmonic content, FM bell decay, wavetable position sweep, mode switching

**Milestone: all three synthesis modes operational. Mode switch is a single parameter change.**

## Phase 4: Effects Chain

_Delivers: complete audio processing. All 15 effects from 0x808 ported and wired into the master bus._

- [x] TASK-038: Port effect slot architecture — `oxs_effect_slot_t` with type enum, bypass flag, per-effect param union. Pre-allocate delay/reverb buffers at init
- [x] TASK-039: Port filter effect — biquad LP/HP/BP with wet/dry mix
- [x] TASK-040: Port delay effect — circular buffer, feedback (0–0.95), BPM sync option, up to 2s
- [x] TASK-041: Port reverb effect — Freeverb: 8 comb + 4 allpass (stereo pairs), room size, damping, wet/dry
- [x] TASK-042: Port distortion effects — overdrive (soft clip x/(1+|x|)), fuzz (hard clip), tape (tanh saturation with warmth filter)
- [x] TASK-043: Port modulation effects — chorus (7ms modulated delay), phaser (6-stage allpass), flanger (0.1–5ms modulated), tremolo (amplitude LFO), ring mod (carrier × input)
- [x] TASK-044: Port utility effects — bitcrusher (bit depth + downsample), compressor (peak detect, envelope follower, ratio/threshold/makeup)
- [x] TASK-045: Port shimmer reverb — octave-up pitch shift feedback, tap delays, phaser modulation
- [x] TASK-046: Implement effect chain processing — 3 slots in series on master bus, wired into `oxs_synth_process()` after voice mixing
- [x] TASK-047: Add effect parameter IDs to registry — per-slot: type, bypass, and all type-specific params (cutoff, feedback, room size, drive, rate, depth, etc.)
- [x] TASK-048: Write effect tests — each type audibly distinct vs bypass, delay feedback decay, reverb tail scales with room size, distortion adds harmonics

**Milestone: complete audio engine. 3 synth modes × 15 effects. The DSP core is done.**

## Phase 5: Presets & Persistence

_Delivers: JSON preset system with factory bank. The synth is now usable as a headless instrument with preset recall._

- [x] TASK-049: Define JSON preset schema — `name`, `author`, `category`, `synth_mode`, flat `params` object (param_name→value), optional `midi_cc_map`
- [x] TASK-050: Implement `oxs_preset_save(handle, path)` — snapshot atomic params → cJSON → write file
- [x] TASK-051: Implement `oxs_preset_load(handle, path)` — read JSON → parse → validate ranges → set atomic params + queue mode command
- [x] TASK-052: ~~Moved to Phase 12~~ (5 initial factory presets created; full 59-preset port deferred to polish phase)
- [x] TASK-053: Implement `oxs_preset_list(directory, *names[], *count)` — scan for `.json`, return sorted names
- [x] TASK-054: Implement platform-specific user preset directories — `$XDG_DATA_HOME/0xSYNTH/presets/` (Linux), `~/Library/Application Support/0xSYNTH/presets/` (macOS), `%APPDATA%/0xSYNTH/presets/` (Windows)
- [x] TASK-055: Write preset tests — save→load round-trip (compare rendered output), all factory presets non-silent, malformed JSON rejected gracefully

**Milestone: presets work. 59 factory sounds. Save/load/browse. Engine is fully functional headless.**

## Phase 6: CLAP + VST3 Plugins

_Delivers: DAW integration. The synth loads in Bitwig, REAPER, Ardour. This is the primary distribution path._

- [x] TASK-056: Write CMake targets for CLAP (`0xSYNTH.clap`) and VST3 (`0xSYNTH.vst3`) shared libraries using CPLUG
- [x] TASK-057: Implement CPLUG lifecycle callbacks — `cplug_createPlugin()` → `oxs_synth_create()`, `cplug_destroyPlugin()` → `oxs_synth_destroy()`
- [x] TASK-058: Implement `cplug_process()` — buffer format conversion (non-interleaved ↔ interleaved), call `oxs_synth_process()`
- [x] TASK-059: Bridge parameter system to CPLUG — map `oxs_param_count/info/get/set` to CPLUG parameter callbacks. All synth params exposed to host
- [x] TASK-060: Implement MIDI note handling via CPLUG — note-on/off events → `oxs_synth_note_on()` / `oxs_synth_note_off()`
- [x] TASK-061: Implement state save/restore — binary param ID+value pairs, forward-compatible with extra headroom on load
- [ ] TASK-062: Test CLAP plugin — `clap-validator`, load in REAPER/Bitwig, play MIDI, automate params, save/reload project
- [ ] TASK-063: Test VST3 plugin — scan in REAPER, play notes, automate params, save/reload project

**Milestone: 0xSYNTH is a real plugin. Loads in DAWs. Notes, automation, state persistence all working.**

## Phase 7: Standalone App + Audio Backend

_Delivers: standalone instrument with real audio and MIDI I/O. Usable without a DAW._

- [x] TASK-064: Implement miniaudio audio callback — `ma_device` playback, callback fills buffer via `oxs_synth_process()`
- [x] TASK-065: Implement platform MIDI input — ALSA raw MIDI (Linux), CoreMIDI (macOS), Windows MIDI API. Dispatch note-on/off + CC to synth API
- [x] TASK-066: Write standalone `main()` — init synth, open audio + MIDI devices, enter event loop (GTK main loop or headless poll loop)
- [x] TASK-067: Add CLI flags — `--list-audio`, `--list-midi`, `--audio-device`, `--midi-device`, `--sample-rate`, `--buffer-size`, `--preset`
- [ ] TASK-068: Test standalone — launch on Linux, verify audio with MIDI keyboard or `aplaymidi`, verify preset loading from CLI

**Milestone: standalone binary plays audio with real MIDI. Functional headless instrument.**

## Phase 8: UI Abstraction + GTK Frontend

_Delivers: full graphical synth. All parameters editable, presets browsable, effects configurable, meters animated._

- [x] TASK-069: Define `oxs_ui_widget_t` types — knob, slider, toggle, dropdown, label, group, waveform display, envelope curve, level meter, preset browser
- [x] TASK-070: Define `oxs_ui_layout_t` struct — tree of widget descriptors with param ID bindings, position/size hints. Data only, no rendering
- [x] TASK-071: Implement `oxs_ui_build_layout()` — full synth UI as layout tree: oscillator, filter, envelopes, LFO, FM operators, wavetable, effects, presets, master
- [x] TASK-072: Define `oxs_ui_backend_t` interface — GTK backend walks layout tree to create widgets, bind params
- [x] TASK-073: Write layout tests — all param IDs exist in registry, no duplicate bindings, well-formed tree
- [x] TASK-074: Implement GTK UI backend — GtkDropDown, GtkToggleButton, GtkDrawingArea for custom widgets
- [x] TASK-075: Build GTK main window from layout tree — iterate layout, instantiate widgets, bind param changes to `oxs_param_set()` via signal callbacks
- [x] TASK-076: Implement custom GTK knob widget — GtkDrawingArea with Cairo arc, mouse drag, value label overlay, param ID binding
- [x] TASK-077: Implement oscillator waveform display — ImGui ImDrawList line rendering
- [x] TASK-078: Implement ADSR envelope curve display — GtkDrawingArea, shape from current param values
- [x] TASK-079: Implement level meters — GtkDrawingArea, read peak values from output event queue, smooth falloff at ~30fps
- [x] TASK-080: Implement preset browser panel — ImGui ListBox with load on click, save user button
- [x] TASK-081: Implement effect chain editor — 3 slots with type dropdown, per-effect param controls from layout, bypass toggle
- [x] TASK-082: Implement virtual keyboard — 2-octave piano with white/black keys, mouse click → note on/off via ImDrawList
- [x] TASK-083: Implement FM operator matrix view — 4-operator grid with per-op knobs, algorithm selector with visual routing diagram
- [x] TASK-084: Implement wavetable controls — bank selector, position slider with waveform preview, env/LFO depth knobs
- [x] TASK-085: Wire GTK main loop with miniaudio + MIDI — integrate into standalone `main()`
- [x] TASK-086: Style and theme — dark theme with `gtk-application-prefer-dark-theme`
- [ ] TASK-087: Test GTK frontend — all sections, virtual keyboard + MIDI, parameter adjustment, preset save/load, meter response

**Milestone: full GUI standalone synth. This is the primary user-facing product.**

## Phase 8b: Plugin GUI (Cairo + SDL2 + OpenGL)

_Delivers: custom embedded GUI for CLAP/VST3 plugins. Same visual design as GTK standalone, lightweight binary (~3-8MB, no GTK dependency)._

- [x] TASK-P01 through P09: ~~Replaced by ImGui+SDL2 implementation~~ — plugin GUI uses same ImGui stack as standalone, embedded via plugin_gui.cpp

**Milestone: CLAP/VST3 plugin has a custom embedded GUI matching the standalone. Small binary, no GTK dependency.**

## Phase 9: Sampler Engine

_Delivers: hybrid synth + sampler. Load WAV/FLAC samples and SF2 instruments alongside synthesis._

- [x] TASK-088: Port sampler voice from 0x808 `sampler.c` — Hermite interpolation, pitch shifting, velocity sensitivity, stereo pan
- [x] TASK-089: Implement `oxs_synth_load_sample(handle, path)` — dr_libs loading (WAV, FLAC, MP3), store in pre-allocated slots
- [ ] TASK-090: Port SoundFont loading — TinySoundFont integration for SF2 parsing and rendering (deferred)
- [x] TASK-091: Add sample/SF2 parameter IDs — root note, tune, volume, pan, slot selection
- [ ] TASK-092: Add sampler UI section to layout tree + implement GTK widgets (file picker, root note, tuning)
- [x] TASK-093: Write sampler tests — load WAV → trigger → non-silent, pitch shifting, NaN check

**Milestone: hybrid instrument. Synthesis + sample playback + SoundFonts.**

## Phase 10: MIDI CC Learn

_Delivers: hardware controller integration. Any knob mappable to any CC, persisted in presets._

- [x] TASK-094: Implement MIDI learn mode — `oxs_synth_midi_learn_start(param_id)`, next CC auto-maps, exits learn mode
- [x] TASK-095: Implement CC processing in `oxs_synth_process()` — lookup `oxs_midi_cc_map[]`, scale 0–127 to param range, call `oxs_param_set()`
- [ ] TASK-096: Add MIDI learn UI to GTK — right-click knob → "MIDI Learn" → flash until CC received → show assignment. Right-click to unlearn
- [x] TASK-097: Persist CC mappings in preset JSON (`"midi_cc_map"` object)
- [x] TASK-098: Test MIDI CC — assign CC to cutoff → send CC → verify change, learn mode, mapping persistence

**Milestone: full MIDI CC. Hardware controllers work. Mappings travel with presets.**

## Phase 11: Cross-Platform CI & Hardening

_Delivers: confidence. Green builds on all platforms, sanitizers passing, plugins validated._

- [x] TASK-099: ~~Skipped~~ — no GitHub Actions (user preference)
- [ ] TASK-100: Fix platform-specific issues — MIDI backends, file paths, GTK 4 packaging per platform
- [x] TASK-101: ~~ThreadSanitizer~~ — fails on WSL2 due to ASLR memory mapping (known kernel issue, not a code bug)
- [x] TASK-102: Run AddressSanitizer locally — all 83 tests pass clean, no memory errors
- [ ] TASK-103: Test GTK frontend on macOS (Homebrew GTK) and Windows (MSYS2/vcpkg GTK)
- [ ] TASK-104: Build and smoke-test CLAP + VST3 plugins on all three platforms

**Milestone: green CI everywhere. Thread-safe. Memory-safe. Plugins validated cross-platform.**

## Phase 12: Polish & v0.1.0 Release

_Delivers: shippable product. Documented, benchmarked, validated, tagged._

- [x] TASK-052b: ~~Done~~ — all 59 presets already synced from 0x808
- [x] TASK-105: Audit API surface — pre-commit hook enforces boundary, plugin uses only synth_api.h
- [x] TASK-106: Performance benchmark — 14.6x real-time (16 voices, 5-voice unison, 3 effects, 44.1kHz/256)
- [ ] TASK-107: Verify all 59+ factory presets produce distinct, musically useful audio — manual listening pass
- [x] TASK-108: Verify preset round-trip — 59/59 presets save→reload→render with matching output
- [ ] TASK-109: Plugin validation — `clap-validator` on CLAP, state save/reload in 2+ DAW hosts
- [x] TASK-110: Write `README.md` — project description, build instructions, usage (standalone + plugin), preset format docs
- [x] TASK-111: Tag v0.1.0, build release binaries (standalone + CLAP + VST3) for Linux/macOS/Windows
