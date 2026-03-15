## ADDED Requirements

### Requirement: VST3 plugin packaging
The system SHALL build as a VST3 plugin (.vst3 shared library) via CPLUG, sharing the same plugin implementation as the CLAP build. The plugin SHALL be scannable by VST3-compliant hosts.

#### Scenario: Plugin scans in REAPER
- **WHEN** 0xSYNTH.vst3 is placed in the VST3 plugin directory
- **THEN** REAPER detects and lists it as an available instrument

#### Scenario: Same parameters as CLAP
- **WHEN** VST3 plugin is loaded
- **THEN** all parameters match the CLAP version in name, range, and behavior

### Requirement: VST3 state compatibility
The system SHALL save and restore VST3 plugin state using the same JSON format as CLAP.

#### Scenario: VST3 project save/reload
- **WHEN** a VST3 host project is saved and reopened
- **THEN** all synth parameters are restored correctly

### Requirement: Shared implementation
The CLAP and VST3 plugins SHALL share the same `plugin.c` source file. CPLUG SHALL handle all format-specific differences.

#### Scenario: Single source for both formats
- **WHEN** a bug is fixed in plugin.c
- **THEN** both CLAP and VST3 builds reflect the fix without additional changes
