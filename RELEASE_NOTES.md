# 0xSYNTH v1.0.0 Release

Multi-engine synthesizer with subtractive, FM, wavetable, and sampler engines. Available as standalone app and CLAP/VST3 plugins.

## Downloads

| Platform | File | Description |
|----------|------|-------------|
| Windows | `0xSYNTH-1.0.0-windows-x64-setup.exe` | Installer (standalone + plugins + presets) |
| Windows | `0xSYNTH-1.0.0-windows-x64.zip` | Portable zip |
| macOS | `0xSYNTH-1.0.0-macos-x64.dmg` | Disk image (drag to Applications) |
| macOS | `0xSYNTH-1.0.0-macos-x64.zip` | Zip with app + plugins |
| Linux | `0xSYNTH-1.0.0-linux-x64.tar.gz` | Standalone + plugins + presets |
| Linux | `0xSYNTH-1.0.0-x86_64.AppImage` | Single-file standalone (double-click to run) |

## Installation

### Windows

**Installer (recommended):** Run `0xSYNTH-1.0.0-windows-x64-setup.exe`. It installs the standalone app, CLAP plugin, and VST3 plugin automatically. Includes Start Menu and Desktop shortcuts.

**Portable zip:** Extract anywhere and run `0xsynth.exe`. To use plugins in your DAW, copy manually:
```
CLAP:  Copy 0xSYNTH.clap to C:\Program Files\Common Files\CLAP\
VST3:  Copy 0xSYNTH.vst3 to C:\Program Files\Common Files\VST3\0xSYNTH.vst3\Contents\x86_64-win\
```

### macOS

**DMG:** Open the disk image and drag 0xSYNTH to Applications.

**Zip:** Extract and copy `0xSYNTH.app` to `/Applications`.

**Plugin installation:**
```bash
mkdir -p ~/Library/Audio/Plug-Ins/CLAP
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp Plugins/0xSYNTH.clap ~/Library/Audio/Plug-Ins/CLAP/
cp -r Plugins/0xSYNTH.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

**Gatekeeper (unsigned app):** macOS may block the app with *"cannot be opened because the developer cannot be verified"*. Fix by running in Terminal:
```bash
# For the standalone app:
xattr -cr /Applications/0xSYNTH.app

# For the CLAP plugin:
xattr -cr ~/Library/Audio/Plug-Ins/CLAP/0xSYNTH.clap

# For the VST3 plugin:
xattr -cr ~/Library/Audio/Plug-Ins/VST3/0xSYNTH.vst3
```
Or go to **System Settings > Privacy & Security** and click **"Allow Anyway"** after attempting to open.

### Linux

**AppImage (easiest):**
```bash
chmod +x 0xSYNTH-1.0.0-x86_64.AppImage
./0xSYNTH-1.0.0-x86_64.AppImage
```

**tar.gz:** Extract and run `./0xsynth`. Plugin installation:
```bash
# CLAP plugin
mkdir -p ~/.clap
cp 0xSYNTH.clap ~/.clap/

# VST3 plugin
mkdir -p ~/.vst3
cp 0xSYNTH.vst3 ~/.vst3/
```

**Requires:** SDL2 (`sudo apt install libsdl2-2.0-0` on Ubuntu/Debian)

## Plugin Paths Reference

| Format | Windows | macOS | Linux |
|--------|---------|-------|-------|
| CLAP | `C:\Program Files\Common Files\CLAP\` | `~/Library/Audio/Plug-Ins/CLAP/` | `~/.clap/` |
| VST3 | `C:\Program Files\Common Files\VST3\` | `~/Library/Audio/Plug-Ins/VST3/` | `~/.vst3/` |

After copying plugins, restart your DAW and rescan.

## Highlights

### Synthesis
- 4 engines: Subtractive, FM (4-op, 8 algorithms), Wavetable (4 banks + import), Sampler
- Sub-oscillator (square/sine, -1/-2 oct) and noise oscillator (white/pink)
- 1-7 voice unison with stereo spread

### Modulation
- 8-slot modulation matrix with 16 source types
- 3 independent LFOs with BPM sync
- 4 macro performance knobs
- MPE support (per-note pitch bend, pressure, slide)

### Filters
- 7 filter types: Lowpass, Highpass, Bandpass, Notch, Ladder 24dB, Comb, Formant
- Dual filters with serial/parallel routing

### Effects
- 3 effect slots, 15 types (delay, reverb, chorus, phaser, distortion, bitcrusher, etc.)

### MIDI Controller Support
- Works with any USB or Bluetooth MIDI controller out of the box
- **MIDI Learn** — right-click any knob, move a controller knob/fader to map it
- Default CC mappings for common budget controllers (GM2 standard + RockJam RJMK25)
- Pitch bend, mod wheel, channel aftertouch supported
- Audio output and MIDI input device selection in Settings
- Windows (WinMM), Linux (ALSA), macOS (CoreMIDI) MIDI input

### Performance
- Arpeggiator (5 modes, rate sync, gate, 1-4 octaves)
- Pitch bend wheel with snap/hold modes
- 2x/4x oversampling

### Recording
- WAV (16/24/32-bit), FLAC, MP3 recording
- Timestamped filenames, format selectable in settings

### GUI
- Real-time oscilloscope
- 7 themes (Dark, Hacker, Midnight, Amber, Vaporwave, Neon, Light)
- 70 factory presets
- Custom window chrome (Windows)
- Virtual keyboard with QWERTY mapping

### Architecture
- Pure C engine (C99/C11), zero allocations in audio path
- Thread-safe atomic params + lock-free queues
- 11 test suites, all passing
