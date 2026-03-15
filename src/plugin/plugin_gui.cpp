/*
 * 0xSYNTH Plugin GUI Implementation
 *
 * SDL2 window embedded inside DAW host window.
 * Uses the same ImGui rendering as the standalone app.
 *
 * Platform-specific embedding:
 * - Windows: SetParent(sdl_hwnd, host_hwnd) + WS_CHILD
 * - Linux: SDL_CreateWindowFrom() or XReparent (future)
 * - macOS: NSView embedding (future)
 */

#include "plugin_gui.h"

extern "C" {
#include "../ui/ui_types.h"
#include "../engine/types.h"
}

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_syswm.h>

#include <cstdio>
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Import ImGui widget functions from imgui_app.cpp ─────────────────── */
/* We reuse the same render_widget function — declare it extern */

/* For now, we duplicate the minimal render logic inline.
 * A proper refactor would extract shared widget code to a separate file. */

/* Reuse the layout tree */
extern "C" const oxs_ui_layout_t *oxs_ui_build_layout(void);

/* ─── Plugin GUI State ───────────────────────────────────────────────────── */

struct oxs_plugin_gui {
    oxs_synth_t     *synth;
    SDL_Window      *window;
    SDL_GLContext    gl_ctx;
    SDL_Thread      *render_thread;
    volatile bool    running;
    volatile bool    visible;
    uint32_t         width;
    uint32_t         height;
    void            *parent_handle;
    ImGuiContext    *imgui_ctx;
};

/* ─── Render Thread ──────────────────────────────────────────────────────── */

/* Forward declare a simplified render_widget for the plugin */
static void plugin_render_widget(const oxs_ui_widget_t *w, oxs_synth_t *synth);

