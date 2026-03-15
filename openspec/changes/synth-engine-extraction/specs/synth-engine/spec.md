## ADDED Requirements

### Requirement: Oscillator waveforms
The engine SHALL provide four oscillator waveforms: sawtooth, square, triangle, and sine. Two oscillators (osc1, osc2) SHALL be mixable with a crossfade parameter.

#### Scenario: Each waveform produces distinct timbre
- **WHEN** a note is triggered with each waveform type in sequence
- **THEN** rendered audio for each waveform has distinct spectral content

#### Scenario: Oscillator mix crossfade
- **WHEN** osc_mix is set to 0.0
- **THEN** only osc1 is audible
- **WHEN** osc_mix is set to 1.0
- **THEN** only osc2 is audible

### Requirement: ADSR envelopes
The engine SHALL provide two ADSR envelope generators per voice: one for amplitude, one for filter cutoff. Attack, decay, and release times SHALL range from 0.001s to 10s. Sustain level SHALL range from 0.0 to 1.0.

#### Scenario: Amplitude envelope shapes output
- **WHEN** a note is triggered with attack=0.1s, decay=0.2s, sustain=0.5, release=0.3s
- **THEN** output amplitude ramps 0→1 in 0.1s, decays to 0.5 in 0.2s, holds at 0.5, then decays to 0 in 0.3s after note off

#### Scenario: Filter envelope modulates cutoff
- **WHEN** filter envelope depth is set to 5000Hz
- **THEN** filter cutoff is modulated by up to ±5000Hz from the base cutoff value following the envelope shape

### Requirement: Biquad filter
The engine SHALL provide a state-variable biquad filter with three modes: lowpass, highpass, bandpass. Cutoff range SHALL be 20Hz to 20000Hz. Resonance (Q) SHALL range from 0.5 to 20.0. Cutoff changes SHALL be smoothed over ~3ms to prevent clicks.

#### Scenario: Lowpass filter attenuates highs
- **WHEN** filter type is lowpass and cutoff is 500Hz
- **THEN** frequencies above 500Hz are progressively attenuated in rendered output

#### Scenario: Cutoff smoothing prevents clicks
- **WHEN** filter cutoff jumps from 200Hz to 10000Hz between process calls
- **THEN** rendered output transitions smoothly without audible clicks or pops

#### Scenario: High resonance is stable
- **WHEN** resonance is set to maximum (20.0) with cutoff sweeping
- **THEN** filter output remains bounded (no numerical explosion)

### Requirement: LFO modulation
The engine SHALL provide one LFO per voice with sine, triangle, square, and sawtooth waveforms. LFO SHALL modulate pitch (±2 semitones), filter cutoff (±2000Hz), or amplitude (×0.5–1.5). LFO rate SHALL support free-running (Hz) and BPM-synced divisions.

#### Scenario: LFO modulates filter cutoff
- **WHEN** LFO destination is filter, rate is 2Hz, depth is 0.5
- **THEN** filter cutoff oscillates around base value at 2Hz

#### Scenario: BPM sync
- **WHEN** LFO sync is enabled with 1/4 note division at 120 BPM
- **THEN** LFO completes one cycle every 0.5 seconds (one quarter note)

### Requirement: Unison and detune
The engine SHALL support 1-7 unison voices per note with configurable detune (0-50 cents). Unison voices SHALL be spread across the stereo field. Gain SHALL be normalized by 1/sqrt(n) to prevent volume increase.

#### Scenario: Unison thickens sound
- **WHEN** unison voices is set to 5 with detune of 20 cents
- **THEN** rendered audio contains multiple detuned copies spread in stereo, with total gain normalized

#### Scenario: Single voice is clean
- **WHEN** unison voices is set to 1
- **THEN** output is a single centered oscillator with no detuning artifacts

### Requirement: Polyphonic voice management
The engine SHALL manage a pool of 16 voices. Voice allocation SHALL find a free voice or steal the oldest active voice. Voice release SHALL enter the ADSR release phase rather than immediate silence.

#### Scenario: 16 simultaneous notes
- **WHEN** 16 different notes are triggered without release
- **THEN** all 16 voices are active and audible

#### Scenario: 17th note steals oldest
- **WHEN** a 17th note is triggered with 16 active voices
- **THEN** the voice with the earliest trigger time is replaced by the new note

#### Scenario: Released voices free after envelope completes
- **WHEN** a note is released and the release envelope reaches zero
- **THEN** the voice returns to idle state and is available for reuse
