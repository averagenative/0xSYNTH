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

#ifdef _WIN32
#include <windows.h>

/* Win32 WndProc subclass to capture keyboard + mouse events
 * that the DAW host doesn't forward to the child SDL window */
static WNDPROC s_orig_wndproc = NULL;
static bool s_mouse_pressed[3] = {};
static char s_char_buf[32] = {};
static int s_char_count = 0;

static LRESULT CALLBACK PluginWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_LBUTTONDOWN: s_mouse_pressed[0] = true; break;
    case WM_LBUTTONUP:   s_mouse_pressed[0] = false; break;
    case WM_RBUTTONDOWN: s_mouse_pressed[1] = true; break;
    case WM_RBUTTONUP:   s_mouse_pressed[1] = false; break;
    case WM_MBUTTONDOWN: s_mouse_pressed[2] = true; break;
    case WM_MBUTTONUP:   s_mouse_pressed[2] = false; break;
    case WM_CHAR:
        /* Consume to prevent Windows error beep */
        if (s_char_count < (int)sizeof(s_char_buf) - 1 && wp >= 32 && wp < 127) {
            s_char_buf[s_char_count++] = (char)wp;
        }
        return 0; /* consume — don't pass to DefWindowProc (prevents beep) */
    case WM_KEYDOWN:
        if (wp == VK_BACK) {
            s_char_buf[s_char_count++] = '\b';
        }
        return 0; /* consume */
    case WM_KEYUP:
        return 0; /* consume */
    case WM_SYSCHAR:
        return 0; /* consume Alt+key beeps too */
    default: break;
    }
    return CallWindowProcW(s_orig_wndproc, hwnd, msg, wp, lp);
}
#endif

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

            /* Grab keyboard focus when mouse enters our window */
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_ENTER) {
                SDL_SetWindowInputFocus(gui->window);
            }

            /* QWERTY keyboard input (only when not typing in text fields) */
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

        /* Track host window resize */
