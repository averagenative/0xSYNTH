## ADDED Requirements

### Requirement: Layout-as-data
The UI abstraction layer SHALL define the synth interface as a tree of `oxs_ui_layout_t` widget descriptors. Each descriptor specifies widget type, parameter ID binding, position hints, and size hints. The layout is data, not rendering code.

#### Scenario: Layout tree covers all parameters
- **WHEN** `oxs_ui_build_layout()` is called
- **THEN** returned layout tree contains widget bindings for every user-facing parameter in the registry

#### Scenario: No duplicate bindings
- **WHEN** layout tree is traversed
- **THEN** no parameter ID appears in more than one editable widget binding

### Requirement: Widget type vocabulary
The abstraction SHALL define these widget types: knob, slider, toggle, dropdown, label, group/section, waveform_display, envelope_curve, level_meter, preset_browser.

#### Scenario: All widget types are defined
- **WHEN** UI backend implements the backend interface
- **THEN** it handles all defined widget types

### Requirement: Backend interface
The abstraction SHALL define `oxs_ui_backend_t` as a struct of function pointers: `create_knob()`, `create_slider()`, `create_toggle()`, `create_dropdown()`, `create_group()`, `create_waveform_display()`, `create_envelope_curve()`, `create_level_meter()`, `create_preset_browser()`, `update_value()`, `set_label()`.

#### Scenario: GTK backend implements all functions
- **WHEN** GTK backend is initialized
- **THEN** all function pointers in `oxs_ui_backend_t` are non-NULL

#### Scenario: Future backend can be substituted
- **WHEN** a new backend (e.g., ImGui) implements `oxs_ui_backend_t`
- **THEN** it can render the full synth UI from the same layout tree without layout changes

### Requirement: Toolkit independence
The layout tree and backend interface SHALL NOT reference any toolkit-specific types (no GtkWidget, no ImGui calls). Toolkit-specific code lives only in backend implementations.

#### Scenario: Layout compiles without GTK headers
- **WHEN** oxs_ui_layout.h is included in a translation unit
- **THEN** it compiles without any GTK or ImGui headers present
