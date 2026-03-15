## ADDED Requirements

### Requirement: GTK 4 main window
The frontend SHALL render a GTK 4 window displaying all synth controls organized by section: oscillator, filter, envelopes, LFO, FM operators, wavetable, effects, preset browser, and master.

#### Scenario: Window launches with all sections
- **WHEN** standalone is launched with GTK frontend
- **THEN** main window displays all synth control sections

#### Scenario: Cross-platform appearance
- **WHEN** frontend runs on Linux, macOS, or Windows
- **THEN** all controls render correctly with dark theme applied

### Requirement: Parameter-bound controls
Every editable control SHALL be bound to a parameter ID via the UI abstraction layer. Adjusting a control SHALL call `oxs_param_set()`. Control positions SHALL reflect current `oxs_param_get()` values.

#### Scenario: Knob adjusts parameter
- **WHEN** user drags a filter cutoff knob
- **THEN** `oxs_param_set(handle, OXS_PARAM_FILTER_CUTOFF, value)` is called with the new value

#### Scenario: Controls reflect loaded preset
- **WHEN** a preset is loaded
- **THEN** all knobs and sliders update to show the preset's parameter values

### Requirement: Custom knob widget
The frontend SHALL implement a custom rotary knob widget using GtkDrawingArea + Cairo. The knob SHALL support mouse drag for value changes and display a value label.

#### Scenario: Knob responds to mouse drag
- **WHEN** user clicks and drags vertically on a knob
- **THEN** parameter value changes proportionally to drag distance

#### Scenario: Knob displays current value
- **WHEN** a knob's parameter value changes
- **THEN** the knob visual and label update to reflect the new value

### Requirement: Waveform and envelope visualization
The frontend SHALL display oscillator waveform shape and ADSR envelope curves using GtkDrawingArea, updated at ~30fps.

#### Scenario: Waveform updates with oscillator type
- **WHEN** oscillator waveform is changed from saw to square
- **THEN** waveform display updates to show square wave shape

#### Scenario: Envelope curve reflects parameters
- **WHEN** attack time is increased
- **THEN** envelope curve display shows a longer attack slope

### Requirement: Level meters
The frontend SHALL display stereo peak level meters reading from the output event queue, with smooth falloff animation at ~30fps.

#### Scenario: Meters respond to audio
- **WHEN** notes are playing
- **THEN** level meters show peak activity corresponding to audio output

#### Scenario: Meters fall off when silent
- **WHEN** all notes are released and audio fades
- **THEN** meters smoothly decay to zero

### Requirement: Preset browser
The frontend SHALL display a preset browser panel listing factory and user presets. Click to load. Save button for user presets with name input.

#### Scenario: Browse and load preset
- **WHEN** user clicks a preset name in the browser
- **THEN** preset is loaded and all controls update

#### Scenario: Save user preset
- **WHEN** user clicks save and enters a name
- **THEN** current parameters are saved as a JSON preset in the user directory

### Requirement: Virtual keyboard
The frontend SHALL display a clickable piano keyboard. Mouse down triggers note-on, mouse up triggers note-off. QWERTY keyboard mapping SHALL also be supported (Z-M lower octave, Q-P upper octave).

#### Scenario: Click piano key plays note
- **WHEN** user clicks a piano key
- **THEN** `oxs_synth_note_on()` is called with the corresponding MIDI note

#### Scenario: QWERTY keyboard plays notes
- **WHEN** user presses 'A' key on QWERTY keyboard
- **THEN** corresponding note is triggered

### Requirement: Effect chain editor
The frontend SHALL display 3 effect slots with type selector dropdown, per-effect parameter controls, and bypass toggle per slot.

#### Scenario: Change effect type
- **WHEN** user selects "Reverb" from slot dropdown
- **THEN** slot switches to reverb and reverb-specific controls appear

#### Scenario: Bypass effect
- **WHEN** user clicks bypass toggle on a slot
- **THEN** that effect is bypassed and audio passes through unmodified

### Requirement: FM operator matrix
The frontend SHALL display a 4-operator view with per-operator knobs (ratio, level, feedback, ADSR) and an algorithm selector with visual routing diagram.

#### Scenario: Algorithm selector shows routing
- **WHEN** user changes FM algorithm
- **THEN** visual diagram updates to show operator routing for the selected algorithm

#### Scenario: Operator knobs control FM params
- **WHEN** user adjusts operator 2's frequency ratio knob
- **THEN** `OXS_PARAM_FM_OP2_RATIO` is updated and FM sound changes

### Requirement: Dark theme
The frontend SHALL use a dark color theme consistent with 0x808's GTK aesthetic -- dark backgrounds, accent-colored knobs, readable labels, professional appearance.

#### Scenario: Dark theme applied at startup
- **WHEN** frontend launches
- **THEN** window uses dark background with high-contrast controls
