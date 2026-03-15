## ADDED Requirements

### Requirement: Opaque synth handle
The system SHALL expose an opaque handle type `oxs_synth_t*` as the sole interface to the engine. Internal struct definitions SHALL NOT be accessible from public headers.

#### Scenario: Create and destroy synth instance
- **WHEN** consumer calls `oxs_synth_create(sample_rate)`
- **THEN** system returns a non-NULL `oxs_synth_t*` handle with all internal state initialized and all parameters set to defaults

#### Scenario: Double destroy is safe
- **WHEN** consumer calls `oxs_synth_destroy(handle)` twice
- **THEN** second call is a no-op (handle is nulled after first destroy)

### Requirement: Audio processing function
The system SHALL provide `oxs_synth_process(handle, output, num_frames)` that renders stereo interleaved float audio into the provided buffer. This function SHALL be real-time safe: no memory allocation, no file I/O, no locks, no syscalls.

#### Scenario: Process silent buffer with no active notes
- **WHEN** `oxs_synth_process()` is called with no notes triggered
- **THEN** output buffer contains silence (all zeros)

#### Scenario: Process buffer with active notes
- **WHEN** a note has been triggered via `oxs_synth_note_on()` and `oxs_synth_process()` is called
- **THEN** output buffer contains non-zero audio samples

#### Scenario: Process drains command queue
- **WHEN** commands are queued before `oxs_synth_process()` is called
- **THEN** all pending commands are applied before audio rendering begins

### Requirement: Note control functions
The system SHALL provide `oxs_synth_note_on(handle, note, velocity)` and `oxs_synth_note_off(handle, note)` for triggering and releasing notes. These functions queue commands and are safe to call from any thread.

#### Scenario: Note on triggers voice
- **WHEN** `oxs_synth_note_on(handle, 60, 100)` is called and audio is processed
- **THEN** a voice is allocated playing MIDI note 60 (middle C) at velocity 100

#### Scenario: Note off releases voice
- **WHEN** `oxs_synth_note_off(handle, 60)` is called after note on
- **THEN** the voice for note 60 enters its release phase

#### Scenario: Voice stealing when pool exhausted
- **WHEN** more than 16 simultaneous notes are triggered
- **THEN** the oldest active voice is stolen for the new note

### Requirement: Panic function
The system SHALL provide `oxs_synth_panic(handle)` that immediately silences all voices.

#### Scenario: Panic kills all voices
- **WHEN** `oxs_synth_panic()` is called with 16 active voices
- **THEN** all voices enter idle state and next process call outputs silence

### Requirement: Output event retrieval
The system SHALL provide `oxs_synth_get_output_events(handle, *events, max_count)` to retrieve pending output events (peak levels, voice activity). This function is safe to call from the GUI thread.

#### Scenario: Retrieve peak levels after processing
- **WHEN** audio has been processed with active notes and GUI calls `oxs_synth_get_output_events()`
- **THEN** returned events include peak level data for left and right channels

#### Scenario: No events when idle
- **WHEN** no audio has been processed since last retrieval
- **THEN** function returns 0 events
