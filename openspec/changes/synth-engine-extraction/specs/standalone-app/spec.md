## ADDED Requirements

### Requirement: Audio output via miniaudio
The standalone application SHALL use miniaudio for cross-platform audio output. The audio callback SHALL call `oxs_synth_process()` to fill the output buffer.

#### Scenario: Audio plays on Linux
- **WHEN** standalone is launched on Linux with default audio device
- **THEN** synthesized audio is audible through system speakers/headphones

#### Scenario: Configurable audio device
- **WHEN** `--audio-device` CLI flag specifies a device name
- **THEN** audio output uses the specified device

#### Scenario: Configurable buffer size
- **WHEN** `--buffer-size 512` is specified
- **THEN** audio callback processes 512 frames per invocation

### Requirement: MIDI input
The standalone application SHALL accept MIDI input from hardware controllers using platform-native APIs: ALSA raw MIDI (Linux), CoreMIDI (macOS), Windows MIDI API. Note-on/off and CC messages SHALL be dispatched to the synth API.

#### Scenario: Hardware MIDI keyboard works
- **WHEN** a USB MIDI keyboard is connected and standalone is running
- **THEN** pressing keys triggers synth voices

#### Scenario: MIDI CC controls parameters
- **WHEN** a mapped MIDI CC message is received
- **THEN** the corresponding synth parameter changes

#### Scenario: List available MIDI devices
- **WHEN** `--list-midi` CLI flag is used
- **THEN** all available MIDI input devices are printed

### Requirement: CLI interface
The standalone SHALL accept command-line flags: `--list-audio`, `--list-midi`, `--audio-device`, `--midi-device`, `--sample-rate`, `--buffer-size`, `--preset`.

#### Scenario: Load preset from CLI
- **WHEN** `--preset path/to/my_preset.json` is specified
- **THEN** synth initializes with the specified preset loaded

#### Scenario: List audio devices
- **WHEN** `--list-audio` is specified
- **THEN** available audio output devices are printed and application exits

### Requirement: Headless mode
The standalone SHALL be runnable without a GUI (headless) for testing and embedded use cases. When no GUI is available, it SHALL run a poll-based event loop processing MIDI and audio.

#### Scenario: Run without display
- **WHEN** standalone is launched in a headless environment (no DISPLAY/WAYLAND_DISPLAY)
- **THEN** audio and MIDI still function without GUI
