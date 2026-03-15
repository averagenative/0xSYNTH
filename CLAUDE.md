# 0xSYNTH — Claude Code Instructions

## Project Overview

0xSYNTH is a standalone synthesizer extracted from the 0x808 drum machine.
Pure C (C99/C11 for atomics), CMake build, targets Linux/macOS/Windows.
Delivers as CLAP plugin (first-class), VST3 (compat), and standalone app with GTK 4 UI.

## Architecture

- **`src/engine/`** — DSP engine. Real-time safe. Zero allocations in audio path. Links only to libm.
- **`src/api/synth_api.h`** — THE public API. Opaque `oxs_synth_t*` handle. All consumers use this exclusively.
- **`src/plugin/`** — CLAP/VST3 wrappers via CPLUG. Calls `synth_api.h` only.
- **`src/standalone/`** — miniaudio + MIDI backend. Calls `synth_api.h` only.
- **`src/gui_gtk/`** — GTK 4 frontend. Calls `synth_api.h` only.
- **`src/ui/`** — Toolkit-agnostic UI layout abstraction. Data only, no rendering.
- **`tests/`** — Test executables linked against `oxs_api`.

## Threading Model

- **Atomic params** (`_Atomic float`) for continuous knob values — GUI writes, audio reads
- **Command queue** (lock-free SPSC) for discrete events — note on/off, preset load, panic
- **Output event queue** (lock-free SPSC) for GUI readback — peaks, voice activity
- **Param snapshot** copied at start of each `process()` call — consistent state within buffer

## Rules

1. **API boundary is sacred.** Code outside `src/engine/` and `src/api/` MUST NOT include engine headers except `synth_api.h` and `types.h`. The pre-commit hook enforces this.
2. **No allocations in audio path.** `oxs_synth_process()` and everything it calls must never call `malloc`, `free`, `fopen`, or any blocking function.
3. **All parameters go through the registry.** New synth parameters need an `oxs_param_id` enum entry, metadata in `oxs_param_registry_init()`, and atomic access via `oxs_param_set/get`.
4. **Tests are non-negotiable.** Every new DSP module needs a test in `tests/`. Run `ctest` before committing.
5. **Read before writing.** Always read existing code before modifying. Understand the pattern before extending it.

## Build

```bash
mkdir -p build && cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure
```

## Task System

Task files in `tasks/` coordinate work for autonomous agents:
- `tasks/engine_tasks.json` — DSP engine work
- `tasks/plugin_tasks.json` — Plugin/standalone/preset work
- `tasks/frontend_tasks.json` — GTK UI work
- `tasks/infra_tasks.json` — CI/platform work
- `tasks/task_ops.sh` — CLI for claiming/completing tasks

OpenSpec change: `openspec/changes/synth-engine-extraction/tasks.md` is the canonical task list.
Task JSON files are work queues derived from it.

## Autonomous Worker Loop

1. Check `tasks/task_ops.sh next` for the next pending task
2. Claim it with `tasks/task_ops.sh claim <id>`
3. Read the relevant openspec spec in `openspec/changes/synth-engine-extraction/specs/`
4. Implement the code
5. Write or update tests
6. Run `cd build && make && ctest --output-on-failure`
7. If tests pass, mark complete with `tasks/task_ops.sh complete <id>`
8. Update `openspec/changes/synth-engine-extraction/tasks.md` checkbox
9. Commit (pre-commit hook runs build + tests + API boundary check)
10. Move to next task

## Reference

- 0x808 source (for porting): `~/projects/0x808/src/engine/`
- OpenSpec proposal: `openspec/changes/synth-engine-extraction/proposal.md`
- Design doc: `openspec/changes/synth-engine-extraction/design.md`
- Specs: `openspec/changes/synth-engine-extraction/specs/*/spec.md`