#ifdef _WIN32
        {
            SDL_SysWMinfo wi;
            SDL_VERSION(&wi.version);
            if (SDL_GetWindowWMInfo(gui->window, &wi)) {
                HWND parent = GetParent(wi.info.win.window);
                if (parent) {
                    RECT rc;
                    GetClientRect(parent, &rc);
                    int pw = rc.right - rc.left;
                    int ph = rc.bottom - rc.top;
                    if (pw > 0 && ph > 0 &&
                        ((uint32_t)pw != gui->width || (uint32_t)ph != gui->height)) {
                        gui->width = (uint32_t)pw;
                        gui->height = (uint32_t)ph;
                        SDL_SetWindowSize(gui->window, pw, ph);
                        SetWindowPos(wi.info.win.window, NULL, 0, 0, pw, ph,
                                     SWP_NOZORDER | SWP_NOMOVE);
                    }
                }
            }
        }

        /* Poll keyboard via GetAsyncKeyState — bypasses DAW accelerators */
        ImGuiIO &char_io = ImGui::GetIO();
        {
            POINT cursor;
            GetCursorPos(&cursor);
            SDL_SysWMinfo wi;
            SDL_VERSION(&wi.version);
            HWND our_hwnd = NULL;
            if (SDL_GetWindowWMInfo(gui->window, &wi))
                our_hwnd = wi.info.win.window;

            HWND under_cursor = WindowFromPoint(cursor);
            bool we_have_focus = (under_cursor == our_hwnd ||
                                  IsChild(our_hwnd, under_cursor));

            static bool prev_state[256] = {};

            if (we_have_focus) {
                if (char_io.WantTextInput) {
                    /* Text input mode: poll printable keys and inject chars */
                    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

                    for (int vk = 'A'; vk <= 'Z'; vk++) {
                        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
                        if (down && !prev_state[vk]) {
                            char c = shift ? (char)vk : (char)(vk + 32);
                            char_io.AddInputCharacter((unsigned int)c);
                        }
                        prev_state[vk] = down;
                    }
                    for (int vk = '0'; vk <= '9'; vk++) {
                        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
                        if (down && !prev_state[vk]) {
                            char_io.AddInputCharacter((unsigned int)vk);
                        }
                        prev_state[vk] = down;
                    }
                    /* Space */
                    {
                        bool down = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                        if (down && !prev_state[VK_SPACE])
                            char_io.AddInputCharacter(' ');
                        prev_state[VK_SPACE] = down;
                    }
                    /* Backspace */
                    {
                        bool down = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;
                        if (down && !prev_state[VK_BACK]) {
                            char_io.AddKeyEvent(ImGuiKey_Backspace, true);
                            char_io.AddKeyEvent(ImGuiKey_Backspace, false);
                        }
                        prev_state[VK_BACK] = down;
                    }
                    /* Enter (confirm) */
                    {
                        bool down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
                        if (down && !prev_state[VK_RETURN]) {
                            char_io.AddKeyEvent(ImGuiKey_Enter, true);
                            char_io.AddKeyEvent(ImGuiKey_Enter, false);
                        }
                        prev_state[VK_RETURN] = down;
                    }
                    /* Common punctuation */
                    static const struct { int vk; char normal; char shifted; } punct[] = {
                        {VK_OEM_MINUS, '-', '_'}, {VK_OEM_PLUS, '=', '+'},
                        {VK_OEM_PERIOD, '.', '>'}, {VK_OEM_COMMA, ',', '<'},
                        {0, 0, 0}
                    };
                    for (int p = 0; punct[p].vk; p++) {
                        bool down = (GetAsyncKeyState(punct[p].vk) & 0x8000) != 0;
                        if (down && !prev_state[punct[p].vk])
                            char_io.AddInputCharacter(shift ? punct[p].shifted : punct[p].normal);
                        prev_state[punct[p].vk] = down;
                    }
                } else {
                    /* QWERTY piano mode */
                    static const struct { int vk; int sc; } keymap[] = {
                        {'Z', SDL_SCANCODE_Z}, {'S', SDL_SCANCODE_S}, {'X', SDL_SCANCODE_X},
                        {'D', SDL_SCANCODE_D}, {'C', SDL_SCANCODE_C}, {'V', SDL_SCANCODE_V},
                        {'G', SDL_SCANCODE_G}, {'B', SDL_SCANCODE_B}, {'H', SDL_SCANCODE_H},
                        {'N', SDL_SCANCODE_N}, {'J', SDL_SCANCODE_J}, {'M', SDL_SCANCODE_M},
                        {'Q', SDL_SCANCODE_Q}, {'2', SDL_SCANCODE_2}, {'W', SDL_SCANCODE_W},
                        {'3', SDL_SCANCODE_3}, {'E', SDL_SCANCODE_E}, {'R', SDL_SCANCODE_R},
                        {'5', SDL_SCANCODE_5}, {'T', SDL_SCANCODE_T}, {'6', SDL_SCANCODE_6},
                        {'Y', SDL_SCANCODE_Y}, {'7', SDL_SCANCODE_7}, {'U', SDL_SCANCODE_U},
                        {0, 0}
                    };
                    for (int k = 0; keymap[k].vk; k++) {
                        bool down = (GetAsyncKeyState(keymap[k].vk) & 0x8000) != 0;
                        if (down && !prev_state[keymap[k].vk])
                            oxs_imgui_qwerty_key(gui->synth, keymap[k].sc, true);
                        if (!down && prev_state[keymap[k].vk])
                            oxs_imgui_qwerty_key(gui->synth, keymap[k].sc, false);
                        prev_state[keymap[k].vk] = down;
                    }
                }
            }
        }
#endif

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        float kb_height = 110.0f;
        float content_h = (float)gui->height - kb_height;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)gui->width, content_h));
        ImGui::Begin("0xSYNTH", NULL, fullscreen_flags);
        oxs_imgui_render_synth_ui(gui->synth, (float)gui->width, content_h);
        ImGui::End();

        /* Virtual keyboard at bottom */
        ImGui::SetNextWindowPos(ImVec2(0, content_h));
        ImGui::SetNextWindowSize(ImVec2((float)gui->width, kb_height));
        ImGui::Begin("##plugin_keyboard", NULL, fullscreen_flags);
        oxs_imgui_render_keyboard(gui->synth);
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
#ifdef _WIN32
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

        /* Install WndProc hook for keyboard/mouse capture */
        s_orig_wndproc = (WNDPROC)SetWindowLongPtrW(
            sdl_hwnd, GWLP_WNDPROC, (LONG_PTR)PluginWndProc);
    }
#else
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

#ifdef _WIN32
    /* Restore original WndProc before destroying window */
    if (gui->window && s_orig_wndproc) {
        SDL_SysWMinfo wi;
        SDL_VERSION(&wi.version);
        if (SDL_GetWindowWMInfo(gui->window, &wi)) {
            SetWindowLongPtrW(wi.info.win.window, GWLP_WNDPROC, (LONG_PTR)s_orig_wndproc);
        }
        s_orig_wndproc = NULL;
    }
#endif

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
