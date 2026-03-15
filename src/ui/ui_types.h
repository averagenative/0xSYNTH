/*
 * 0xSYNTH UI Abstraction Layer
 *
 * Toolkit-agnostic layout tree. Describes the full synth UI as data —
 * widget types, param bindings, grouping, position hints. No rendering code.
 *
 * GTK backend walks this tree to create GtkWidgets.
 * Plugin backend walks it to create Cairo draw commands on SDL2 surface.
 */

#ifndef OXS_UI_TYPES_H
#define OXS_UI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Widget types */
typedef enum {
    OXS_UI_GROUP,          /* container for child widgets */
    OXS_UI_KNOB,           /* rotary knob bound to a param */
    OXS_UI_SLIDER,         /* horizontal or vertical slider */
    OXS_UI_TOGGLE,         /* on/off toggle button */
    OXS_UI_DROPDOWN,       /* dropdown selector (integer param) */
    OXS_UI_LABEL,          /* static text label */
    OXS_UI_WAVEFORM,       /* oscillator waveform display */
    OXS_UI_ENVELOPE,       /* ADSR envelope curve display */
    OXS_UI_METER,          /* peak level meter (L/R) */
    OXS_UI_PRESET_BROWSER, /* preset list with load/save */
    OXS_UI_KEYBOARD,       /* virtual piano keyboard */
} oxs_ui_widget_type_t;

/* Layout direction for groups */
typedef enum {
    OXS_UI_HORIZONTAL,
    OXS_UI_VERTICAL,
} oxs_ui_direction_t;

/* Maximum children per group, max dropdown options */
#define OXS_UI_MAX_CHILDREN 32
#define OXS_UI_MAX_OPTIONS  16
#define OXS_UI_MAX_LABEL    48

/* Dropdown option */
typedef struct {
    char    label[OXS_UI_MAX_LABEL];
    int     value;
} oxs_ui_option_t;

/* Widget node */
typedef struct oxs_ui_widget {
    oxs_ui_widget_type_t type;
    char                 label[OXS_UI_MAX_LABEL];
    int32_t              param_id;       /* -1 if not bound to a param */

    /* Layout hints */
    int                  row, col;       /* grid position within parent group */
    int                  col_span;       /* how many columns to span (default 1) */

    /* Dropdown options (only for OXS_UI_DROPDOWN) */
    oxs_ui_option_t      options[OXS_UI_MAX_OPTIONS];
    int                  num_options;

    /* Envelope display bindings (only for OXS_UI_ENVELOPE) */
    int32_t              env_attack_id;
    int32_t              env_decay_id;
    int32_t              env_sustain_id;
    int32_t              env_release_id;

    /* Group properties */
    oxs_ui_direction_t   direction;      /* layout direction for children */
    struct oxs_ui_widget *children[OXS_UI_MAX_CHILDREN];
    int                  num_children;
} oxs_ui_widget_t;

/* The complete synth UI layout */
typedef struct {
    oxs_ui_widget_t *root;
    int              total_widgets; /* count for validation */
} oxs_ui_layout_t;

/* Build the full synth UI layout tree.
 * Returns a statically allocated layout — do not free. */
const oxs_ui_layout_t *oxs_ui_build_layout(void);

/* Validate that all param IDs in the layout exist in the registry. */
bool oxs_ui_validate_layout(const oxs_ui_layout_t *layout);

#endif /* OXS_UI_TYPES_H */
