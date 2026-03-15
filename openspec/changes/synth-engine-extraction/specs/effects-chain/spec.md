## ADDED Requirements

### Requirement: Master bus effect chain
The engine SHALL provide 3 effect slots on the master output bus, processed in series after voice mixing. Each slot has independent type selection and bypass toggle.

#### Scenario: Effects applied in order
- **WHEN** slot 0 is distortion, slot 1 is delay, slot 2 is reverb
- **THEN** audio is distorted first, then delayed, then reverbed

#### Scenario: Bypass skips effect
- **WHEN** a slot's bypass flag is set
- **THEN** audio passes through unmodified for that slot

#### Scenario: Empty slot passes through
- **WHEN** a slot's type is EFFECT_NONE
- **THEN** audio passes through unmodified

### Requirement: 15 effect types
The engine SHALL support these effect types: filter (biquad LP/HP/BP), delay (up to 2s with feedback), reverb (Freeverb), overdrive (soft clip), fuzz (hard clip), chorus, bitcrusher, compressor, phaser, flanger, tremolo, ring modulator, tape saturation, shimmer reverb. Each type SHALL have type-specific parameters exposed through the parameter system.

#### Scenario: Each effect type alters audio distinctly
- **WHEN** each effect type is activated on a test signal
- **THEN** output is audibly and measurably different from dry input

#### Scenario: Delay feedback decays
- **WHEN** delay is active with feedback at 0.5
- **THEN** each echo is approximately half the amplitude of the previous one

#### Scenario: Reverb tail scales with room size
- **WHEN** reverb room_size is increased from 0.2 to 0.9
- **THEN** reverb tail duration increases proportionally

#### Scenario: Compressor reduces dynamic range
- **WHEN** compressor threshold is set below input peak
- **THEN** output peaks are attenuated by the specified ratio

### Requirement: Effect buffer pre-allocation
All effect buffers (delay lines, reverb comb/allpass filters, chorus buffers) SHALL be pre-allocated during `oxs_synth_create()`. No memory allocation SHALL occur during audio processing.

#### Scenario: Effect buffers ready at init
- **WHEN** synth is created
- **THEN** all effect buffers are allocated for maximum configuration (2s delay, full reverb)

#### Scenario: No allocation during process
- **WHEN** effect type is changed during playback
- **THEN** existing pre-allocated buffers are reused (cleared, not reallocated)
