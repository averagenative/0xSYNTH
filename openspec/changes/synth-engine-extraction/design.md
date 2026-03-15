## Context

0xSYNTH is extracted from the 0x808 drum machine project. The 0x808 synth engine — subtractive, FM (4-op, 8 algorithms), and wavetable synthesis with 15 effect types and 59 factory presets — is tightly coupled to the step sequencer and transport system. Frontends directly mutate `sq_synth_preset_t`, `sq_synth_voice_t`, and `sq_engine_t` structs from the GUI thread, creating data races and making the synth impossible to reuse outside the drum machine context.

This design describes how to extract the synth into a standalone instrument with a clean API boundary (`synth_api.h`), correct real-time threading, CLAP/VST3 plugin support, and a GTK 4 frontend.

Constraints:
- Pure C (C99, C11 for `_Atomic`), CMake build system
- Must run on Linux, macOS, and Windows
- Must work as standalone app, CLAP plugin, and VST3 plugin
- Audio callback must be real-time safe: no allocations, no locks, no I/O
- All dependencies must be free/open-source with permissive licenses
- The 0x808 project continues independently — 0xSYNTH has no dependency on it

Source material:
- DSP code ported from `0x808/src/engine/` (synth.c, effects.c, envelope.c, sampler.c, mixer.c)
- Lock-free SPSC queue adapted from `0x808/src/engine/command_queue.h`
- CPLUG integration patterns from `0x808/src/plugin/plugin.c`
- 59 hardcoded C presets converted to JSON

## Goals / Non-Goals

**Goals:**
- Clean API boundary (`synth_api.h`) — opaque `oxs_synth_t*` handle, no struct exposure, all interaction through public functions
- Hybrid threading model: `_Atomic float` params for continuous knobs + lock-free SPSC command queue for discrete events + output event queue for GUI readback
- CLAP-first plugin architecture via CPLUG, VST3 as compatibility layer
- Parameter system with integer IDs mapping to CLAP params, full metadata (name, range, default, units, group), MIDI CC support
- JSON presets via cJSON (factory bank of 59+ presets, user preset directories)
- GTK 4 frontend via shared UI abstraction layer (data-driven layout tree)
- Sampler voices with Hermite interpolation + SoundFont (SF2) support via TinySoundFont
- Standalone application with miniaudio audio backend and platform MIDI input
- Cross-platform CI (Linux GCC/Clang, macOS Clang, Windows MSVC)

**Non-Goals:**
- Sequencer, transport, or pattern system (stays in 0x808)
- ImGui frontend (deferred — UI abstraction layer enables it later without architectural changes)
- MPE / per-note expression (future version)
- Audio recording or export
- Network/OSC control
- Mobile platforms (iOS/Android)
- Plugin hosting or send/return routing

## Decisions

### Decision 1: Opaque handle API

**Choice**: `oxs_synth_t*` is an opaque pointer returned by `oxs_synth_create()` and passed to all public functions. Internal structs are defined only in `.c` files, never exposed in `synth_api.h`.

**Alternatives considered**:
- **Public structs with discipline** — consumers include the full struct definitions but agree not to touch internals directly. Rejected because this is exactly what 0x808 does, and it failed. The GTK frontend in 0x808 directly writes to `engine->synth_presets[i].filter_cutoff` from the GUI thread. The plugin layer in `plugin.c` calls `apply_param_to_engine()` which writes to `engine->synth_presets[i]` fields with no synchronization. The "discipline" approach is not enforceable in C — if the struct is visible, it will be mutated.
- **C++ with private members** — rejected because the project is pure C, and the developer's toolchain and expertise are C-focused.

**Rationale**: An opaque handle is the only mechanism C provides to enforce an API boundary. If a consumer cannot see the struct layout, they cannot bypass the API. All parameter access goes through `oxs_param_set()` / `oxs_param_get()` (atomic), all events go through the command queue, and all state readback goes through the output event queue. This eliminates the class of threading bugs that 0x808 has.

