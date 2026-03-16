/*
 * 0xSYNTH ImGui Application Implementation
 *
 * SDL2 window + OpenGL 3.3 + Dear ImGui.
 * Walks the UI layout tree and renders ImGui widgets.
 * Custom knobs via ImDrawList arc drawing.
 *
 * Features:
 *   - Borderless window with custom chrome (drag, minimize, maximize, close)
 *   - 7 built-in color themes
 *   - QWERTY keyboard mapping for virtual piano
 *   - Condensed 2-column modular layout
 *   - Settings popup with theme selector and keybind help
 */

#include "imgui_app.h"

extern "C" {
#include "../ui/ui_types.h"
#include "../engine/types.h"
}

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

/* ─── Theme System ──────────────────────────────────────────────────────── */

enum OxsTheme {
    THEME_DARK = 0,
    THEME_HACKER,
    THEME_MIDNIGHT,
    THEME_AMBER,
    THEME_VAPORWAVE,
    THEME_NEON,
    THEME_LIGHT,
    THEME_COUNT
};

static const char *theme_names[THEME_COUNT] = {
    "Dark", "Hacker", "Midnight", "Amber", "Vaporwave", "Neon", "Light"
};

static int g_current_theme = THEME_DARK;
static bool g_light_theme = false;

/* Accent color for the current theme (used by knob arcs, waveform, etc.) */
static ImU32 g_accent_color = IM_COL32(99, 150, 255, 255);
static ImVec4 g_accent_vec = ImVec4(0.39f, 0.59f, 1.0f, 1.0f);

static void apply_theme(int theme_id)
{
    g_current_theme = theme_id;
    g_light_theme = (theme_id == THEME_LIGHT);
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 2.0f;
    style.WindowPadding = ImVec2(4, 3);
    style.ItemSpacing = ImVec2(4, 2);
    style.ItemInnerSpacing = ImVec2(3, 2);
    style.FramePadding = ImVec2(4, 2);

    switch (theme_id) {
    default:
    case THEME_DARK:
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg]   = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_Header]     = ImVec4(0.15f, 0.15f, 0.20f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.22f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.25f, 0.28f, 0.40f, 1.0f);
        style.Colors[ImGuiCol_Button]     = ImVec4(0.18f, 0.20f, 0.28f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.30f, 0.42f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]    = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
        g_accent_color = IM_COL32(99, 150, 255, 255);
        g_accent_vec = ImVec4(0.39f, 0.59f, 1.0f, 1.0f);
        break;

    case THEME_HACKER:
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg]   = ImVec4(0.02f, 0.05f, 0.02f, 1.0f);
        style.Colors[ImGuiCol_Text]       = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_Header]     = ImVec4(0.05f, 0.15f, 0.05f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.08f, 0.25f, 0.08f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.10f, 0.35f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_Button]     = ImVec4(0.05f, 0.18f, 0.05f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.08f, 0.30f, 0.08f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]    = ImVec4(0.03f, 0.08f, 0.03f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
        g_accent_color = IM_COL32(0, 255, 0, 255);
        g_accent_vec = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        break;

    case THEME_MIDNIGHT:
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg]   = ImVec4(0.06f, 0.04f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_Text]       = ImVec4(0.85f, 0.80f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_Header]     = ImVec4(0.15f, 0.08f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.12f, 0.35f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.30f, 0.15f, 0.45f, 1.0f);
        style.Colors[ImGuiCol_Button]     = ImVec4(0.18f, 0.08f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.12f, 0.42f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]    = ImVec4(0.10f, 0.06f, 0.18f, 1.0f);
        g_accent_color = IM_COL32(0, 220, 220, 255);
        g_accent_vec = ImVec4(0.0f, 0.86f, 0.86f, 1.0f);
        break;

    case THEME_AMBER:
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg]   = ImVec4(0.08f, 0.05f, 0.02f, 1.0f);
        style.Colors[ImGuiCol_Text]       = ImVec4(1.0f, 0.75f, 0.20f, 1.0f);
        style.Colors[ImGuiCol_Header]     = ImVec4(0.18f, 0.10f, 0.02f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.16f, 0.04f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.38f, 0.22f, 0.06f, 1.0f);
        style.Colors[ImGuiCol_Button]     = ImVec4(0.22f, 0.12f, 0.02f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.20f, 0.04f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]    = ImVec4(0.12f, 0.07f, 0.02f, 1.0f);
        g_accent_color = IM_COL32(255, 180, 30, 255);
        g_accent_vec = ImVec4(1.0f, 0.71f, 0.12f, 1.0f);
        break;

    case THEME_VAPORWAVE:
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg]   = ImVec4(0.08f, 0.02f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_Text]       = ImVec4(0.95f, 0.80f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_Header]     = ImVec4(0.20f, 0.05f, 0.22f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.08f, 0.32f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.40f, 0.10f, 0.42f, 1.0f);
        style.Colors[ImGuiCol_Button]     = ImVec4(0.25f, 0.05f, 0.28f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.10f, 0.42f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]    = ImVec4(0.14f, 0.03f, 0.16f, 1.0f);
        g_accent_color = IM_COL32(255, 100, 200, 255);
        g_accent_vec = ImVec4(1.0f, 0.39f, 0.78f, 1.0f);
        break;

    case THEME_NEON:
        ImGui::StyleColorsDark();
        style.Colors[ImGuiCol_WindowBg]   = ImVec4(0.04f, 0.04f, 0.06f, 1.0f);
        style.Colors[ImGuiCol_Text]       = ImVec4(0.95f, 0.95f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_Header]     = ImVec4(0.15f, 0.05f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.08f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.35f, 0.12f, 0.42f, 1.0f);
        style.Colors[ImGuiCol_Button]     = ImVec4(0.20f, 0.03f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.05f, 0.42f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]    = ImVec4(0.08f, 0.04f, 0.10f, 1.0f);
        g_accent_color = IM_COL32(255, 0, 180, 255);
        g_accent_vec = ImVec4(1.0f, 0.0f, 0.71f, 1.0f);
        break;

    case THEME_LIGHT:
        ImGui::StyleColorsLight();
        style.Colors[ImGuiCol_Text]       = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        style.Colors[ImGuiCol_WindowBg]   = ImVec4(0.94f, 0.94f, 0.96f, 1.0f);
        style.Colors[ImGuiCol_Header]     = ImVec4(0.82f, 0.85f, 0.92f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.72f, 0.78f, 0.90f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.62f, 0.70f, 0.88f, 1.0f);
        style.Colors[ImGuiCol_Button]     = ImVec4(0.75f, 0.80f, 0.90f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.65f, 0.72f, 0.88f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]    = ImVec4(0.88f, 0.88f, 0.92f, 1.0f);
        g_accent_color = IM_COL32(50, 100, 220, 255);
        g_accent_vec = ImVec4(0.20f, 0.39f, 0.86f, 1.0f);
        break;
    }
}

