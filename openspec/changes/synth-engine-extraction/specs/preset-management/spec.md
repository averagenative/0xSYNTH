## ADDED Requirements

### Requirement: JSON preset format
The system SHALL serialize presets as JSON files containing: name, author, category, synth_mode, and a flat params object mapping parameter names to float values. An optional midi_cc_map object SHALL store CC-to-parameter assignments.

#### Scenario: Preset file is human-readable
- **WHEN** a preset is saved to disk
- **THEN** the resulting file is valid JSON that can be opened and edited in any text editor

#### Scenario: Preset schema includes all parameters
- **WHEN** a preset is saved
- **THEN** the params object contains entries for every registered parameter

### Requirement: Preset save
The system SHALL provide `oxs_preset_save(handle, path)` that snapshots all current parameter values and writes them to a JSON file.

#### Scenario: Save captures current state
- **WHEN** user modifies filter cutoff to 5000Hz and calls save
- **THEN** saved JSON contains "Filter Cutoff": 5000.0

#### Scenario: Save creates parent directories
- **WHEN** save path includes non-existent directories
- **THEN** directories are created before writing

### Requirement: Preset load
The system SHALL provide `oxs_preset_load(handle, path)` that reads a JSON preset and applies all parameter values. Invalid or out-of-range values SHALL be clamped.

#### Scenario: Load restores full state
- **WHEN** a previously saved preset is loaded
- **THEN** all parameters match the saved values

#### Scenario: Load clamps invalid values
- **WHEN** a preset JSON contains "Filter Cutoff": 99999.0 (above max of 20000)
- **THEN** the value is clamped to 20000.0

#### Scenario: Malformed JSON is rejected gracefully
- **WHEN** `oxs_preset_load()` is given a file with invalid JSON
- **THEN** function returns an error code and current parameters are unchanged

### Requirement: Preset listing
The system SHALL provide `oxs_preset_list(directory, *names, *count)` that scans a directory for .json preset files and returns a sorted list of names.

#### Scenario: List factory presets
- **WHEN** `oxs_preset_list()` is called on the factory preset directory
- **THEN** it returns 59+ preset names sorted alphabetically

### Requirement: Factory presets
The system SHALL ship with 59+ factory presets covering subtractive, FM, and wavetable synthesis modes, ported from 0x808.

#### Scenario: All factory presets produce audio
- **WHEN** each factory preset is loaded and a note is triggered
- **THEN** rendered output is non-silent

#### Scenario: Factory presets span all synth modes
- **WHEN** factory presets are categorized by synth_mode
- **THEN** there are presets for SUBTRACTIVE, FM, and WAVETABLE modes

### Requirement: User preset directories
The system SHALL use platform-specific directories for user presets: `$XDG_DATA_HOME/0xSYNTH/presets/` (Linux), `~/Library/Application Support/0xSYNTH/presets/` (macOS), `%APPDATA%/0xSYNTH/presets/` (Windows). Directories SHALL be created on first save if missing.

#### Scenario: User saves to default location
- **WHEN** user saves a preset without specifying a full path
- **THEN** preset is written to the platform-appropriate user directory

#### Scenario: Directory created on first save
- **WHEN** user preset directory does not exist and a save is requested
- **THEN** directory is created and preset is saved successfully

### Requirement: Preset round-trip fidelity
The system SHALL guarantee that saving and reloading a preset produces identical audio output (within floating-point epsilon).

#### Scenario: Save-load-render matches original
- **WHEN** a preset is saved, reloaded, and audio is rendered
- **THEN** output matches the pre-save rendering within ±1e-6 per sample