### Decision 2: Hybrid parameter model (atomics + command queue)

**Choice**: Two communication channels from GUI to audio thread:
1. **Atomic parameter store** — `_Atomic float params[OXS_PARAM_COUNT]` array indexed by param ID. Written by GUI thread via `oxs_param_set(id, value)` with `memory_order_relaxed`. Read by audio thread at the start of each `oxs_synth_process()` call.
2. **Lock-free SPSC command queue** — ring buffer of `oxs_command_t` structs for discrete events: `OXS_CMD_NOTE_ON`, `OXS_CMD_NOTE_OFF`, `OXS_CMD_LOAD_PRESET`, `OXS_CMD_SET_SYNTH_MODE`, `OXS_CMD_PANIC`. Adapted from 0x808's `sq_command_queue_t` (same SPSC ring buffer with atomic indices, power-of-2 size).

**Alternatives considered**:
- **All-command-queue** — every parameter change (including continuous knobs like filter cutoff, envelope times, LFO rate) goes through the command queue. Rejected because continuous parameters change every GUI frame during a knob drag — at 60fps that's 60 commands/sec per knob. If a user drags two knobs simultaneously, that's 120 commands/sec. A complex automation recording could push hundreds of commands per second. The queue would need to be very large, and the audio thread would spend significant time draining it. Atomics are zero-overhead for this use case: one `atomic_store_explicit` per knob change, one bulk copy per audio buffer.
- **All-atomics** — every event (including note on/off) communicated via atomic flags. Rejected because note events carry associated data (note number, velocity, channel) and need ordering guarantees. Two rapid note-on events for the same note must be processed in order — atomics cannot guarantee this. The command queue provides FIFO ordering naturally.
- **Mutex-protected shared state** — rejected because mutexes can cause priority inversion in real-time audio threads. The audio callback must never block.

**Rationale**: The hybrid model matches the nature of the data. Continuous parameters (filter cutoff, envelope attack time, LFO rate, mix levels) are single float values that the audio thread can read at any time — a stale value from 1ms ago is inaudible. Discrete events (note on/off, preset load) need guaranteed delivery and ordering. Splitting the two channels gives both optimal performance (zero-overhead atomics for ~120-150 continuous params) and correctness (FIFO ordering for events).

### Decision 3: Parameter snapshot in process()

**Choice**: At the start of each `oxs_synth_process()` call, copy all `_Atomic float` params into a local `oxs_param_snapshot_t` struct. All DSP code within the buffer reads from the snapshot, never from the atomic array.

```
process() entry:
  1. Drain command queue (note on/off, preset load, etc.)
  2. for (i = 0; i < OXS_PARAM_COUNT; i++)
       snapshot.values[i] = atomic_load_explicit(&params[i], memory_order_relaxed);
  3. Render voices using snapshot
  4. Mix, apply effects using snapshot
  5. Push output events
```

**Alternatives considered**:
- **Read atomics per-sample** — each DSP function reads the atomic param it needs on every sample. Rejected because a GUI thread could change a param mid-buffer, causing the value to change partway through a 256-sample block. For most params this causes a click or discontinuity. For linked params (e.g., filter cutoff + resonance) this causes inconsistent filter coefficient computation.
- **No snapshot, read per-block but inline** — each DSP function reads its own params at the start of its processing. Rejected because different functions would snapshot at different times within the same `process()` call, creating potential inconsistencies between related params.

**Rationale**: The snapshot ensures all DSP code sees a consistent, frozen parameter state for the duration of one audio buffer. Cost is ~150 `atomic_load_explicit` calls + 150 float copies per buffer — roughly 600 bytes of memory copies. At 44.1kHz with 256-sample buffers, this happens ~172 times/sec. The cost is negligible compared to oscillator, filter, and effect computation.

### Decision 4: Output event queue (audio to GUI)

