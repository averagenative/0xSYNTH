/*
 * 0xSYNTH Plugin GUI
 *
 * SDL2 + OpenGL + ImGui embedded inside the DAW's plugin window.
 * Same rendering code as the standalone — just different window hosting.
 */

#ifndef OXS_PLUGIN_GUI_H
#define OXS_PLUGIN_GUI_H

#include "../api/synth_api.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oxs_plugin_gui oxs_plugin_gui_t;

/* Create the plugin GUI (does not show yet). */
oxs_plugin_gui_t *oxs_plugin_gui_create(oxs_synth_t *synth);

/* Destroy the plugin GUI. */
void oxs_plugin_gui_destroy(oxs_plugin_gui_t *gui);

/* Attach GUI to host window (HWND on Windows, NSView on macOS, XWindow on Linux). */
void oxs_plugin_gui_attach(oxs_plugin_gui_t *gui, void *parent_handle);

/* Detach GUI from host window. */
void oxs_plugin_gui_detach(oxs_plugin_gui_t *gui);

/* Set visibility. */
void oxs_plugin_gui_set_visible(oxs_plugin_gui_t *gui, bool visible);

/* Get preferred size. */
void oxs_plugin_gui_get_size(oxs_plugin_gui_t *gui, uint32_t *width, uint32_t *height);

/* Set size. */
bool oxs_plugin_gui_set_size(oxs_plugin_gui_t *gui, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif /* OXS_PLUGIN_GUI_H */
