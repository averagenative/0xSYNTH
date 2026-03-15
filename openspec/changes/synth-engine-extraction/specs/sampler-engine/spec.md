## ADDED Requirements

### Requirement: Sample playback
The engine SHALL play loaded audio samples with Hermite interpolation for pitch shifting. Samples SHALL support velocity-sensitive gain and stereo panning.

#### Scenario: Play sample at original pitch
- **WHEN** a sample is loaded and triggered with pitch_offset=0
- **THEN** sample plays back at its recorded pitch with Hermite interpolation

#### Scenario: Pitch shift by semitones
- **WHEN** a sample is triggered with pitch_offset=+12
- **THEN** sample plays back one octave higher (rate = 2^(12/12) = 2.0)

#### Scenario: Velocity sensitivity
- **WHEN** a sample is triggered with velocity=64 vs velocity=127
- **THEN** the lower velocity note is quieter than the higher velocity note

### Requirement: Sample loading
The system SHALL provide `oxs_synth_load_sample(handle, path)` supporting WAV, FLAC, and MP3 formats via dr_libs. Samples SHALL be stored in pre-allocated slots.

#### Scenario: Load WAV file
- **WHEN** a valid WAV file is loaded
- **THEN** sample data is decoded and stored, ready for playback

#### Scenario: Load FLAC file
- **WHEN** a valid FLAC file is loaded
- **THEN** sample data is decoded and stored, ready for playback

#### Scenario: Reject unsupported format
- **WHEN** an unsupported file type is provided
- **THEN** function returns an error code

### Requirement: SoundFont support
The engine SHALL load SF2 SoundFont files via TinySoundFont and render SF2 instruments alongside synthesized voices.

#### Scenario: Load SF2 instrument
- **WHEN** an SF2 file is loaded and an instrument preset is selected
- **THEN** triggering notes plays the SF2 instrument's samples with correct mapping

#### Scenario: SF2 mixed with synthesis
- **WHEN** both synth voices and SF2 voices are active
- **THEN** both are mixed together in the output
