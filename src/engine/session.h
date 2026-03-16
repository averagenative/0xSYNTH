/*
 * 0xSYNTH Extended Session State
 *
 * Saves/restores UI preferences alongside synth params:
 * - Theme selection
 * - Window size/position
 * - Selected preset name
 * - Version for forward compatibility
 */

#ifndef OXS_SESSION_H
#define OXS_SESSION_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int      theme_id;
    int      window_x, window_y;
    int      window_w, window_h;
    char     preset_name[128];
    char     version[16];
    int      octave_offset;
    bool     keyboard_visible;
} oxs_session_ui_t;

/* Save UI state to JSON file */
bool oxs_session_ui_save(const oxs_session_ui_t *ui, const char *path);

/* Load UI state from JSON file. Returns false if file not found. */
bool oxs_session_ui_load(oxs_session_ui_t *ui, const char *path);

/* Get the session UI state file path */
const char *oxs_session_ui_path(void);

#endif /* OXS_SESSION_H */
