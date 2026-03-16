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

/* Theme management */
void oxs_imgui_apply_theme(int theme_id);
int oxs_imgui_get_theme(void);
const char* oxs_imgui_theme_name(int id);
int oxs_imgui_theme_count(void);

/* Shared render function — call this to render the full synth UI.
 * Renders toolbar, 2-column layout, and settings panel.
 * Does NOT render the title bar or keyboard panel (host-specific). */
void oxs_imgui_render_synth_ui(oxs_synth_t *synth, float window_width, float window_height);

/* Render just the keyboard widget (for standalone bottom panel) */
void oxs_imgui_render_keyboard(oxs_synth_t *synth);

/* QWERTY keyboard handling */
void oxs_imgui_qwerty_key(oxs_synth_t *synth, int scancode, bool pressed);

/* Octave offset accessor (for arrow key handling in standalone) */
int oxs_imgui_get_octave_offset(void);
void oxs_imgui_set_octave_offset(int offset);