/* ─── Custom Knob Widget ─────────────────────────────────────────────────── */

static bool ImGuiKnob(const char *label, float *value, float min, float max,
                      float radius = 20.0f)
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImVec2 pos = ImGui::GetCursorScreenPos();

    float normalized = (*value - min) / (max - min);
    if (normalized < 0) normalized = 0;
    if (normalized > 1) normalized = 1;

    float start_angle = (float)(0.75 * M_PI);
    float end_angle = (float)(2.25 * M_PI);
    float val_angle = start_angle + normalized * (end_angle - start_angle);

    /* Invisible button — wide enough for label text */
    float knob_w = radius * 2;
    ImVec2 label_size = ImGui::CalcTextSize(label);
    if (label_size.x + 4 > knob_w) knob_w = label_size.x + 4;

    /* Center the arc in the wider button */
    ImVec2 center(pos.x + knob_w * 0.5f, pos.y + radius);
    ImGui::InvisibleButton(label, ImVec2(knob_w, radius * 2 + 14));
    bool changed = false;

    if (ImGui::IsItemActive() && io.MouseDelta.y != 0) {
        float range = max - min;
        /* Shift+drag for fine adjustment (10x precision) */
        float speed = (io.KeyShift) ? 0.0005f : 0.005f;
        *value -= io.MouseDelta.y * range * speed;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        changed = true;
    }

    /* Double-click to reset to default (midpoint) */
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        *value = (min + max) * 0.5f;
        changed = true;
    }

    /* Track arc (dark) */
    draw->PathArcTo(center, radius - 3, start_angle, end_angle, 32);
    draw->PathStroke(IM_COL32(80, 80, 80, 255), 0, 2.5f);

    /* Value arc (accent color) */
    if (normalized > 0.001f) {
        draw->PathArcTo(center, radius - 3, start_angle, val_angle, 32);
        draw->PathStroke(g_accent_color, 0, 2.5f);
    }

    /* Indicator dot */
    float dot_x = center.x + (radius - 3) * cosf(val_angle);
    float dot_y = center.y + (radius - 3) * sinf(val_angle);
    draw->AddCircleFilled(ImVec2(dot_x, dot_y), 2.5f, IM_COL32(255, 255, 255, 255));

    /* Label below */
    ImU32 knob_text_col = g_light_theme ? IM_COL32(30, 30, 30, 255) : IM_COL32(200, 200, 200, 255);
    ImVec2 text_size = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(center.x - text_size.x * 0.5f, pos.y + radius * 2 + 1),
                  knob_text_col, label);

    /* Value text in center */
    char val_str[16];
    if (max - min > 100)
        snprintf(val_str, sizeof(val_str), "%.0f", *value);
    else
        snprintf(val_str, sizeof(val_str), "%.2f", *value);
    ImVec2 vsize = ImGui::CalcTextSize(val_str);
    draw->AddText(ImVec2(center.x - vsize.x * 0.5f, center.y - vsize.y * 0.5f),
                  knob_text_col, val_str);

    return changed;
}

/* ─── Envelope Display ───────────────────────────────────────────────────── */

static void ImGuiEnvelope(const char *label, float a, float d, float s, float r)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float max_w = ImGui::GetContentRegionAvail().x - 4;
    float w = 100, h = 40;
    if (w > max_w && max_w > 30) w = max_w;

    ImGui::InvisibleButton(label, ImVec2(w, h));

    float total = a + d + 0.3f + r;
    if (total < 0.01f) total = 0.01f;

    float x_a = pos.x + (a / total) * w;
    float x_d = x_a + (d / total) * w;
    float x_s = x_d + (0.3f / total) * w;
    float x_r = x_s + (r / total) * w;

    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(30, 30, 30, 255));

    draw->AddLine(ImVec2(pos.x, pos.y + h), ImVec2(x_a, pos.y), g_accent_color, 2);
    draw->AddLine(ImVec2(x_a, pos.y), ImVec2(x_d, pos.y + (1 - s) * h), g_accent_color, 2);
    draw->AddLine(ImVec2(x_d, pos.y + (1 - s) * h), ImVec2(x_s, pos.y + (1 - s) * h), g_accent_color, 2);
    draw->AddLine(ImVec2(x_s, pos.y + (1 - s) * h), ImVec2(x_r, pos.y + h), g_accent_color, 2);
}

/* ─── Level Meter ────────────────────────────────────────────────────────── */

static float g_peak_l = 0, g_peak_r = 0;

/* Global toolbar synth selector index (shared so randomize can reset it) */
static int g_tb_selected = -1;
static char g_tb_random_label[32] = "";

