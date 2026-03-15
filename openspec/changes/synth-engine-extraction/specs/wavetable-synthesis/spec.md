## ADDED Requirements

### Requirement: Wavetable oscillator
The engine SHALL provide wavetable synthesis with bank-based playback. A wavetable bank contains an ordered sequence of single-cycle waveform frames. The oscillator SHALL interpolate between adjacent frames based on a position parameter (0.0-1.0).

#### Scenario: Position morphing
- **WHEN** wavetable position sweeps from 0.0 to 1.0
- **THEN** output timbre transitions smoothly through all frames in the bank

#### Scenario: Position smoothing prevents clicks
- **WHEN** position jumps abruptly
- **THEN** one-pole smoother (~5ms) prevents audible discontinuities

### Requirement: Wavetable modulation
The engine SHALL support modulation of wavetable position by envelope depth and LFO depth parameters.

#### Scenario: Envelope modulates position
- **WHEN** wavetable envelope depth is 0.5 and filter envelope triggers
- **THEN** wavetable position is offset by up to ±0.5 following the envelope shape

#### Scenario: LFO modulates position
- **WHEN** wavetable LFO depth is 0.3 and LFO is running
- **THEN** wavetable position oscillates by ±0.3 at the LFO rate

### Requirement: Wavetable bank loading
The engine SHALL load wavetable banks from WAV files using dr_libs. Stock wavetable files SHALL be bundled with the application.

#### Scenario: Load bank from file
- **WHEN** a valid wavetable WAV file is loaded
- **THEN** bank frames are parsed and available for oscillator playback

#### Scenario: Stock wavetables available at startup
- **WHEN** the synth is initialized
- **THEN** bundled wavetable banks are accessible without user action