**Choice**: A second SPSC ring buffer (`oxs_output_queue_t`) pushing `oxs_output_event_t` from the audio thread to the GUI thread. Each event contains:
- Peak levels (L/R float pair)
- Voice activity bitmap (`uint16_t`, one bit per voice, 16 voices)
- Per-voice envelope stage (4 bits per voice packed into `uint64_t`)

The audio thread pushes one event per `oxs_synth_process()` call (~172/sec at 256-sample buffers). The GUI pops at its own rate (~30-60fps via `oxs_synth_pop_output_event()`).

**Alternatives considered**:
- **Shared atomic struct** — a single `_Atomic` struct that the audio thread overwrites and the GUI thread reads. Rejected because `oxs_output_event_t` is too large for a single atomic operation (C11 `_Atomic` on structs > 8 bytes is not lock-free on most platforms, defeating the purpose).
- **GUI reads engine state directly** — the GUI thread reads `engine->synth_voices[i].amp_env.stage` and `engine->master_peak[0]` directly. Rejected because this is the exact 0x808 antipattern — unsynchronized cross-thread reads of mutable engine state. It works most of the time but is technically undefined behavior.
- **Callback from audio thread** — the audio thread calls a GUI-provided function pointer. Rejected because the audio callback must not call into GUI code (unpredictable latency, potential locks in GUI toolkit).

**Rationale**: The SPSC queue is the same primitive we already use for GUI-to-audio commands, just in the reverse direction. It is lock-free, bounded, and the producer (audio thread) never blocks. If the GUI is slow and the queue fills, the audio thread drops events — this is correct behavior (the GUI can miss a few meter updates without consequence).

### Decision 5: Parameter registry with metadata

**Choice**: A static `oxs_param_info_t` table populated at init by `oxs_param_registry_init()`. Each entry contains:

| Field | Type | Purpose |
|-------|------|---------|
| `id` | `uint32_t` | Unique integer ID (enum `oxs_param_id`) |
| `name` | `char[48]` | Human-readable name |
| `group` | `char[24]` | Category (Oscillator, Filter, Amp Env, etc.) |
| `min` | `float` | Minimum value |
| `max` | `float` | Maximum value |
| `default_val` | `float` | Default value |
| `step` | `float` | Step size (0 = continuous) |
| `units` | `char[8]` | Display units (Hz, dB, ms, %, etc.) |
| `flags` | `uint32_t` | Automatable, integer, boolean, etc. |

Estimated parameter count: ~120-150 total, organized into groups:
- Oscillator (osc1 wave, osc2 wave, mix, detune, unison voices, unison detune): ~8
- Filter (type, cutoff, resonance, env depth): ~4
- Amp envelope (ADSR): 4
- Filter envelope (ADSR): 4
- LFO (waveform, rate, depth, dest, BPM sync, sync division): ~6
- FM operators 0-3 (ratio, level, feedback, ADSR each): 28
- FM global (algorithm): 1
- Wavetable (bank, position, env depth, LFO depth): 4
- Effects (3 slots x type + bypass + ~8 type-specific params): ~30
- Master (volume, synth mode): ~2
- Sampler (root note, tune, volume, pan, slot): ~5
- Reserved ranges for future expansion

**Alternatives considered**:
- **String-keyed parameter map** — parameters identified by string names. Rejected because CLAP uses integer param IDs, and string comparison in the audio thread (for MIDI CC lookup) is wasteful. Integer IDs allow O(1) array indexing.
- **No registry, hardcoded per-consumer** — each consumer (CLAP wrapper, GTK UI, standalone) maintains its own parameter table. Rejected because this is what 0x808 does: `plugin.c` has `PARAM_DEFS[18]` with hardcoded ranges, while the GTK frontend reads struct fields directly with different assumptions about ranges. A single registry is the source of truth.

**Rationale**: CLAP requires parameter metadata for host integration (automation lanes, parameter lists, value display). MIDI CC mapping needs min/max for scaling 0-127 to parameter range. The UI abstraction layer needs metadata to generate appropriate widgets (knob vs. dropdown vs. toggle). One table serves all three consumers. The 0x808 plugin exposes only 18 params because adding more requires manual duplication in `PARAM_DEFS` — with a registry, all ~150 params are automatically available to every consumer.