static void ImGuiMeter(oxs_synth_t *synth)
{
    oxs_output_event_t ev;
    while (oxs_synth_pop_output_event(synth, &ev)) {
        g_peak_l = ev.peak_l;
        g_peak_r = ev.peak_r;
    }
    g_peak_l *= 0.95f;
    g_peak_r *= 0.95f;

    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = 10, h = 70;

    ImGui::InvisibleButton("##meter", ImVec2(w * 2 + 4, h));

    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(20, 20, 20, 255));
    draw->AddRectFilled(ImVec2(pos.x + w + 4, pos.y), ImVec2(pos.x + w * 2 + 4, pos.y + h), IM_COL32(20, 20, 20, 255));

    float h_l = g_peak_l * h; if (h_l > h) h_l = h;
    float h_r = g_peak_r * h; if (h_r > h) h_r = h;

    draw->AddRectFilled(ImVec2(pos.x, pos.y + h - h_l), ImVec2(pos.x + w, pos.y + h), IM_COL32(30, 200, 80, 255));
    draw->AddRectFilled(ImVec2(pos.x + w + 4, pos.y + h - h_r), ImVec2(pos.x + w * 2 + 4, pos.y + h), IM_COL32(30, 200, 80, 255));
}

/* ─── QWERTY Keyboard Mapping ────────────────────────────────────────────── */

static int g_octave_offset = 0; /* -2 to +4, shared with keyboard widget */
#define QWERTY_BASE_NOTE (48 + g_octave_offset * 12)

struct QwertyKeyMap {
    SDL_Scancode scancode;
    int note_offset; /* relative to QWERTY_BASE_NOTE */
};

/* Lower octave: white keys on Z row, sharps on S row */
static const QwertyKeyMap g_qwerty_lower[] = {
    { SDL_SCANCODE_Z, 0  }, /* C  */
    { SDL_SCANCODE_S, 1  }, /* C# */
    { SDL_SCANCODE_X, 2  }, /* D  */
    { SDL_SCANCODE_D, 3  }, /* D# */
    { SDL_SCANCODE_C, 4  }, /* E  */
    { SDL_SCANCODE_V, 5  }, /* F  */
    { SDL_SCANCODE_G, 6  }, /* F# */
    { SDL_SCANCODE_B, 7  }, /* G  */
    { SDL_SCANCODE_H, 8  }, /* G# */
    { SDL_SCANCODE_N, 9  }, /* A  */
    { SDL_SCANCODE_J, 10 }, /* A# */
    { SDL_SCANCODE_M, 11 }, /* B  */
};
static const int g_qwerty_lower_count = sizeof(g_qwerty_lower) / sizeof(g_qwerty_lower[0]);

/* Upper octave: white keys on Q row, sharps on number row */
static const QwertyKeyMap g_qwerty_upper[] = {
    { SDL_SCANCODE_Q, 12 }, /* C  */
    { SDL_SCANCODE_2, 13 }, /* C# */
    { SDL_SCANCODE_W, 14 }, /* D  */
    { SDL_SCANCODE_3, 15 }, /* D# */
    { SDL_SCANCODE_E, 16 }, /* E  */
    { SDL_SCANCODE_R, 17 }, /* F  */
    { SDL_SCANCODE_5, 18 }, /* F# */
    { SDL_SCANCODE_T, 19 }, /* G  */
    { SDL_SCANCODE_6, 20 }, /* G# */
    { SDL_SCANCODE_Y, 21 }, /* A  */
    { SDL_SCANCODE_7, 22 }, /* A# */
    { SDL_SCANCODE_U, 23 }, /* B  */
};
static const int g_qwerty_upper_count = sizeof(g_qwerty_upper) / sizeof(g_qwerty_upper[0]);

static bool g_qwerty_key_state[128] = {};

static void qwerty_handle_key(oxs_synth_t *synth, SDL_Scancode sc, bool pressed)
{
    int note = -1;

    for (int i = 0; i < g_qwerty_lower_count; i++) {
        if (g_qwerty_lower[i].scancode == sc) {
            note = QWERTY_BASE_NOTE + g_qwerty_lower[i].note_offset;
            break;
        }
    }
    if (note < 0) {
        for (int i = 0; i < g_qwerty_upper_count; i++) {
            if (g_qwerty_upper[i].scancode == sc) {
                note = QWERTY_BASE_NOTE + g_qwerty_upper[i].note_offset;
                break;
            }
        }
    }

    if (note < 0 || note > 127) return;

    if (pressed && !g_qwerty_key_state[note]) {
        g_qwerty_key_state[note] = true;
        oxs_synth_note_on(synth, (uint8_t)note, 100, 0);
    } else if (!pressed && g_qwerty_key_state[note]) {
        g_qwerty_key_state[note] = false;
        oxs_synth_note_off(synth, (uint8_t)note, 0);
    }
}

/* ─── Layout Tree Walker ─────────────────────────────────────────────────── */

