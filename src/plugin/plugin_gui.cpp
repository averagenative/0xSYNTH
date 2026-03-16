/*
 * 0xSYNTH Plugin GUI Implementation
 *
 * SDL2 window embedded inside DAW host window.
 * Uses the same shared ImGui rendering as the standalone app
 * via oxs_imgui_render_synth_ui() from imgui_widgets.cpp.
 *
 * Platform-specific embedding:
 * - Windows: SetParent(sdl_hwnd, host_hwnd) + WS_CHILD
 * - Linux: SDL_CreateWindowFrom() or XReparent (future)
 * - macOS: NSView embedding (future)
 */

#include "plugin_gui.h"
#include "../gui_imgui/imgui_widgets.h"

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

static int render_thread_func(void *data)
{
    oxs_plugin_gui_t *gui = (oxs_plugin_gui_t *)data;

    SDL_GL_MakeCurrent(gui->window, gui->gl_ctx);

    /* ImGui setup on render thread */
    IMGUI_CHECKVERSION();
    gui->imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(gui->imgui_ctx);

    /* Apply the shared theme system */
    oxs_imgui_apply_theme(0); /* THEME_DARK */

    ImGui_ImplSDL2_InitForOpenGL(gui->window, gui->gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    ImGuiWindowFlags fullscreen_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    while (gui->running) {
        if (!gui->visible) {
            SDL_Delay(50);
            continue;
        }

        /* Process SDL events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            /* QWERTY keyboard input */
            ImGuiIO &io = ImGui::GetIO();
            if (!io.WantTextInput) {
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    oxs_imgui_qwerty_key(gui->synth, event.key.keysym.scancode, true);
                }
                if (event.type == SDL_KEYUP) {
                    oxs_imgui_qwerty_key(gui->synth, event.key.keysym.scancode, false);
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)gui->width, (float)gui->height));
        ImGui::Begin("0xSYNTH", NULL, fullscreen_flags);

        oxs_imgui_render_synth_ui(gui->synth, (float)gui->width, (float)gui->height);

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
