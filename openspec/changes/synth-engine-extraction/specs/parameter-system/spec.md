## ADDED Requirements

### Requirement: Flat integer parameter IDs
The system SHALL assign each synth parameter a unique integer ID from the `oxs_param_id` enum. IDs SHALL be grouped by category with reserved ranges for future expansion.

#### Scenario: Every parameter has a unique ID
- **WHEN** the parameter registry is initialized
- **THEN** no two parameters share the same integer ID

#### Scenario: Parameter IDs are stable across versions
- **WHEN** a new version adds parameters
- **THEN** existing parameter IDs retain their values (new params use reserved ranges)

### Requirement: Atomic parameter access
The system SHALL provide `oxs_param_set(handle, id, value)` and `oxs_param_get(handle, id)` for lock-free parameter access using `_Atomic float` storage. These functions SHALL be safe to call from any thread.

#### Scenario: Set and get round-trip
- **WHEN** `oxs_param_set(handle, OXS_PARAM_FILTER_CUTOFF, 2000.0f)` is called
- **THEN** `oxs_param_get(handle, OXS_PARAM_FILTER_CUTOFF)` returns 2000.0f

#### Scenario: Values are clamped to valid range
- **WHEN** `oxs_param_set()` is called with a value outside the parameter's min/max range
- **THEN** the value is clamped to the valid range before storage

#### Scenario: Concurrent access from multiple threads
- **WHEN** GUI thread writes params while audio thread reads via snapshot
- **THEN** no data races occur (verified by ThreadSanitizer)

### Requirement: Parameter metadata registry
The system SHALL maintain a registry of `oxs_param_info_t` records providing: id, name, group, min, max, default, step, units, and flags (automatable, modulatable) for every parameter.

#### Scenario: Query parameter info by ID
- **WHEN** `oxs_param_get_info(handle, OXS_PARAM_FILTER_CUTOFF, &info)` is called
- **THEN** info contains name="Filter Cutoff", group="Filter", min=20.0, max=20000.0, default=1000.0, units="Hz"

#### Scenario: Query parameter count
- **WHEN** `oxs_param_count(handle)` is called
- **THEN** it returns the total number of registered parameters

#### Scenario: Lookup parameter by name
- **WHEN** `oxs_param_id_by_name(handle, "Filter Cutoff")` is called
- **THEN** it returns `OXS_PARAM_FILTER_CUTOFF`

### Requirement: MIDI CC mapping
The system SHALL maintain a mapping table of 128 MIDI CC numbers to parameter IDs. Each CC number maps to at most one parameter.

#### Scenario: Assign CC to parameter
- **WHEN** `oxs_midi_cc_assign(handle, 74, OXS_PARAM_FILTER_CUTOFF)` is called
- **THEN** incoming MIDI CC 74 messages will control filter cutoff

#### Scenario: Unassign CC
- **WHEN** `oxs_midi_cc_unassign(handle, 74)` is called
- **THEN** MIDI CC 74 no longer controls any parameter

#### Scenario: CC value scaling
- **WHEN** a MIDI CC message with value 64 (of 0-127) is received for a mapped parameter
- **THEN** the parameter is set to the midpoint of its min/max range
