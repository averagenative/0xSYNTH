## Why

The 0x808 project bundles a capable synthesis engine (subtractive, FM, wavetable) tightly with a step sequencer, sampler, and transport system. Frontends (GTK, ImGui) directly mutate engine structs from the GUI thread — creating thread-safety risks and making the synth impossible to reuse as a standalone instrument. Extracting the synth into its own project with a clean API boundary enables it to ship as a proper VST3/CLAP plugin and standalone application, with correct real-time threading, MIDI CC automation, and host parameter integration.

## What Changes

- **Extract synthesis DSP** from 0x808 (oscillators, filters, envelopes, LFOs, FM synthesis, wavetable synthesis, 15 effect types) into a self-contained engine library with zero external dependencies beyond libm
- **New public C API** (`synth_api.h`) — the sole interface for all consumers. Hybrid architecture: lock-free command queue for discrete events (note on/off, preset load) + atomic parameter store for continuous knobs (filter cutoff, envelope times, LFO rate). No direct struct access from outside the engine
- **Atomic parameter readback** for GUI state display + lightweight output event queue (audio→GUI) for transient data (peak meters, voice activity, envelope positions)
- **Parameter registry** with integer IDs, metadata (name, range, default, units, groups), and MIDI CC mapping — designed to map directly to CLAP's parameter model
- **CLAP as first-class plugin target**, VST3 as compatibility layer, both via CPLUG
- **Standalone application** with miniaudio backend + MIDI input, targeting Linux/macOS/Windows
- **GTK 4 frontend** as the single UI toolkit (cross-platform), with a shared layout/widget abstraction layer so ImGui can be added later without architectural changes
- **Sampler voice engine** carried over from 0x808 (Hermite interpolation, pitch shifting) + SoundFont (SF2) support
- **JSON preset system** — factory presets (ported from 0x808's 59 built-in presets) + user preset directories
- **MIDI CC learn and automation mapping** integrated into the parameter system from day one
- **BREAKING**: This is a new project, not a modification of 0x808. No backward compatibility constraints.

## Capabilities

### New Capabilities
- `synth-engine`: Core DSP library — oscillators (saw/square/tri/sine), biquad filters (LP/HP/BP), ADSR envelopes, LFO, unison/detune, polyphonic voice management (16 voices, voice stealing). Pure C99, real-time safe, no allocations in audio path
- `fm-synthesis`: 4-operator FM synthesis with 8 algorithms, per-operator envelopes, self-feedback. Ported from 0x808
- `wavetable-synthesis`: Wavetable oscillator with bank loading, position morphing, envelope/LFO modulation of table position
- `effects-chain`: 15 effect types (filter, delay, reverb, overdrive, fuzz, chorus, bitcrusher, compressor, phaser, flanger, tremolo, ring mod, tape, shimmer) with per-voice and master bus routing
- `sampler-engine`: Sample playback with Hermite interpolation, pitch shifting, velocity sensitivity. SoundFont (SF2) instrument loading
- `synth-api`: Public C API — `synth_init()`, `synth_process()`, `synth_set_param()`, `synth_note_on/off()`, `synth_get_state()`. Lock-free command queue + atomic parameter store. Thread-safe by design
- `parameter-system`: Flat integer-ID parameter registry with metadata (name, range, default, units, group). Maps to CLAP params. Supports MIDI CC learn/mapping and host automation
- `preset-management`: JSON-based preset serialization. Factory preset bank (59+ presets). User preset directories with save/load/browse
- `plugin-clap`: CLAP plugin wrapper via CPLUG — parameter bridging, MIDI input, audio processing, GUI embedding. First-class target
- `plugin-vst3`: VST3 plugin wrapper via CPLUG — compatibility layer using same engine API
- `standalone-app`: Standalone executable with miniaudio audio backend, platform MIDI input, GTK 4 UI
- `gtk-frontend`: GTK 4 synthesizer UI — knob/slider controls, oscillator visualization, envelope curves, effect chain editor, preset browser, peak meters. Cross-platform (Linux/macOS/Windows)
- `ui-abstraction`: Shared layout/widget abstraction that the GTK frontend implements, designed so a future ImGui backend can achieve visual parity without duplicating layout logic

### Modified Capabilities
<!-- None — this is a new project extracted from 0x808 -->

## Business Value Analysis

### Who Benefits and How

**1. Music Producers Using DAWs (Primary)**
The working musician who loads plugins in Bitwig, REAPER, Ardour, or other CLAP/VST3 hosts. Today 0x808's synth is trapped inside a drum machine — they can't use it as an instrument in a mix. 0xSYNTH gives them a multi-engine synth (subtractive, FM, wavetable) that slots into their existing workflow, with full host automation and MIDI CC control. The JSON preset format means presets are portable and human-readable — shareable on forums, versionable in git, editable by scripts or AI agents.

**2. Linux Audio Users (Underserved Market)**
Linux musicians are chronically underserved by commercial synth developers. Most VST/CLAP plugins are Windows/macOS only. 0xSYNTH is built Linux-first with GTK 4 and targets all three platforms equally. This audience is loyal, vocal, and will contribute bug reports, presets, and patches if the tool respects their ecosystem (native toolkit, open formats, no DRM).

**3. Sound Designers and Preset Creators**
The JSON preset system with clearly defined parameter IDs lowers the barrier to programmatic sound design. A sound designer can write scripts to generate preset variations, batch-modify parameters, or build randomized preset packs. AI agents can also generate and iterate on presets through the same JSON interface — opening up generative sound design workflows.

**4. Developers and Tinkerers**
The clean C API (`synth_api.h`) makes the engine embeddable. Someone building a game, interactive installation, or custom sequencer can link the engine library directly without pulling in UI or plugin framework code. The API-first architecture makes this a building block, not just a product.

### What Problem It Solves

The core problem is **locked value**. The 0x808 synth engine is capable — 3 synthesis modes, 15 effects, 59 presets — but it's inaccessible outside the drum machine context. You can't load it in a DAW. You can't automate its parameters from a host. You can't use it without the sequencer. The direct struct coupling means you can't even safely run the GUI and audio on separate threads without risking data races.

This isn't a technical curiosity — it's the difference between a personal project and a distributable instrument. Without the extraction, the synth work done in 0x808 benefits exactly one application. With it, the same DSP code serves DAW users, standalone users, embedded developers, and preset creators across three operating systems.

### Priority (Ranked by Value Delivered)

| Priority | Capability | Value Rationale |
|----------|-----------|-----------------|
| **P0 — Must Have** | `synth-api`, `parameter-system` | Everything else depends on these. No API = no plugin, no standalone, no safe threading. This is the architectural foundation. |
| **P0 — Must Have** | `synth-engine`, `fm-synthesis`, `wavetable-synthesis` | The actual sound. Without synthesis, there's no product. Porting from 0x808 means this is proven code, not greenfield risk. |
| **P0 — Must Have** | `effects-chain` | Effects are table stakes for any synth. Producers expect reverb, delay, and filters at minimum. 15 types already exist in 0x808. |
| **P1 — High Value** | `plugin-clap`, `plugin-vst3` | This is how 90%+ of users will consume the synth. CLAP-first with VST3 compat covers the DAW market. Without plugins, the audience is limited to standalone users. |
| **P1 — High Value** | `preset-management` | A synth without presets is a synth nobody uses. Factory presets demonstrate capability; user presets create stickiness. JSON format enables the AI/scripting workflow. |
| **P1 — High Value** | `standalone-app`, `gtk-frontend` | Required for users without a DAW and for development/testing. Also the primary way Linux users will interact with the synth. |
| **P2 — Important** | `sampler-engine` | Adds versatility (hybrid synth + sampler), but the core value proposition is synthesis. Can ship without this initially. |
| **P2 — Important** | `ui-abstraction` | Insurance policy for future ImGui support. Low cost to design now, high cost to retrofit later. |
| **P3 — Nice to Have** | MIDI CC learn | Power-user feature. Important for live performance but not blocking for initial release. Should be designed into the parameter system now even if the UI comes later. |

### What Happens If We Don't Build This

- **The synth stays buried in 0x808.** It can only be used as part of the drum machine. No DAW integration, no standalone use, no reuse in other projects.
- **The threading bugs stay.** Direct struct mutation from the GUI thread is a latent source of glitches, crashes, and undefined behavior. The longer this goes unfixed, the more code is built on the broken foundation.
- **No distribution path.** Without CLAP/VST3 plugin packaging, there's no way to put this in front of users who discover instruments through their DAW's plugin scanner. The audience stays at zero.
- **Preset work is wasted.** The 59 presets in 0x808 are hardcoded C structs. Without a JSON preset system and a standalone synth to load them in, they can't be shared, modified, or expanded by users or agents.
- **Future features compound the coupling.** Every new feature added to 0x808's synth (new oscillator types, more effects, MPE support) deepens the entanglement with the sequencer, making extraction harder over time. The cost of extraction only goes up.

### Success Metrics

| Metric | Target | How to Measure |
|--------|--------|----------------|
| **Audio stability** | Zero xruns at 256-sample buffer (5.8ms @ 44.1kHz) with 16 voices + effects | Automated render test: process 60s of polyphonic audio, assert no NaN/Inf, measure wall-clock vs audio-clock ratio |
| **Thread safety** | Zero data races under concurrent GUI + audio operation | ThreadSanitizer CI pass on all integration tests; no direct struct access outside `synth_api.h` (enforced by header visibility) |
| **Plugin loads in hosts** | CLAP scans and instantiates in Bitwig, REAPER, Ardour; VST3 scans in REAPER | Manual smoke test on each target host per release |
| **Preset round-trip** | Load preset JSON → render audio → save preset → reload → render identical audio | Automated test comparing output buffers (bit-exact or within floating-point epsilon) |
| **Cross-platform builds** | CI green on Linux (GCC + Clang), macOS (Clang), Windows (MSVC) | GitHub Actions or equivalent CI matrix |
| **Parameter completeness** | 100% of synth parameters exposed via API, mappable to MIDI CC, automatable in DAW | Automated test enumerating parameter registry vs known parameter list |
| **Factory presets** | 59+ presets ported from 0x808, all producing audible output | Automated test: load each preset, trigger note, assert non-silent output |
| **Standalone usability** | Launch → select preset → play notes via MIDI or virtual keyboard → hear audio | Manual acceptance test on each platform |

## Impact

- **New repository**: `0xSYNTH/` — standalone project, no dependency on 0x808
- **Source from 0x808**: DSP code ported from `~/projects/0x808/src/engine/` (synth.c, effects.c, envelope.c, sampler.c, mixer.c) — refactored to remove sequencer/transport/pattern dependencies
- **Build system**: CMake, targeting Linux (GCC/Clang), macOS (Clang), Windows (MSVC/MinGW)
- **Dependencies**: libm (engine), GTK 4 (standalone UI), SDL2 + OpenGL + Cairo (plugin GUI), miniaudio (standalone audio), CPLUG (plugin wrappers), cJSON (preset I/O), dr_libs (sample loading), TinySoundFont (SF2)
- **Language**: Pure C (C99/C11 for atomics) throughout — engine, API, frontend, plugin wrappers
- **Threading model**: Audio thread owns engine state exclusively. GUI communicates only via atomic params + command queue. No mutexes in the audio path