static void render_widget(const oxs_ui_widget_t *w, oxs_synth_t *synth)
{
    switch (w->type) {
    case OXS_UI_GROUP: {
        /* Push unique ID for each group to prevent conflicts */
        ImGui::PushID(w->label);

        if (w->label[0] != '\0' && strcmp(w->label, "0xSYNTH") != 0) {
            /* Check if this is an FM operator group — render compact 2-row */
            bool is_fm_op = (strncmp(w->label, "Operator", 8) == 0);

            if (ImGui::CollapsingHeader(w->label,
                    is_fm_op ? (ImGuiTreeNodeFlags)0 : ImGuiTreeNodeFlags_DefaultOpen)) {
                if (is_fm_op && w->num_children >= 7) {
                    /* Compact FM operator: top row = Ratio,Level,FB; bottom row = A,D,S,R */
                    for (int i = 0; i < 3 && i < w->num_children; i++) {
                        if (i > 0) ImGui::SameLine();
                        ImGui::PushID(i);
                        ImGui::BeginGroup();
                        render_widget(w->children[i], synth);
                        ImGui::EndGroup();
                        ImGui::PopID();
                    }
                    for (int i = 3; i < w->num_children; i++) {
                        if (i > 3) ImGui::SameLine();
                        ImGui::PushID(i);
                        ImGui::BeginGroup();
                        render_widget(w->children[i], synth);
                        ImGui::EndGroup();
                        ImGui::PopID();
                    }
                } else if (w->direction == OXS_UI_HORIZONTAL) {
                    for (int i = 0; i < w->num_children; i++) {
                        if (i > 0) ImGui::SameLine();
                        ImGui::PushID(i);
                        ImGui::BeginGroup();
                        render_widget(w->children[i], synth);
                        ImGui::EndGroup();
                        ImGui::PopID();
                    }
                } else {
                    for (int i = 0; i < w->num_children; i++) {
                        ImGui::PushID(i);
                        render_widget(w->children[i], synth);
                        ImGui::PopID();
                    }
                }
            }
        } else {
            /* Unnamed groups still respect direction */
            if (w->direction == OXS_UI_HORIZONTAL) {
                for (int i = 0; i < w->num_children; i++) {
                    if (i > 0) ImGui::SameLine();
                    ImGui::PushID(i);
                    ImGui::BeginGroup();
                    render_widget(w->children[i], synth);
                    ImGui::EndGroup();
                    ImGui::PopID();
                }
            } else {
                for (int i = 0; i < w->num_children; i++) {
                    ImGui::PushID(i);
                    render_widget(w->children[i], synth);
                    ImGui::PopID();
                }
            }
        }

        ImGui::PopID();
        break;
    }

    case OXS_UI_KNOB: {
        oxs_param_info_t info;
        if (oxs_synth_param_info(synth, (uint32_t)w->param_id, &info)) {
            float val = oxs_synth_get_param(synth, (uint32_t)w->param_id);

            /* For effect P0-P7 knobs, show descriptive name based on effect type */
            const char *display_label = w->label;
            const char *tooltip = info.name;
            char desc_label[24] = {};

            if (strncmp(w->label, "P", 1) == 0 && w->label[1] >= '0' && w->label[1] <= '7') {
                int pn = w->label[1] - '0';
                /* Determine which effect slot this belongs to */
                int32_t pid = w->param_id;
                int efx_type = -1;
                if (pid >= 123 && pid <= 130) efx_type = (int)oxs_synth_get_param(synth, 120);
                else if (pid >= 143 && pid <= 150) efx_type = (int)oxs_synth_get_param(synth, 140);
                else if (pid >= 163 && pid <= 170) efx_type = (int)oxs_synth_get_param(synth, 160);

                /* Effect-specific param names */
                static const char *efx_param_names[15][8] = {
                    /* NONE */    {"","","","","","","",""},
                    /* FILTER */  {"Cutoff","Reso","","","","","",""},
                    /* DELAY */   {"Time","Feedbk","","","","","",""},
                    /* REVERB */  {"Room","Damp","","","","","",""},
                    /* OVERDRIVE*/{"Drive","Tone","","","","","",""},
                    /* FUZZ */    {"Gain","Tone","","","","","",""},
                    /* CHORUS */  {"Rate","Depth","","","","","",""},
                    /* BITCRUSH */{"Bits","Dwnsmpl","","","","","",""},
                    /* COMPRESS */{"Thresh","Ratio","Atk","Rel","Makeup","","",""},
                    /* PHASER */  {"Rate","Depth","Feedbk","","","","",""},
                    /* FLANGER */ {"Rate","Depth","Feedbk","","","","",""},
                    /* TREMOLO */ {"Rate","Depth","Wave","","","","",""},
                    /* RINGMOD */ {"Freq","","","","","","",""},
                    /* TAPE */    {"Drive","Warmth","","","","","",""},
                    /* SHIMMER */ {"Decay","Shimmer","","","","","",""},
                };

                if (efx_type > 0 && efx_type < 15 && pn < 8 &&
                    efx_param_names[efx_type][pn][0]) {
                    display_label = efx_param_names[efx_type][pn];
                    snprintf(desc_label, sizeof(desc_label), "%s",
                             efx_param_names[efx_type][pn]);
                    tooltip = desc_label;
                }
            }

            if (ImGuiKnob(display_label, &val, info.min, info.max)) {
                oxs_synth_set_param(synth, (uint32_t)w->param_id, val);
            }

            /* Tooltip on hover */
            if (ImGui::IsItemHovered()) {
                char tip[128];
                if (info.units[0])
                    snprintf(tip, sizeof(tip), "%s: %.2f %s", tooltip, val, info.units);
                else
                    snprintf(tip, sizeof(tip), "%s: %.2f", tooltip, val);
                ImGui::SetTooltip("%s", tip);
            }
        }
        break;
    }

    case OXS_UI_DROPDOWN: {
        float val = oxs_synth_get_param(synth, (uint32_t)w->param_id);
        int cur = (int)val;
        if (cur < 0) cur = 0;
        if (cur >= w->num_options) cur = w->num_options - 1;
        const char *preview = (cur >= 0 && cur < w->num_options) ? w->options[cur].label : "?";

        /* Label to the left, then combo */
        ImGui::Text("%s", w->label);
        ImGui::SameLine();
        ImGui::PushItemWidth(120);
        char id[64];
        snprintf(id, sizeof(id), "##dd_%d", w->param_id);
        if (ImGui::BeginCombo(id, preview)) {
            for (int i = 0; i < w->num_options; i++) {
                bool selected = (i == cur);
                if (ImGui::Selectable(w->options[i].label, selected)) {
                    oxs_synth_set_param(synth, (uint32_t)w->param_id, (float)i);
                }
            }
            ImGui::EndCombo();
        }
        /* Scroll wheel on hover to change selection */
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0) {
                int delta = (wheel > 0) ? -1 : 1;
                int next = cur + delta;
                if (next < 0) next = w->num_options - 1; /* wrap */
                if (next >= w->num_options) next = 0;
                oxs_synth_set_param(synth, (uint32_t)w->param_id, (float)next);
            }
        }
        ImGui::PopItemWidth();
        break;
    }

    case OXS_UI_TOGGLE: {
        float val = oxs_synth_get_param(synth, (uint32_t)w->param_id);
        bool on = val > 0.5f;
        if (ImGui::Checkbox(w->label, &on)) {
            oxs_synth_set_param(synth, (uint32_t)w->param_id, on ? 1.0f : 0.0f);
        }
        break;
    }

    case OXS_UI_LABEL:
        ImGui::Text("%s", w->label);
        break;

    case OXS_UI_ENVELOPE: {
        float a = oxs_synth_get_param(synth, (uint32_t)w->env_attack_id);
        float d = oxs_synth_get_param(synth, (uint32_t)w->env_decay_id);
        float s = oxs_synth_get_param(synth, (uint32_t)w->env_sustain_id);
        float r = oxs_synth_get_param(synth, (uint32_t)w->env_release_id);
        char id[64];
        snprintf(id, sizeof(id), "##env_%s", w->label);
        ImGuiEnvelope(id, a, d, s, r);
        break;
    }

    case OXS_UI_METER:
        ImGuiMeter(synth);
        break;

    case OXS_UI_WAVEFORM: {
        /* Draw current oscillator waveform — clamp to available width */
        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float max_ww = ImGui::GetContentRegionAvail().x - 4;
        float ww = 100, wh = 40;
        if (ww > max_ww && max_ww > 20) ww = max_ww;
        ImGui::InvisibleButton("##waveform", ImVec2(ww, wh));
        draw->AddRectFilled(pos, ImVec2(pos.x + ww, pos.y + wh), IM_COL32(30, 30, 30, 255));

        int wave = (int)oxs_synth_get_param(synth, 10); /* OXS_PARAM_OSC1_WAVE */
        for (int x = 0; x < (int)ww; x++) {
            float phase = (float)x / ww;
            float val = 0;
            switch (wave) {
            case 0: val = 2.0f * phase - 1.0f; break; /* saw */
            case 1: val = (phase < 0.5f) ? 1.0f : -1.0f; break; /* square */
            case 2: /* triangle */
                if (phase < 0.25f) val = 4.0f * phase;
                else if (phase < 0.75f) val = 2.0f - 4.0f * phase;
                else val = 4.0f * phase - 4.0f;
                break;
            case 3: val = sinf(phase * 2.0f * (float)M_PI); break; /* sine */
            }
            float y = pos.y + wh * 0.5f - val * wh * 0.4f;
            float y_next = y;
            if (x < (int)ww - 1) {
                float phase2 = (float)(x + 1) / ww;
                float val2 = 0;
                switch (wave) {
                case 0: val2 = 2.0f * phase2 - 1.0f; break;
                case 1: val2 = (phase2 < 0.5f) ? 1.0f : -1.0f; break;
                case 2:
                    if (phase2 < 0.25f) val2 = 4.0f * phase2;
                    else if (phase2 < 0.75f) val2 = 2.0f - 4.0f * phase2;
                    else val2 = 4.0f * phase2 - 4.0f;
                    break;
                case 3: val2 = sinf(phase2 * 2.0f * (float)M_PI); break;
                }
                y_next = pos.y + wh * 0.5f - val2 * wh * 0.4f;
            }
            draw->AddLine(ImVec2(pos.x + x, y), ImVec2(pos.x + x + 1, y_next),
                          g_accent_color, 1.5f);
        }
        break;
    }

    case OXS_UI_PRESET_BROWSER: {
        /* Save As... button with popup for preset name */
        static char save_name[128] = "My Patch";
        static bool save_popup_open = false;

        if (ImGui::Button("Save As...")) {
            save_popup_open = true;
            ImGui::OpenPopup("Save Preset##save_popup");
        }

        if (ImGui::BeginPopupModal("Save Preset##save_popup", &save_popup_open,
                ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Preset Name:");
            ImGui::PushItemWidth(200);
            ImGui::InputText("##save_preset_name", save_name, sizeof(save_name));
            ImGui::PopItemWidth();

            if (ImGui::Button("Save", ImVec2(100, 0))) {
                const char *dir = oxs_synth_preset_user_dir();
                char path[512];
                snprintf(path, sizeof(path), "%s/%s.json", dir, save_name);
                oxs_synth_preset_save(synth, path, save_name, "User", "Custom");
                save_popup_open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                save_popup_open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        break;
    }

    case OXS_UI_KEYBOARD: {
        /* Virtual piano keyboard with octave selector */
        float key_w = 22, key_h = 70;
        int num_white = 22; /* 3 octaves + 1 */
        int start_note = 48 + g_octave_offset * 12;
        if (start_note < 0) start_note = 0;
        if (start_note > 96) start_note = 96;

        static const int white_notes[] = {0, 2, 4, 5, 7, 9, 11};
        static const int black_notes[] = {1, 3, -1, 6, 8, 10, -1};

        /* Octave selector buttons */
        if (ImGui::Button("<<##oct")) { if (g_octave_offset > -2) g_octave_offset--; }
        ImGui::SameLine();
        char oct_label[16];
        snprintf(oct_label, sizeof(oct_label), "C%d-C%d", 3 + g_octave_offset, 6 + g_octave_offset);
        ImGui::Text("%s", oct_label);
        ImGui::SameLine();
        if (ImGui::Button(">>##oct")) { if (g_octave_offset < 4) g_octave_offset++; }
        ImGui::SameLine();
        ImGui::TextDisabled("(Z-/ lower, Q-P upper)");

        /* Center the keyboard in the panel */
        float kb_total_w = key_w * num_white;
        float avail_w = ImGui::GetContentRegionAvail().x;
        if (kb_total_w < avail_w) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail_w - kb_total_w) * 0.5f);
        }

        /* Read pos AFTER centering offset */
        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("##keyboard", ImVec2(kb_total_w, key_h));
        ImGuiIO &io = ImGui::GetIO();

        /* Draw white keys */
        for (int i = 0; i < num_white; i++) {
            int octave = i / 7;
            int note_in_octave = white_notes[i % 7];
            int midi_note = start_note + octave * 12 + note_in_octave;

            ImVec2 key_pos(pos.x + i * key_w, pos.y);
            ImVec2 key_end(key_pos.x + key_w - 1, key_pos.y + key_h);

            bool hovered = ImGui::IsItemHovered() &&
                io.MousePos.x >= key_pos.x && io.MousePos.x < key_end.x &&
                io.MousePos.y >= key_pos.y && io.MousePos.y < key_end.y;

            /* Highlight if QWERTY key is held */
            bool qwerty_held = (midi_note < 128) && g_qwerty_key_state[midi_note];

            ImU32 col;
            if (hovered && io.MouseDown[0])
                col = g_accent_color;
            else if (qwerty_held)
                col = g_accent_color;
            else
                col = IM_COL32(220, 220, 225, 255); /* off-white */

            draw->AddRectFilled(key_pos, key_end, col, 3.0f); /* rounded */
            draw->AddRect(key_pos, key_end, IM_COL32(80, 80, 85, 255), 3.0f);

            /* C-note label */
            if (note_in_octave == 0 && key_w > 12) {
                char clabel[8];
                int oct_num = 3 + octave;
                snprintf(clabel, sizeof(clabel), "C%d", oct_num);
                draw->AddText(ImVec2(key_pos.x + 2, key_end.y - 14),
                              IM_COL32(80, 80, 90, 255), clabel);
            }

            /* Note on/off on click */
            static bool key_state[128] = {};
            if (hovered && io.MouseDown[0] && !key_state[midi_note]) {
                oxs_synth_note_on(synth, (uint8_t)midi_note, 100, 0);
                key_state[midi_note] = true;
            }
            if (key_state[midi_note] && (!io.MouseDown[0] || !hovered)) {
                oxs_synth_note_off(synth, (uint8_t)midi_note, 0);
                key_state[midi_note] = false;
            }
        }

        /* Draw black keys on top */
        for (int i = 0; i < num_white; i++) {
            int bn = black_notes[i % 7];
            if (bn < 0) continue;
            int octave = i / 7;
            int midi_note = start_note + octave * 12 + bn;

            float bk_w = key_w * 0.6f;
            float bk_h = key_h * 0.6f;
            ImVec2 bk_pos(pos.x + i * key_w + key_w * 0.7f, pos.y);
            ImVec2 bk_end(bk_pos.x + bk_w, bk_pos.y + bk_h);

            bool hovered = ImGui::IsItemHovered() &&
                io.MousePos.x >= bk_pos.x && io.MousePos.x < bk_end.x &&
                io.MousePos.y >= bk_pos.y && io.MousePos.y < bk_end.y;

            bool qwerty_held = (midi_note < 128) && g_qwerty_key_state[midi_note];

            ImU32 col;
            if (hovered && io.MouseDown[0])
                col = IM_COL32(60, 140, 220, 255);
            else if (qwerty_held)
                col = IM_COL32(60, 140, 220, 255);
            else
                col = IM_COL32(25, 25, 30, 255);

            draw->AddRectFilled(bk_pos, bk_end, col, 2.0f); /* rounded */
            draw->AddRect(bk_pos, bk_end, IM_COL32(15, 15, 18, 255), 2.0f);

            static bool bkey_state[128] = {};
            if (hovered && io.MouseDown[0] && !bkey_state[midi_note]) {
                oxs_synth_note_on(synth, (uint8_t)midi_note, 100, 0);
                bkey_state[midi_note] = true;
            }
            if (bkey_state[midi_note] && (!io.MouseDown[0] || !hovered)) {
                oxs_synth_note_off(synth, (uint8_t)midi_note, 0);
                bkey_state[midi_note] = false;
            }
        }
        break;
    }

    default:
        break;
    }
}

