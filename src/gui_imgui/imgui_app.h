/*
 * 0xSYNTH ImGui Application
 *
 * SDL2 + OpenGL + Dear ImGui frontend.
 * Used for both standalone (normal window) and plugin (embedded window).
 */

#ifndef OXS_IMGUI_APP_H
#define OXS_IMGUI_APP_H

#include "../api/synth_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Launch the ImGui standalone application.
 * Blocks until the window is closed. */
int oxs_imgui_run(oxs_synth_t *synth, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* OXS_IMGUI_APP_H */
