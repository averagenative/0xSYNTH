/*
 * 0xSYNTH ImGui Standalone Application
 *
 * SDL2 window + OpenGL 3.3 + Dear ImGui.
 * Borderless window with custom title bar (drag, minimize, maximize, close).
 * Delegates all synth rendering to imgui_widgets.cpp via oxs_imgui_render_synth_ui().
 */

#include "imgui_app.h"
#include "imgui_widgets.h"

extern "C" {
#include "../ui/ui_types.h"
#include "../engine/types.h"
}

/* Pitch bend param IDs */
#define OXS_PARAM_PITCH_BEND       195
#define OXS_PARAM_PITCH_BEND_SNAP  197

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_syswm.h>

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Win32 Borderless Window Support ────────────────────────────────────── */

#ifdef _WIN32
#define RESIZE_BORDER 8

static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK borderless_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCALCSIZE && wp == TRUE) {
        if (IsZoomed(hwnd)) {
            NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *)lp;
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfo(mon, &mi))
                params->rgrc[0] = mi.rcWork;
        }
        return 0;
    }

    if (msg == WM_NCHITTEST) {
        LRESULT hit = DefWindowProcW(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);

            int w = rc.right, h = rc.bottom;
            bool top    = pt.y < RESIZE_BORDER;
            bool bottom = pt.y >= h - RESIZE_BORDER;
            bool left   = pt.x < RESIZE_BORDER;
            bool right  = pt.x >= w - RESIZE_BORDER;

            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (top)             return HTTOP;
            if (bottom)          return HTBOTTOM;
            if (left)            return HTLEFT;
            if (right)           return HTRIGHT;
        } else {
            return hit;
        }
    }

    if (msg == WM_GETMINMAXINFO) {
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfo(mon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top  - mi.rcMonitor.top;
            mmi->ptMaxSize.x     = mi.rcWork.right  - mi.rcWork.left;
            mmi->ptMaxSize.y     = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }

    return CallWindowProcW(g_orig_wndproc, hwnd, msg, wp, lp);
}

static void install_borderless_wndproc(SDL_Window *window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        g_orig_wndproc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                                                      (LONG_PTR)borderless_wndproc);
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style |= WS_THICKFRAME | WS_CAPTION | WS_SYSMENU |
                 WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
}

static bool is_window_maximized(SDL_Window *window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo))
        return IsZoomed(wmInfo.info.win.window) != 0;
    return false;
}

static void toggle_maximize(SDL_Window *window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    }
}
#else
static void install_borderless_wndproc(SDL_Window *) {}
static bool is_window_maximized(SDL_Window *window) {
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
}
static void toggle_maximize(SDL_Window *window) {
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) SDL_RestoreWindow(window);
    else SDL_MaximizeWindow(window);
}
#endif

/* ─── Main Application Loop ──────────────────────────────────────────────── */

