/*
 * 0xSYNTH Shared ImGui Widgets
 *
 * All synth rendering code shared between standalone and plugin.
 * Themes, knobs, envelopes, meters, layout, toolbar — everything
 * needed to draw the full synth UI.
 */

#pragma once

#include "../api/synth_api.h"

#ifdef __cplusplus
extern "C" {
#include "../ui/ui_types.h"
#include "../engine/types.h"
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Theme management */
void oxs_imgui_apply_theme(int theme_id);
int oxs_imgui_get_theme(void);
const char* oxs_imgui_theme_name(int id);
int oxs_imgui_theme_count(void);

/* Render the toolbar — synth selector, randomize, reset, theme, REC,
 * help, gear icon, separator, and settings panel.
 * Self-contained; callable from a separate ImGui window. */
void oxs_imgui_render_toolbar(oxs_synth_t *synth);

/* Render the synth parameter layout and mod matrix.
 * Does NOT include the toolbar — call oxs_imgui_render_toolbar() separately. */
void oxs_imgui_render_synth_ui(oxs_synth_t *synth, float window_width, float window_height);

/* Render just the keyboard widget (for standalone bottom panel) */
void oxs_imgui_render_keyboard(oxs_synth_t *synth);

/* Render oscilloscope (for fixed top panel) */
void oxs_imgui_render_scope(oxs_synth_t *synth);

/* Set plugin mode (shows preview-only label on keyboard) */
void oxs_imgui_set_plugin_mode(bool is_plugin);

/* QWERTY keyboard handling */
void oxs_imgui_qwerty_key(oxs_synth_t *synth, int scancode, bool pressed);

/* Octave offset accessor (for arrow key handling in standalone) */
int oxs_imgui_get_octave_offset(void);
void oxs_imgui_set_octave_offset(int offset);
void oxs_imgui_set_octave_offset_with_synth(oxs_synth_t *synth, int offset);

/* Pitch bend arrow state (prevents snap decay while arrows held) */
void oxs_imgui_set_pitch_arrow_held(bool held);

/* Recording — set recorder pointer for REC button in toolbar.
 * Pass NULL to disable recording (e.g. in plugin context). */
void oxs_imgui_set_recorder(void *recorder, uint32_t sample_rate);

/* MIDI note output callback — set by plugin to forward QWERTY notes to DAW.
 * Callback signature: void cb(void *ctx, uint8_t note, uint8_t vel, bool on)
 * When set, QWERTY notes are sent both to the synth AND to the callback. */
typedef void (*oxs_note_output_cb)(void *ctx, uint8_t note, uint8_t vel, bool on);
void oxs_imgui_set_note_output(oxs_note_output_cb cb, void *ctx);

/* Session UI state — save/restore theme, preset name, etc */
void oxs_imgui_save_session_ui(void);
void oxs_imgui_load_session_ui(void);

/* Extended session save/load with window geometry */
void oxs_imgui_save_session_ui_full(int win_x, int win_y, int win_w, int win_h, bool kb_visible);
bool oxs_imgui_load_session_ui_full(int *win_x, int *win_y, int *win_w, int *win_h, bool *kb_visible);

#ifdef __cplusplus
}
#endif