/* ─── Render layout children into 2-column modular layout ────────────────── */

static void render_layout_2col(const oxs_ui_widget_t *root, oxs_synth_t *synth)
{
    if (!root) return;

    /* Collect top-level groups (sections) for 2-column layout */
    /* Non-group widgets or the keyboard get rendered inline */
    int section_count = 0;
    const oxs_ui_widget_t *sections[OXS_UI_MAX_CHILDREN];
    const oxs_ui_widget_t *keyboard_widget = NULL;

    /* Get current synth mode for section filtering */
    int synth_mode = (int)oxs_synth_get_param(synth, 1); /* OXS_PARAM_SYNTH_MODE */

    for (int i = 0; i < root->num_children; i++) {
        const oxs_ui_widget_t *child = root->children[i];
        if (child->type == OXS_UI_KEYBOARD) {
            keyboard_widget = child;
            continue;
        }

        /* Hide sections irrelevant to current synth mode */
        const char *lbl = child->label;
        if (synth_mode != 0 && strcmp(lbl, "Oscillator") == 0) continue; /* sub only */
        if (synth_mode != 1 && strcmp(lbl, "FM Synthesis") == 0) continue; /* FM only */
        if (synth_mode != 2 && strcmp(lbl, "Wavetable") == 0) continue; /* WT only */
        /* Filter only for subtractive and wavetable */
        if (synth_mode == 1 && strcmp(lbl, "Filter") == 0) continue;

        sections[section_count++] = child;
    }

    /* Render sections in 2 columns using ImGui::Columns for independent heights */
    if (section_count > 0) {
        float avail_w = ImGui::GetContentRegionAvail().x;

        /* Split: first half of sections in left column, rest in right */
        int left_count = (section_count + 1) / 2;

        ImGui::Columns(2, "##modular", true);
        ImGui::SetColumnWidth(0, avail_w * 0.5f);

        /* Left column */
        for (int i = 0; i < left_count && i < section_count; i++) {
            ImGui::PushID(i);
            render_widget(sections[i], synth);
            ImGui::PopID();
        }

        ImGui::NextColumn();

        /* Right column */
        for (int i = left_count; i < section_count; i++) {
            ImGui::PushID(i);
            render_widget(sections[i], synth);
            ImGui::PopID();
        }

        ImGui::Columns(1);
    }

    /* Keyboard is NOT rendered here — it's handled separately at the bottom */
    (void)keyboard_widget;
}