extern "C" int oxs_imgui_run(oxs_synth_t *synth, int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *window = SDL_CreateWindow(
        "0xSYNTH",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Install borderless wndproc for resize/maximize/snap on Windows */
    install_borderless_wndproc(window);
    SDL_SetWindowMinimumSize(window, 640, 480);

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1); /* vsync */

    /* ImGui setup */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    /* Don't enable keyboard nav — arrow keys are used for octave/pitch bend */
    /* io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; */

    /* Apply default theme */
    oxs_imgui_apply_theme(0); /* THEME_DARK */

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    /* State */
    bool running = true;
    bool show_keyboard = true;

    /* Title bar drag state */
    bool dragging_title = false;
    int drag_offset_x = 0, drag_offset_y = 0;
    (void)drag_offset_x; (void)drag_offset_y;

    static const float TITLE_BAR_HEIGHT = 30.0f;

    /* Main loop */
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE) {
                running = false;
            }

            /* QWERTY keyboard input (only when ImGui doesn't want keyboard) */
            if (!io.WantTextInput) {
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_T &&
                               (event.key.keysym.mod & KMOD_CTRL)) {
                        /* Ctrl+T: cycle theme */
                        int next = (oxs_imgui_get_theme() + 1) % oxs_imgui_theme_count();
                        oxs_imgui_apply_theme(next);
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_K &&
                               (event.key.keysym.mod & KMOD_CTRL)) {
                        /* Ctrl+K: toggle virtual keyboard */
                        show_keyboard = !show_keyboard;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_LEFT) {
                        int oct = oxs_imgui_get_octave_offset();
                        if (oct > -2) oxs_imgui_set_octave_offset_with_synth(synth, oct - 1);
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
                        int oct = oxs_imgui_get_octave_offset();
                        if (oct < 4) oxs_imgui_set_octave_offset_with_synth(synth, oct + 1);
                    } else {
                        oxs_imgui_qwerty_key(synth, event.key.keysym.scancode, true);
                    }
                }
                /* Arrow keys for pitch bend — allow repeats */
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_UP) {
                        oxs_imgui_set_pitch_arrow_held(true);
                        float bend = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND);
                        bend += 0.08f;
                        if (bend > 1.0f) bend = 1.0f;
                        oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND, bend);
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_DOWN) {
                        oxs_imgui_set_pitch_arrow_held(true);
                        float bend = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND);
                        bend -= 0.08f;
                        if (bend < -1.0f) bend = -1.0f;
                        oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND, bend);
                    }
                }
                if (event.type == SDL_KEYUP) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_UP ||
                        event.key.keysym.scancode == SDL_SCANCODE_DOWN) {
                        oxs_imgui_set_pitch_arrow_held(false);
                        /* Snap back on release in snap mode */
                        if (oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND_SNAP) < 0.5f) {
                            oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND, 0.0f);
                        }
                    }
                    oxs_imgui_qwerty_key(synth, event.key.keysym.scancode, false);
                }
            }

            /* Custom title bar dragging + double-click maximize */
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                int ww, wh;
                SDL_GetWindowSize(window, &ww, &wh);
                if (my < (int)TITLE_BAR_HEIGHT && mx < ww - 100) {
                    if (event.button.clicks == 2) {
                        /* Double-click title bar: toggle maximize */
                        toggle_maximize(window);
                    } else {
                        dragging_title = true;
                        drag_offset_x = mx;
                        drag_offset_y = my;
                    }
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                dragging_title = false;
            }
            if (event.type == SDL_MOUSEMOTION && dragging_title) {
                int wx, wy;
                SDL_GetWindowPosition(window, &wx, &wy);
                SDL_SetWindowPosition(window,
                    wx + event.motion.xrel,
                    wy + event.motion.yrel);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        /* Get window size */
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);

        /* ── Custom Title Bar ─────────────────────────────────────────── */
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)win_w, TITLE_BAR_HEIGHT));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
        ImGui::Begin("##titlebar", NULL,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        /* Logo with glow pulse */
        {
            float t = (float)SDL_GetTicks() / 1000.0f;
            float glow = 0.7f + 0.3f * sinf(t * 2.0f);
            /* Get accent color from theme for logo glow */
            ImGuiStyle &style = ImGui::GetStyle();
            ImVec4 accent = style.Colors[ImGuiCol_ButtonHovered];
            ImVec4 logo_col(
                accent.x * glow,
                accent.y * glow,
                accent.z * glow,
                1.0f
            );
            ImGui::PushStyleColor(ImGuiCol_Text, logo_col);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::Text("0xSYNTH");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        /* Window control buttons on the right */
        float btn_w = 28.0f;
        float btn_h = 20.0f;
        float right_x = (float)win_w - (btn_w + 4) * 3 - 8;

        ImGui::SameLine(right_x);
        if (ImGui::Button("_", ImVec2(btn_w, btn_h))) {
            SDL_MinimizeWindow(window);
        }

        ImGui::SameLine();
        {
            bool maxed = is_window_maximized(window);
            if (ImGui::Button(maxed ? "[]" : "[ ]", ImVec2(btn_w, btn_h))) {
                toggle_maximize(window);
            }
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("X", ImVec2(btn_w, btn_h))) {
            running = false;
        }
        ImGui::PopStyleColor(3);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        /* ── Main Content Area ────────────────────────────────────────── */
        float content_top = TITLE_BAR_HEIGHT;
        float keyboard_height = show_keyboard ? 115.0f : 0.0f;
        float content_height = (float)win_h - content_top - keyboard_height;

        ImGui::SetNextWindowPos(ImVec2(0, content_top));
        ImGui::SetNextWindowSize(ImVec2((float)win_w, content_height));
        ImGui::Begin("##content", NULL,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        /* Keyboard toggle button in the toolbar (injected before shared UI) */
        /* The shared UI renders toolbar + layout; we add keyboard toggle inline */
        oxs_imgui_render_synth_ui(synth, (float)win_w, content_height);

        ImGui::End();

        /* ── Virtual Keyboard (fixed at bottom) ──────────────────────── */
        if (show_keyboard) {
            ImGui::SetNextWindowPos(ImVec2(0, (float)win_h - keyboard_height));
            ImGui::SetNextWindowSize(ImVec2((float)win_w, keyboard_height));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
            ImGui::Begin("##keyboard_panel", NULL,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoBringToFrontOnFocus);

            oxs_imgui_render_keyboard(synth);

            ImGui::End();
            ImGui::PopStyleColor();
        }

        /* ── Render ───────────────────────────────────────────────────── */
        ImGui::Render();

        glViewport(0, 0, win_w, win_h);
        glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    /* Cleanup */
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
