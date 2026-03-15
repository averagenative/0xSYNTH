## ADDED Requirements

### Requirement: 4-operator FM synthesis
The engine SHALL provide 4-operator FM synthesis. Each operator SHALL have: frequency ratio (0.5-16.0), level (0.0-1.0), feedback (0.0-1.0), and independent ADSR envelope.

#### Scenario: Single modulator-carrier pair
- **WHEN** algorithm 0 is selected (serial chain) with op1 modulating op0
- **THEN** op0 output contains sidebands from frequency modulation by op1

#### Scenario: Operator feedback generates harmonics
- **WHEN** an operator's feedback is set above 0
- **THEN** operator output becomes progressively more harmonically rich (approaching sawtooth at high feedback)

#### Scenario: Per-operator envelopes shape timbre over time
- **WHEN** a modulator has short decay and carrier has long sustain
- **THEN** sound starts bright (strong modulation) and becomes pure (modulation fades)

### Requirement: 8 FM algorithms
The engine SHALL support 8 routing algorithms defining modulator-carrier connections:
0. Serial: 3→2→1→0 (carrier: 0)
1. Two routes: 2→1→0, 3→0 (carrier: 0)
2. Two pairs: 3→2, 1→0 (carriers: 0, 2)
3. Three-chain + free: 3→2→1, 0 (carriers: 0, 1)
4. Three mods: 3,2,1→0 (carriers: 0, 2, 3)
5. Pair + two free: 3→2, 1, 0 (carriers: 0, 1, 2)
6. All additive: 3,2,1,0 (carriers: 0, 1, 2, 3)
7. Split: 3→{1,2}, 0 (carriers: 0, 1, 2)

#### Scenario: Each algorithm produces distinct output
- **WHEN** the same operator settings are used with different algorithms
- **THEN** rendered audio has measurably different spectral content

#### Scenario: Carrier output normalization
- **WHEN** an algorithm has N carriers
- **THEN** combined output is normalized by sqrt(N) to maintain consistent volume

### Requirement: FM mode selection
The engine SHALL switch to FM synthesis when `OXS_PARAM_SYNTH_MODE` is set to `OXS_SYNTH_MODE_FM`. In FM mode, subtractive oscillators, filter, and unison are bypassed.

#### Scenario: Mode switch to FM
- **WHEN** synth mode parameter is changed from SUBTRACTIVE to FM
- **THEN** subsequent notes use FM synthesis path with 4 operators