/* Simplified knob for plugin context */
static bool PluginKnob(const char *label, float *value, float vmin, float vmax)
{
    ImGui::PushItemWidth(60);
    char id[64];
    snprintf(id, sizeof(id), "##%s", label);
    bool changed = ImGui::SliderFloat(id, value, vmin, vmax, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("%s", label);
    return changed;
}

static void plugin_render_widget(const oxs_ui_widget_t *w, oxs_synth_t *synth)
{
    switch (w->type) {
    case OXS_UI_GROUP:
        if (w->label[0] && strcmp(w->label, "0xSYNTH") != 0) {
            if (ImGui::CollapsingHeader(w->label, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (w->direction == OXS_UI_HORIZONTAL) {
                    for (int i = 0; i < w->num_children; i++) {
                        if (i > 0) ImGui::SameLine();
                        ImGui::BeginGroup();
                        plugin_render_widget(w->children[i], synth);
                        ImGui::EndGroup();
                    }
                } else {
                    for (int i = 0; i < w->num_children; i++)
                        plugin_render_widget(w->children[i], synth);
                }
            }
        } else {
            for (int i = 0; i < w->num_children; i++)
                plugin_render_widget(w->children[i], synth);
        }
        break;

    case OXS_UI_KNOB: {
        oxs_param_info_t info;
        if (oxs_synth_param_info(synth, (uint32_t)w->param_id, &info)) {
            float val = oxs_synth_get_param(synth, (uint32_t)w->param_id);
            if (PluginKnob(w->label, &val, info.min, info.max))
                oxs_synth_set_param(synth, (uint32_t)w->param_id, val);
        }
        break;
    }

    case OXS_UI_DROPDOWN: {
        float val = oxs_synth_get_param(synth, (uint32_t)w->param_id);
        int cur = (int)val;
        const char *preview = (cur >= 0 && cur < w->num_options) ? w->options[cur].label : "?";
        ImGui::PushItemWidth(100);
        char id[64];
        snprintf(id, sizeof(id), "%s##dd_%d", w->label, w->param_id);
        if (ImGui::BeginCombo(id, preview)) {
            for (int i = 0; i < w->num_options; i++) {
                if (ImGui::Selectable(w->options[i].label, i == cur))
                    oxs_synth_set_param(synth, (uint32_t)w->param_id, (float)i);
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        break;
    }

    case OXS_UI_TOGGLE: {
        float val = oxs_synth_get_param(synth, (uint32_t)w->param_id);
        bool on = val > 0.5f;
        if (ImGui::Checkbox(w->label, &on))
            oxs_synth_set_param(synth, (uint32_t)w->param_id, on ? 1.0f : 0.0f);
        break;
    }

    case OXS_UI_LABEL:
        ImGui::Text("%s", w->label);
        break;

    default:
        break;
    }
}

static int render_thread_func(void *data)
{
    oxs_plugin_gui_t *gui = (oxs_plugin_gui_t *)data;

    SDL_GL_MakeCurrent(gui->window, gui->gl_ctx);

    /* ImGui setup on render thread */
    IMGUI_CHECKVERSION();
    gui->imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(gui->imgui_ctx);
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);

    ImGui_ImplSDL2_InitForOpenGL(gui->window, gui->gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    const oxs_ui_layout_t *layout = oxs_ui_build_layout();

    while (gui->running) {
        if (!gui->visible) {
            SDL_Delay(50);
            continue;
        }

        /* Process SDL events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)gui->width, (float)gui->height));
        ImGui::Begin("0xSYNTH", NULL,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        if (layout && layout->root)
            plugin_render_widget(layout->root, gui->synth);

        ImGui::End();
        ImGui::Render();

        glViewport(0, 0, (int)gui->width, (int)gui->height);
        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(gui->window);

        SDL_Delay(16); /* ~60fps */
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(gui->imgui_ctx);
    gui->imgui_ctx = NULL;

    return 0;
}

/* ─── Public API ─────────────────────────────────────────────────────────── */

extern "C" {

oxs_plugin_gui_t *oxs_plugin_gui_create(oxs_synth_t *synth)
{
    oxs_plugin_gui_t *gui = new oxs_plugin_gui_t();
    memset(gui, 0, sizeof(*gui));
    gui->synth = synth;
    gui->width = 900;
    gui->height = 700;
    return gui;
}

void oxs_plugin_gui_destroy(oxs_plugin_gui_t *gui)
{
    if (!gui) return;
    oxs_plugin_gui_detach(gui);
    delete gui;
}

void oxs_plugin_gui_attach(oxs_plugin_gui_t *gui, void *parent_handle)
{
    if (!gui || gui->running) return;

    gui->parent_handle = parent_handle;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Plugin GUI: SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);

    gui->window = SDL_CreateWindow(
        "0xSYNTH",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        (int)gui->width, (int)gui->height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN
    );
    if (!gui->window) {
        fprintf(stderr, "Plugin GUI: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }

    gui->gl_ctx = SDL_GL_CreateContext(gui->window);
    if (!gui->gl_ctx) {
        fprintf(stderr, "Plugin GUI: GL context failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(gui->window);
        gui->window = NULL;
        return;
    }

    /* Reparent into host window (platform-specific) */
#ifdef OXS_PLATFORM_WINDOWS
    SDL_SysWMinfo wm_info;
    SDL_VERSION(&wm_info.version);
    if (SDL_GetWindowWMInfo(gui->window, &wm_info)) {
        HWND sdl_hwnd = wm_info.info.win.window;
        HWND host_hwnd = (HWND)parent_handle;
        SetParent(sdl_hwnd, host_hwnd);
        LONG style = GetWindowLong(sdl_hwnd, GWL_STYLE);
        style = (style & ~WS_POPUP) | WS_CHILD;
        SetWindowLong(sdl_hwnd, GWL_STYLE, style);
        SetWindowPos(sdl_hwnd, NULL, 0, 0, (int)gui->width, (int)gui->height,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowWindow(sdl_hwnd, SW_SHOW);
    }
#else
    /* Linux/macOS: show as floating for now (proper embedding is platform-specific) */
    SDL_ShowWindow(gui->window);
#endif

    /* Release GL context from this thread before starting render thread */
    SDL_GL_MakeCurrent(gui->window, NULL);

    gui->running = true;
    gui->visible = true;
    gui->render_thread = SDL_CreateThread(render_thread_func, "oxs_gui", gui);
}

void oxs_plugin_gui_detach(oxs_plugin_gui_t *gui)
{
    if (!gui || !gui->running) return;

    gui->running = false;
    if (gui->render_thread) {
        SDL_WaitThread(gui->render_thread, NULL);
        gui->render_thread = NULL;
    }

    if (gui->gl_ctx) {
        SDL_GL_DeleteContext(gui->gl_ctx);
        gui->gl_ctx = NULL;
    }
    if (gui->window) {
        SDL_DestroyWindow(gui->window);
        gui->window = NULL;
    }
}

void oxs_plugin_gui_set_visible(oxs_plugin_gui_t *gui, bool visible)
{
    if (!gui) return;
    gui->visible = visible;
    if (gui->window) {
        if (visible) SDL_ShowWindow(gui->window);
        else SDL_HideWindow(gui->window);
    }
}

void oxs_plugin_gui_get_size(oxs_plugin_gui_t *gui, uint32_t *width, uint32_t *height)
{
    if (gui) { *width = gui->width; *height = gui->height; }
    else { *width = 900; *height = 700; }
}

bool oxs_plugin_gui_set_size(oxs_plugin_gui_t *gui, uint32_t width, uint32_t height)
{
    if (!gui) return false;
    gui->width = width;
    gui->height = height;
    if (gui->window) {
        SDL_SetWindowSize(gui->window, (int)width, (int)height);
    }
    return true;
}

} /* extern "C" */