### Decision 6: CLAP-first via CPLUG

**Choice**: Use CPLUG to implement both CLAP and VST3 plugin formats. CLAP is the primary target; VST3 is a compatibility layer.

**Alternatives considered**:
- **Raw CLAP API** — implement the CLAP C headers directly without an abstraction layer. Rejected because CPLUG handles significant boilerplate (entry points, extension negotiation, GUI embedding, thread marshaling) and also provides VST3 support essentially for free. The 0x808 project already uses CPLUG successfully.
- **JUCE** — rejected (C++ framework, GPL license for open-source, heavyweight).
- **DPF (DISTRHO Plugin Framework)** — C++ framework, lighter than JUCE but still C++.

**Rationale**: CLAP has no licensing requirements (unlike Steinberg's VST3 SDK which requires a license agreement), has a better parameter model with per-note expressions and thread-safe parameter access conventions, and has growing DAW adoption (Bitwig native, REAPER, Ardour, FL Studio). VST3 comes nearly free through CPLUG for hosts that don't support CLAP yet. CPLUG is actively maintained, small enough to patch if needed, and proven in the 0x808 codebase. The 0xSYNTH CPLUG integration will be simpler than 0x808's because there is no sequencer transport to sync — just params, MIDI, and audio.

### Decision 7: ImGui+SDL2+OpenGL everywhere — zero external dependencies

**Choice**: One rendering stack for all GUI targets:
- **Standalone app**: SDL2 window + OpenGL + Dear ImGui. Ships as a single binary (+ SDL2.dll on Windows, SDL2.framework in .app bundle on macOS). No GTK, no Homebrew, no MSYS2. Double-click and it opens.
- **CLAP/VST3 plugin**: Same stack, SDL2 window embedded inside the DAW via CPLUG's native handle (`SetParent` on Windows, XReparent on Linux, NSView on macOS).
- **GTK 4**: Optional Linux-only alternative for native desktop integration. Not required.

This matches the 0x808 approach (SDL2+OpenGL+ImGui for the plugin) and extends it to the standalone app as well, creating one unified GUI codebase.

**Alternatives considered**:
- **GTK 4 for standalone, ImGui for plugin** — the original plan. Rejected because GTK requires Homebrew on macOS (~40MB) and MSYS2 DLLs on Windows (~30MB). Users shouldn't need to install dependencies to run a synth.
- **GTK 4 everywhere** — rejected. GTK doesn't embed cleanly in DAW plugin windows, and requires heavy external dependencies on non-Linux platforms.
- **Cairo+SDL2 (custom rendering)** — considered for visual parity with GTK Cairo drawing. Rejected in favor of ImGui because ImGui is already proven in 0x808, has a rich widget set, and the custom theming capability is sufficient for a professional look.
- **Qt** — rejected (C++, licensing complexity, large dependency).

**Architecture**:
1. SDL2 window with OpenGL 3.3 Core context
2. Dear ImGui (vendored) renders all UI — knobs, sliders, envelope displays, meters, preset browser
3. UI abstraction layer (`oxs_ui_layout_t`) drives ImGui widget creation
4. Custom ImGui widgets via `ImDrawList` for knobs (arc drawing), envelopes (line plots), meters (filled rects)
5. For plugin: SDL2 window reparented into DAW host window, render thread at 60fps
6. For standalone: normal SDL2 window, ImGui event loop

**Rationale**: One codebase, zero friction on all platforms. ImGui is 6 .cpp files (~500KB compiled), SDL2 is a single shared library. The resulting binary is small (~3-8MB) and self-contained. The UI abstraction layer means the same layout tree drives both standalone and plugin GUI — visual parity guaranteed.

### Decision 8: UI abstraction as data-driven layout tree

**Choice**: `oxs_ui_layout_t` is a tree of `oxs_ui_widget_t` descriptors — each node specifies a widget type (knob, slider, dropdown, toggle, label, group, waveform display, envelope curve, level meter, preset browser), a param ID binding, and position/size hints. This is pure data, not rendering code.

A backend interface (`oxs_ui_backend_t`) provides function pointers (`create_knob()`, `create_slider()`, `update_value()`, etc.) that the GTK implementation fills in. The layout tree is walked once at startup to create real widgets.

**Alternatives considered**:
- **Direct GTK code** — build the UI directly with GTK API calls, no abstraction. Rejected because if ImGui is added later, all layout logic (which params are in which section, what widget type each param uses, how sections are arranged) would need to be reimplemented from scratch. The abstraction costs one small C file defining the tree.
- **XML/Glade UI definition** — GTK's built-in UI builder. Rejected because it's GTK-specific (doesn't help ImGui) and doesn't support custom widget types (knobs, waveform displays).

**Rationale**: The layout tree is cheap to build (one-time init, ~150 widget descriptors) and serves as the single source of truth for UI structure. The ImGui backend walks the tree and emits ImGui widget calls. Custom widgets (knobs, meters, envelope curves) use `ImDrawList` for direct rendering. The same code runs in both standalone (normal SDL2 window) and plugin (embedded SDL2 window) — no duplication. GTK backend exists as an optional alternative for Linux users who prefer native look.

### Decision 9: JSON presets via cJSON

**Choice**: Presets are JSON files with a flat structure:
```json
{
  "name": "FM Bell",
  "author": "0xSYNTH",
  "category": "Keys",
  "synth_mode": "fm",
  "params": {
    "fm_algorithm": 0,
    "fm_op0_ratio": 1.0,
    "fm_op0_level": 1.0,
    "fm_op1_ratio": 3.5,
    "fm_op1_level": 0.6,
    ...
  },
  "midi_cc_map": {
    "1": "filter_cutoff",
    "74": "filter_resonance"
  }
}
```

Serialization via cJSON (already vendored from 0x808). Factory presets in `presets/factory/`, user presets in platform-specific directories (`$XDG_DATA_HOME/0xSYNTH/presets/` on Linux, `~/Library/Application Support/0xSYNTH/presets/` on macOS, `%APPDATA%/0xSYNTH/presets/` on Windows).

**Alternatives considered**:
- **Binary format** — pack param values as a flat float array. Smaller (~600 bytes vs ~2-4KB JSON) but not human-readable, not diffable, not scriptable. Rejected because human-readability is a key value proposition — users can share presets on forums, edit them in text editors, generate them with scripts or AI agents.
- **TOML or YAML** — arguably more human-friendly syntax than JSON. Rejected because cJSON is already vendored and proven in the 0x808 codebase, JSON is universally understood and supported by every programming language, and the preset structure is simple enough that JSON's verbosity is not a problem.
- **0x808's hardcoded C structs** — the current approach in 0x808 where presets are compiled-in `sq_synth_preset_t` initializers. Rejected because users cannot create, share, or modify presets without recompiling.

**Rationale**: JSON presets unlock the entire preset sharing and generation workflow that hardcoded C structs prevent. A preset file maps param names to values — the same param names used in the registry. Loading a preset is: parse JSON, look up each param by name, call `oxs_param_set()`. Saving is the reverse. The CLAP state save/restore callbacks serialize to the same JSON format, so DAW project recall and standalone preset files are interchangeable.

### Decision 10: Pre-allocated everything, zero allocations in audio path

**Choice**: All buffers are allocated in `oxs_synth_create()`:
- Voice state array (16 voices, each with oscillator phases, envelope state, filter state, FM operator state, wavetable state)
- Effect chain buffers (delay lines up to 2s at 48kHz = 96K samples, reverb comb/allpass buffers = 8 combs + 4 allpass per stereo channel)
- Wavetable bank storage (8 banks x 64 frames x 2048 samples)
- Sample slots (pre-allocated slot array, sample data loaded on demand via command queue)
- Interleave/deinterleave conversion buffers
- Output event queue buffer

The audio thread (`oxs_synth_process()`) never calls `malloc`, `free`, `realloc`, `fopen`, or any function that may block.

**Alternatives considered**:
- **Allocate on demand** — allocate delay buffers when a delay effect is activated, allocate voice state when a note is triggered. Rejected because `malloc` can block (waiting for a mutex inside the allocator), and the allocation time is unpredictable. A single `malloc` stall causes an audio dropout.
- **Pool allocator in the audio thread** — pre-allocate a memory pool, sub-allocate from it in the audio path. Rejected as unnecessary complexity — the total memory footprint is predictable and modest (a few MB), so we can allocate everything upfront.

**Rationale**: 0x808 already follows this pattern — `sq_engine_init()` pre-allocates everything. This decision carries it forward. The total memory footprint for 0xSYNTH is estimated at:
- Voice state: 16 voices x ~2KB = ~32KB
- Delay buffers: 3 slots x 96K samples x 4 bytes = ~1.1MB
- Reverb buffers: ~200KB
- Wavetable banks: 8 x 64 x 2048 x 4 = ~4MB
- Total: ~5-6MB, allocated once at init

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                       Consumers                              │
│  ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌────────────┐ │
│  │ GTK GUI  │  │ CLAP Host│  │ VST3 Host │  │ Standalone │ │
│  │ (gtk4)   │  │ (DAW)    │  │ (DAW)     │  │ (miniaudio)│ │
│  └────┬─────┘  └────┬─────┘  └─────┬─────┘  └─────┬──────┘ │
│       │              │              │               │        │
│  ═════╪══════════════╪══════════════╪═══════════════╪══════  │
│                    synth_api.h                               │
│            (opaque handle, public functions)                  │
│  ════════════════════════════════════════════════════════════ │
│       │                    │                    │             │
│  ┌────┴──────┐    ┌───────┴────────┐   ┌───────┴──────────┐ │
│  │  Atomic   │    │ Command Queue  │   │ Output Event     │ │
│  │  Params   │    │ (GUI → Audio)  │   │ Queue            │ │
│  │           │    │                │   │ (Audio → GUI)    │ │
│  │ _Atomic   │    │ note on/off    │   │                  │ │
│  │ float[]   │    │ preset load    │   │ peak levels L/R  │ │
│  │           │    │ synth mode     │   │ voice bitmap     │ │
│  │ relaxed   │    │ panic          │   │ envelope stages  │ │
│  │ ordering  │    │ sample load    │   │                  │ │
│  └────┬──────┘    └───────┬────────┘   └───────┬──────────┘ │
│       │                   │                     │            │
│  ═════╪═══════════════════╪═════════════════════╪══════════  │
│               Engine Internals (private)                     │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ oxs_synth_process(handle, *output_L, *output_R, N)  │   │
│  │                                                      │   │
│  │  1. Drain command queue → apply note/preset/mode     │   │
│  │  2. Snapshot atomic params → oxs_param_snapshot_t    │   │
│  │  3. For each active voice:                           │   │
│  │     a. Compute oscillator output (sub/FM/WT/sampler) │   │
│  │     b. Apply per-voice filter                        │   │
│  │     c. Apply amplitude envelope × velocity           │   │
│  │     d. Apply LFO modulation                          │   │
│  │     e. Pan and accumulate to stereo mix buffer       │   │
│  │  4. Apply effect chain (3 slots in series)           │   │
│  │  5. Apply master volume                              │   │
│  │  6. Compute peaks, push output event                 │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐ │
│  │ Oscillators │ │ Filters  │ │Envelopes │ │  Effects    │ │
│  │             │ │          │ │          │ │             │ │
│  │ Saw         │ │ Biquad   │ │ ADSR     │ │ Filter      │ │
│  │ Square      │ │ LP/HP/BP │ │ (rate-   │ │ Delay       │ │
│  │ Triangle    │ │          │ │  based   │ │ Reverb      │ │
│  │ Sine        │ │ TDF-II   │ │  state   │ │ Overdrive   │ │
│  │             │ │ 3ms      │ │  machine)│ │ Fuzz        │ │
│  │ FM (4-op,   │ │ cutoff   │ │          │ │ Chorus      │ │
│  │  8 algo)    │ │ smoothing│ │ LFO      │ │ Bitcrusher  │ │
│  │             │ │          │ │ (sin/tri/│ │ Compressor  │ │
│  │ Wavetable   │ │ State    │ │  sq/saw) │ │ Phaser      │ │
│  │ (bank scan, │ │ clamped  │ │          │ │ Flanger     │ │
│  │  position   │ │ to ±4    │ │          │ │ Tremolo     │ │
│  │  morphing)  │ │          │ │          │ │ Ring Mod    │ │
│  │             │ │          │ │          │ │ Tape        │ │
│  │ Sampler     │ │          │ │          │ │ Shimmer     │ │
│  │ (Hermite    │ │          │ │          │ │             │ │
│  │  interp)    │ │          │ │          │ │ 3 slots,    │ │
│  │             │ │          │ │          │ │ master bus, │ │
│  │ SF2 (TSF)   │ │          │ │          │ │ in series   │ │
│  └─────────────┘ └──────────┘ └──────────┘ └─────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

**Data flow summary:**
- GUI thread writes: `oxs_param_set()` → atomic store, `oxs_synth_note_on()` → command queue push
- Audio thread reads: atomic load (snapshot) + command queue drain → DSP → output event push
- GUI thread reads: `oxs_synth_pop_output_event()` → peaks, voice activity, envelope stages

**Key difference from 0x808**: In 0x808, `plugin.c:apply_param_to_engine()` directly writes to `engine->synth_presets[i].filter_cutoff` and `engine->transport.bpm` — cross-thread mutation of mutable state with no synchronization. In 0xSYNTH, all cross-thread communication goes through exactly three channels: atomic params (continuous), command queue (discrete GUI→audio), output event queue (audio→GUI). No exceptions.

## Risks / Trade-offs

### Risks

1. **GTK 4 on Windows/macOS is second-class.** GTK 4 works via Homebrew (macOS) and MSYS2/vcpkg (Windows), but packaging is heavier than platform-native toolkits, and rendering quality may differ from Linux. **Mitigation**: Test early on all platforms (Phase 11 CI). The UI abstraction layer (Decision 8) means we can swap in a platform-native backend if GTK proves unworkable on a specific platform.

2. **Atomic param snapshot may miss rapid automation changes.** At 44.1kHz with 256-sample buffers, the snapshot runs ~172 times/sec. If a DAW automates a parameter at audio rate (per-sample changes), the snapshot captures only one value per buffer. **Mitigation**: For the v0.1.0 release, per-buffer granularity is sufficient — it matches how most synth plugins handle parameter changes. CLAP's per-sample parameter events could be supported in a future version by processing parameter events within the sample loop, at the cost of more complex DSP code.

3. **120-150 parameters may overwhelm some DAW UIs.** Hosts display all automatable parameters in lists. **Mitigation**: Use CLAP parameter groups to organize into logical categories (Oscillator, Filter, Amp Env, Filter Env, LFO, FM Op 0-3, Wavetable, Effect 1-3, Master). Most DAWs handle large param counts well (Vital exposes 300+, Pigments 500+).

4. **Porting 2000+ lines of DSP from 0x808 may introduce regressions.** Refactoring the synth to read from a param snapshot instead of struct fields changes every DSP function's interface. **Mitigation**: Golden-master testing — render each of the 59 factory presets through the 0x808 engine with a known MIDI sequence, save the output buffers, then verify 0xSYNTH produces bit-identical (or within floating-point epsilon) output for the same inputs. This catches regressions during porting.

5. **CPLUG may not support all CLAP features we need.** CPLUG is a thin abstraction and may lag behind CLAP spec additions. **Mitigation**: CPLUG covers the features we need for v0.1.0 (params, MIDI note events, state save/restore, GUI embedding). CPLUG is small (~2K lines) and MIT-licensed, so we can patch it if needed. If CPLUG becomes abandoned, the 0xSYNTH CPLUG integration layer is thin enough to replace with raw CLAP calls.

### Trade-offs

6. **Single-threaded engine.** All DSP (voice rendering, mixing, effects) runs in the audio callback thread. No internal parallelism. This is simpler and avoids intra-engine synchronization, but limits CPU scaling. With 16 voices + 3 effects, profiling on 0x808 shows ~3-5% CPU at 44.1kHz/256 buffers on a modern x86 core — well within budget. If voice count or effect complexity grows, per-voice rendering could be parallelized later without API changes (the opaque handle hides the internal threading model).

7. **JSON presets are larger than binary.** A full preset with ~150 params is ~2-4KB as JSON vs ~600 bytes as a packed binary format. At 59 factory presets, the factory bank is ~120-240KB of JSON — trivial. The human-readability, scriptability, and AI-agent-editability of JSON are worth the size overhead.

8. **ImGui is C++.** Dear ImGui is written in C++, which breaks the "pure C" project constraint for the GUI layer. However: the engine, API, and all DSP code remain pure C. Only the ImGui backend (.cpp files) and ImGui itself are C++. The build system handles this cleanly — engine is C99, GUI is C++17. This is the same approach 0x808 uses successfully.

9. **No per-voice effects.** The effect chain processes the master bus only (post-mix). Per-voice effects (e.g., per-note reverb sends) would enable richer sound design but significantly complicate the architecture (effect state per voice, voice-level wet/dry routing). Deferred to post-v0.1.0 — master bus processing matches what 0x808 does today and is sufficient for release.

## Open Questions

1. **Wavetable format compatibility.** Should 0xSYNTH support only its own `.wav` wavetable banks (carried over from 0x808), or also third-party formats (Serum `.wav` wavetables, `.wt` files)? **Recommendation**: Start with our own format. Add third-party format support based on user demand — the wavetable loading code is isolated enough that new formats can be added without architectural changes.

2. **Plugin GUI embedding.** ~~Should the plugin start parameter-only?~~ **RESOLVED**: Plugin ships with a custom GUI using SDL2 + OpenGL + Cairo (not GTK). Same Cairo drawing code as the GTK standalone, embedded inside the DAW via CPLUG's native handle mechanism (SetParent on Windows, proven in 0x808). This keeps the plugin binary small (~3-8MB, no GTK dependency) while maintaining visual parity with the standalone.

3. **Polyphony configuration.** Currently fixed at 16 voices with oldest-voice-stealing (matching 0x808's `SQ_MAX_SYNTH_VOICES`). Should polyphony count and steal mode (oldest, quietest, lowest, highest) be exposed as parameters? **Recommendation**: Yes — low implementation cost (voice allocator already exists, just parameterize the count and steal strategy), and it gives users useful control. Mono mode (1 voice) with legato/retrigger would be valuable for bass and lead patches.

4. **Effect routing topology.** Currently master bus only with 3 effect slots in series. Per-voice effects and parallel routing (e.g., dry/wet splits, send/return) would add sound design flexibility. **Recommendation**: Defer to post-v0.1.0. Master bus with 3 serial slots matches 0x808's current architecture, is simple to implement and understand, and covers the most common use cases. The opaque API hides the routing topology, so it can be changed later without breaking consumers.

5. **Sample rate change handling.** When a CLAP host changes the sample rate, all filter coefficients, delay buffer sizes, and envelope rates need recalculation. Should this trigger a full engine destroy/recreate, or support in-place reconfiguration? **Recommendation**: Destroy and recreate. CPLUG guarantees `setSampleRateAndBlockSize` is called outside the audio processing context (no concurrent `process()` calls). 0x808 already uses this approach (`sq_engine_shutdown()` + `sq_engine_init()` in the CPLUG callback). The cost is one allocation burst at sample rate change — this happens rarely (typically once at plugin load).
