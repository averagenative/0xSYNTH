## ADDED Requirements

### Requirement: CLAP plugin packaging
The system SHALL build as a CLAP plugin (.clap shared library) via CPLUG. The plugin SHALL be scannable by any CLAP-compliant host.

#### Scenario: Plugin scans in Bitwig
- **WHEN** 0xSYNTH.clap is placed in the CLAP plugin directory
- **THEN** Bitwig detects and lists it as an available instrument

#### Scenario: Plugin scans in REAPER
- **WHEN** 0xSYNTH.clap is placed in the CLAP plugin directory
- **THEN** REAPER detects and lists it as an available instrument

### Requirement: Full parameter exposure
The system SHALL expose all synth parameters to the CLAP host via CPLUG parameter callbacks, mapped from the `oxs_param_info_t` registry. Parameters SHALL be organized into CLAP parameter groups.

#### Scenario: Host shows all parameters
- **WHEN** plugin is loaded in a CLAP host
- **THEN** all registered parameters are visible with correct names, ranges, and grouping

#### Scenario: Host automation works
- **WHEN** a parameter is automated in the DAW
- **THEN** parameter values change in real-time via `oxs_param_set()`

### Requirement: MIDI note handling
The system SHALL process MIDI note-on and note-off events from the CLAP host, dispatching them to `oxs_synth_note_on()` and `oxs_synth_note_off()`.

#### Scenario: MIDI keyboard triggers notes
- **WHEN** MIDI note-on events arrive from the host
- **THEN** corresponding voices are triggered in the synth engine

#### Scenario: MIDI note-off releases notes
- **WHEN** MIDI note-off events arrive
- **THEN** corresponding voices enter release phase

### Requirement: State persistence
The system SHALL save and restore plugin state via CPLUG state callbacks, serializing all parameter values as JSON. State SHALL survive DAW project save/reload.

#### Scenario: Save and reload DAW project
- **WHEN** DAW project is saved with 0xSYNTH configured and later reopened
- **THEN** all synth parameters are restored to their saved values

#### Scenario: State format is JSON
- **WHEN** plugin state is serialized
- **THEN** format is JSON compatible with the preset system (reuses `oxs_preset_save/load` logic)

### Requirement: Audio processing bridge
The system SHALL convert between the host's non-interleaved audio format and the engine's interleaved stereo format within `cplug_process()`.

#### Scenario: Audio output reaches host
- **WHEN** notes are playing and host requests audio
- **THEN** synthesized audio appears in the host's mixer at correct levels
