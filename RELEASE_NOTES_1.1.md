# 0xSYNTH v1.1.0 — Step Sequencer

**You don't need to know how to play piano to make music.**

v1.1.0 introduces a **32-step sequencer** with a one-click randomize button that generates musical patterns in random scales, tempos, and directions. Hit Dice, tweak a knob, record. That's it.

![Step Sequencer](https://raw.githubusercontent.com/averagenative/0xSYNTH/master/screenshots/step-sequencer.png)

## Step Sequencer

- **32-step pattern grid** with per-step note, velocity, gate length, slide, and accent
- **Dice button** — one click generates a random pattern in a musical scale (major, minor, pentatonic, blues) with random BPM (45-200), swing, direction, and rests
- **4 direction modes** — Forward, Reverse, Ping-Pong, Random
- **Slide** — legato between consecutive steps for smooth bass lines
- **Accent** — velocity boost on selected steps for rhythmic emphasis
- **Swing** — adjustable groove from straight to heavy shuffle
- **Rainbow shimmer header** — because the sequencer deserves to look as fun as it sounds
- **Interactive grid** — left-click toggles steps, scroll wheel changes pitch, right-click toggles slide, middle-click toggles accent
- **Moving playhead** — glowing indicator shows the current step in real-time

### How to use it

1. Pick any synth preset (or hit Randomize for a random sound)
2. Scroll down to **Step Sequencer** and click **ON**
3. Hit **Dice** to generate a random pattern
4. Twist knobs — filter cutoff, resonance, LFO depth — while the pattern plays
5. Hit **REC** to capture it

The sequencer and arpeggiator are mutually exclusive — enabling one disables the other.

## MIDI Controller Support (New)

- **Windows MIDI input** — full WinMM support, any USB/Bluetooth controller works out of the box
- **MIDI Learn** — right-click any knob, move a controller knob/fader, it auto-maps
- **Flashing yellow indicator** in the toolbar when learning
- **Default CC mappings** for budget controllers (GM2 standard + RockJam RJMK25 knob defaults)
- **Audio output and MIDI input device selection** in Settings panel

### Default Knob Mappings (RockJam / GM2)

| CC | Knob | Mapped To |
|----|------|-----------|
| 70 | Knob 1 | Osc Mix |
| 71 | Knob 2 | Filter Resonance |
| 72 | Knob 3 | Amp Release |
| 73 | Knob 4 | Amp Attack |
| 74 | Knob 5 | Filter Cutoff |
| 75 | Knob 6 | LFO Rate |
| 76 | Knob 7 | LFO Depth |
| 77 | Knob 8 | Filter Env Depth |

## Bug Fixes

- Sequencer always starts OFF on launch, preset load, and session restore
- Turning off the sequencer releases all held notes
- Swing control displays correctly (0-100%)
- 24-bit WAV recording byte alignment fixed (clean recordings)
- Recording falls back to Music folder when exe directory is read-only (Program Files)

## Downloads

| Platform | File |
|----------|------|
| Windows | `0xSYNTH-1.1.0-windows-x64-setup.exe` (installer) |
| Windows | `0xSYNTH-1.1.0-windows-x64.zip` (portable) |
| macOS | `0xSYNTH-1.1.0-macos-x64.dmg` |
| macOS | `0xSYNTH-1.1.0-macos-x64.zip` |
| Linux | `0xSYNTH-1.1.0-linux-x64.tar.gz` |
| Linux | `0xSYNTH-1.1.0-x86_64.AppImage` |

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the full v1.0.0 feature list.