/* ─── Find keyboard widget in layout tree ────────────────────────────────── */

static const oxs_ui_widget_t *find_keyboard_widget(const oxs_ui_widget_t *w)
{
    if (!w) return NULL;
    if (w->type == OXS_UI_KEYBOARD) return w;
    for (int i = 0; i < w->num_children; i++) {
        const oxs_ui_widget_t *found = find_keyboard_widget(w->children[i]);
        if (found) return found;
    }
    return NULL;
}

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    /* Apply default theme */
    apply_theme(THEME_DARK);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    /* Build layout tree */
    const oxs_ui_layout_t *layout = oxs_ui_build_layout();
    const oxs_ui_widget_t *keyboard_widget = layout ? find_keyboard_widget(layout->root) : NULL;

    /* State */
    bool running = true;
    bool show_keyboard = true;
    bool show_settings = false;
    static ImVec2 gear_screen_pos_global = ImVec2(0,0);
    /* (maximize state queried live via is_window_maximized()) */

    /* Title bar drag state */
    bool dragging_title = false;
    int drag_offset_x = 0, drag_offset_y = 0;

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
                        g_current_theme = (g_current_theme + 1) % THEME_COUNT;
                        apply_theme(g_current_theme);
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_K &&
                               (event.key.keysym.mod & KMOD_CTRL)) {
                        /* Ctrl+K: toggle virtual keyboard */
                        show_keyboard = !show_keyboard;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_LEFT) {
                        /* Left arrow: octave down */
                        if (g_octave_offset > -2) g_octave_offset--;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
                        /* Right arrow: octave up */
                        if (g_octave_offset < 4) g_octave_offset++;
                    } else {
                        qwerty_handle_key(synth, event.key.keysym.scancode, true);
                    }
                }
                if (event.type == SDL_KEYUP) {
                    qwerty_handle_key(synth, event.key.keysym.scancode, false);
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
            ImVec4 logo_col(
                g_accent_vec.x * glow,
                g_accent_vec.y * glow,
                g_accent_vec.z * glow,
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
        float keyboard_height = show_keyboard ? 90.0f : 0.0f;
        float content_height = (float)win_h - content_top - keyboard_height;

        ImGui::SetNextWindowPos(ImVec2(0, content_top));
        ImGui::SetNextWindowSize(ImVec2((float)win_w, content_height));
        ImGui::Begin("##content", NULL,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        /* Top toolbar */
        ImGui::SetWindowFontScale(1.2f);
        {
            /* Synth selector — inline in toolbar */
            {
                int &tb_selected = g_tb_selected;
                static char *tb_names[128];
                static int tb_count = -1;
                if (tb_count < 0) {
                    tb_count = oxs_synth_preset_list("presets/factory", tb_names, 128);
                    if (tb_count <= 0)
                        tb_count = oxs_synth_preset_list("../presets/factory", tb_names, 128);
                }
                const char *tb_preview;
                if (tb_selected >= 0 && tb_selected < tb_count)
                    tb_preview = tb_names[tb_selected];
                else if (tb_selected == -2 && g_tb_random_label[0])
                    tb_preview = g_tb_random_label;
                else
                    tb_preview = "Select Synth...";
                ImGui::PushItemWidth(200);
                if (ImGui::BeginCombo("##synth_select", tb_preview)) {
                    for (int i = 0; i < tb_count; i++) {
                        if (ImGui::Selectable(tb_names[i], tb_selected == i)) {
                            tb_selected = i;
                            char path[256];
                            snprintf(path, sizeof(path), "presets/factory/%s.json", tb_names[i]);
                            if (!oxs_synth_preset_load(synth, path)) {
                                snprintf(path, sizeof(path), "../presets/factory/%s.json", tb_names[i]);
                                oxs_synth_preset_load(synth, path);
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                /* Scroll wheel on synth selector */
                if (ImGui::IsItemHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
                    float wheel = ImGui::GetIO().MouseWheel;
                    if (wheel != 0 && tb_count > 0) {
                        int delta = (wheel > 0) ? -1 : 1;
                        tb_selected += delta;
                        if (tb_selected < 0) tb_selected = tb_count - 1;
                        if (tb_selected >= tb_count) tb_selected = 0;
                        char path[256];
                        snprintf(path, sizeof(path), "presets/factory/%s.json", tb_names[tb_selected]);
                        if (!oxs_synth_preset_load(synth, path)) {
                            snprintf(path, sizeof(path), "../presets/factory/%s.json", tb_names[tb_selected]);
                            oxs_synth_preset_load(synth, path);
                        }
                    }
                }
                ImGui::PopItemWidth();
            }

            ImGui::SameLine();
            static float dice_anim = 0.0f;

            if (dice_anim > 0.0f) {
                dice_anim -= io.DeltaTime;
                ImGui::Text("Rolling...");
                if (dice_anim <= 0.0f) {
                    oxs_synth_randomize(synth);
                    g_tb_selected = -2; /* special value = randomized */
                    int rmode = (int)oxs_synth_get_param(synth, 1); /* SYNTH_MODE */
                    const char *mode_names[] = {"Subtractive", "FM", "Wavetable"};
                    snprintf(g_tb_random_label, sizeof(g_tb_random_label),
                             "(Random - %s)", mode_names[rmode % 3]);
                }
            } else {
                if (ImGui::Button("Randomize")) {
                    dice_anim = 0.4f;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                oxs_synth_reset_to_default(synth);
            }

            ImGui::SameLine();
            if (ImGui::Button(show_keyboard ? "Keyboard [ON]" : "Keyboard [OFF]")) {
                show_keyboard = !show_keyboard;
            }

            ImGui::SameLine();
            /* Theme button — shows current theme name, click to cycle */
            {
                char theme_btn[32];
                snprintf(theme_btn, sizeof(theme_btn), "[%s]", theme_names[g_current_theme]);
                if (ImGui::Button(theme_btn)) {
                    g_current_theme = (g_current_theme + 1) % THEME_COUNT;
                    apply_theme(g_current_theme);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Theme (Ctrl+T)");
                }
            }

            /* Right-aligned: ? help + gear settings */
            ImGui::SetWindowFontScale(1.0f);

            float right_edge = ImGui::GetWindowContentRegionMax().x;
            float gear_sz = 24.0f;
            float help_w = 24.0f;
            float right_x = right_edge - gear_sz - help_w - 8;

            ImGui::SameLine(right_x);

            /* ? Help button — shows keyboard shortcuts as tooltip */
            ImGui::SetWindowFontScale(1.3f);
            if (ImGui::Button("?##help", ImVec2(28, 28))) {}
            ImGui::SetWindowFontScale(1.0f);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("KEYBOARD SHORTCUTS");
                ImGui::Separator();
                ImGui::Text("PIANO:");
                ImGui::Text("  Z-M, comma, period, /: Lower octave");
                ImGui::Text("  Q-P: Upper octave");
                ImGui::Text("  S,D,G,H,J,L,;: Lower sharps");
                ImGui::Text("  2,3,5,6,7,9,0: Upper sharps");
                ImGui::Separator();
                ImGui::Text("CONTROLS:");
                ImGui::Text("  Left/Right arrow: Shift octave");
                ImGui::Text("  Ctrl+T: Cycle theme");
                ImGui::Text("  Ctrl+K: Toggle keyboard");
                ImGui::Text("  Shift+Drag: Fine knob adjustment");
                ImGui::Text("  Double-click knob: Reset to center");
                ImGui::Text("  Scroll wheel on dropdown: Cycle values");
                ImGui::Text("  ESC: Close window");
                ImGui::EndTooltip();
            }

            ImGui::SameLine();

            /* Gear icon — far right */
            {
                ImVec2 gpos = ImGui::GetCursorScreenPos();
                gear_screen_pos_global = gpos;
                ImGui::InvisibleButton("##gear", ImVec2(gear_sz, gear_sz));
                if (ImGui::IsItemClicked()) {
                    show_settings = !show_settings;
                }
                ImDrawList *gdl = ImGui::GetWindowDrawList();
                ImVec2 gc(gpos.x + gear_sz*0.5f, gpos.y + gear_sz*0.5f);
                float gr = gear_sz * 0.32f;
                for (int t = 0; t < 8; t++) {
                    float a = (float)t * (float)M_PI * 2.0f / 8.0f;
                    float tr = gr + 4.0f;
                    gdl->AddCircleFilled(ImVec2(gc.x + tr*cosf(a), gc.y + tr*sinf(a)),
                                         3.0f, IM_COL32(200, 200, 200, 255));
                }
                gdl->AddCircleFilled(gc, gr, IM_COL32(160, 160, 160, 255));
                gdl->AddCircleFilled(gc, gr * 0.35f, IM_COL32(30, 30, 40, 255));
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Settings");
                }
            }

            ImGui::Separator();
        }

        /* Settings panel — anchored near gear icon, not at mouse position */
        if (show_settings) {
            ImGui::SetNextWindowPos(ImVec2(gear_screen_pos_global.x - 220, gear_screen_pos_global.y + 28),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(260, 0));
            ImGui::Begin("Settings##panel", &show_settings,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("SETTINGS");
            ImGui::Separator();

            ImGui::Text("Theme:");
            ImGui::PushItemWidth(200);
            if (ImGui::BeginCombo("##theme_select", theme_names[g_current_theme])) {
                for (int i = 0; i < THEME_COUNT; i++) {
                    bool selected = (i == g_current_theme);
                    if (ImGui::Selectable(theme_names[i], selected)) {
                        apply_theme(i);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::Separator();

            /* Close button — large, red with white text */
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::SetWindowFontScale(1.3f);
            float close_w = ImGui::GetContentRegionAvail().x;
            if (ImGui::Button("Close", ImVec2(close_w, 30))) {
                show_settings = false;
            }
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor(3);

            ImGui::End();
        }

        /* Render layout tree in 2-column modular layout */
        if (layout && layout->root) {
            render_layout_2col(layout->root, synth);
        }

        ImGui::End();

        /* ── Virtual Keyboard (fixed at bottom) ──────────────────────── */
        if (show_keyboard && keyboard_widget) {
            ImGui::SetNextWindowPos(ImVec2(0, (float)win_h - keyboard_height));
            ImGui::SetNextWindowSize(ImVec2((float)win_w, keyboard_height));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
            ImGui::Begin("##keyboard_panel", NULL,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoBringToFrontOnFocus);

            render_widget(keyboard_widget, synth);

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
