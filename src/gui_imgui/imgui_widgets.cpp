/*
 * 0xSYNTH Shared ImGui Widgets
 *
 * Extracted from imgui_app.cpp — all rendering code shared between
 * standalone and plugin contexts. Themes, custom knobs, envelopes,
 * meters, waveform display, preset browser, virtual keyboard,
 * toolbar, 2-column layout, and settings panel.
 */

#include "imgui_widgets.h"

extern "C" {
#include "../ui/ui_types.h"
#include "../engine/types.h"
}

/* Pitch bend param IDs (from params.h — can't include directly in C++ due to _Atomic) */
#define OXS_PARAM_PITCH_BEND       195
#define OXS_PARAM_PITCH_BEND_RANGE 196
#define OXS_PARAM_PITCH_BEND_SNAP  197

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
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

/* Public theme API */
void oxs_imgui_apply_theme(int theme_id) { apply_theme(theme_id); }
int oxs_imgui_get_theme(void) { return g_current_theme; }
const char* oxs_imgui_theme_name(int id) {
    if (id < 0 || id >= THEME_COUNT) return "?";
    return theme_names[id];
}
int oxs_imgui_theme_count(void) { return THEME_COUNT; }

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

    /* Filled circle background (0x808 style) */
    ImU32 knob_bg = g_light_theme ? IM_COL32(200, 200, 210, 255) : IM_COL32(50, 50, 55, 255);
    ImU32 knob_track = g_light_theme ? IM_COL32(170, 170, 180, 255) : IM_COL32(35, 35, 40, 255);
    draw->AddCircleFilled(center, radius + 1, knob_bg, 32);

    /* Track arc (full sweep) */
    draw->PathArcTo(center, radius - 2, start_angle, end_angle, 32);
    draw->PathStroke(knob_track, 0, 3.0f);

    /* Value arc (accent color) */
    if (normalized > 0.001f) {
        draw->PathArcTo(center, radius - 2, start_angle, val_angle, 32);
        draw->PathStroke(g_accent_color, 0, 3.0f);
    }

    /* Indicator line from center toward value position */
    ImU32 indicator_col = g_light_theme ? IM_COL32(40, 40, 50, 255) : IM_COL32(220, 220, 230, 255);
    {
        float inner_r = radius * 0.3f;
        float outer_r = radius * 0.7f;
        float ix = center.x + inner_r * cosf(val_angle);
        float iy = center.y + inner_r * sinf(val_angle);
        float ox = center.x + outer_r * cosf(val_angle);
        float oy = center.y + outer_r * sinf(val_angle);
        draw->AddLine(ImVec2(ix, iy), ImVec2(ox, oy), indicator_col, 2.0f);
    }

    /* Indicator dot at value position */
    {
        float dot_r = radius * 0.7f;
        float dx = center.x + dot_r * cosf(val_angle);
        float dy = center.y + dot_r * sinf(val_angle);
        draw->AddCircleFilled(ImVec2(dx, dy), 2.5f, indicator_col, 8);
    }

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
static bool g_pitch_arrow_held = false; /* true when up/down arrow is held */
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

/* Public QWERTY API */
void oxs_imgui_qwerty_key(oxs_synth_t *synth, int scancode, bool pressed)
{
    qwerty_handle_key(synth, (SDL_Scancode)scancode, pressed);
}

int oxs_imgui_get_octave_offset(void) { return g_octave_offset; }
void oxs_imgui_set_octave_offset(int offset) { g_octave_offset = offset; }
void oxs_imgui_set_pitch_arrow_held(bool held) { g_pitch_arrow_held = held; }

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
        /* User Patches dropdown + Save As button */
        static char save_name[128] = "My Patch";
        static bool save_popup_open = false;
        static char *user_names[128] = {};
        static int user_count = 0;
        static int user_selected = -1;
        static bool user_needs_refresh = true;

        /* Refresh user preset list */
        if (user_needs_refresh) {
            /* Free old names safely */
            for (int i = 0; i < 128; i++) {
                if (user_names[i]) { free(user_names[i]); user_names[i] = NULL; }
            }
            user_count = 0;
            const char *udir = oxs_synth_preset_user_dir();
            if (udir && udir[0]) {
                user_count = oxs_synth_preset_list(udir, user_names, 128);
                if (user_count < 0) user_count = 0;
            }
            user_needs_refresh = false;
        }

        /* User Patches dropdown */
        if (user_count > 0) {
            const char *u_preview = (user_selected >= 0 && user_selected < user_count
                                     && user_names[user_selected])
                                    ? user_names[user_selected] : "User Patches...";
            ImGui::PushItemWidth(150);
            if (ImGui::BeginCombo("##user_patches", u_preview)) {
                for (int i = 0; i < user_count; i++) {
                    if (!user_names[i]) continue; /* safety */
                    if (ImGui::Selectable(user_names[i], user_selected == i)) {
                        user_selected = i;
                        const char *udir = oxs_synth_preset_user_dir();
                        if (!udir) break;
                        char path[512];
                        snprintf(path, sizeof(path), "%s/%s.json", udir, user_names[i]);
                        /* Panic voices before loading to prevent audio thread crash */
                        oxs_synth_panic(synth);
                        oxs_synth_preset_load(synth, path);
                        g_tb_selected = -2;
                        snprintf(g_tb_random_label, sizeof(g_tb_random_label),
                                 "%s (User)", user_names[i]);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
        }

        /* Save As button */
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
                /* Sanitize: strip path separators to prevent directory traversal */
                for (char *p = save_name; *p; p++) {
                    if (*p == '/' || *p == '\\' || *p == '.' || *p == ':')
                        *p = '_';
                }
                if (save_name[0] == '\0') strncpy(save_name, "Untitled", sizeof(save_name));

                const char *dir = oxs_synth_preset_user_dir();
                char path[512];
                snprintf(path, sizeof(path), "%s/%s.json", dir, save_name);
                oxs_synth_preset_save(synth, path, save_name, "User", "Custom");
                save_popup_open = false;
                user_needs_refresh = true; /* refresh list to show new preset */
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

        /* Compute centering offset for the whole keyboard area */
        float whl_w = 26;
        float snap_w = 34;
        float kb_total_w = key_w * num_white;
        float total_w = snap_w + 4 + whl_w + 4 + whl_w + 4 + kb_total_w;
        float avail_w = ImGui::GetContentRegionAvail().x;
        float center_offset = 0;
        if (total_w < avail_w)
            center_offset = (avail_w - total_w) * 0.5f;

        /* Octave controls — aligned with Snap button left edge */
        float base_x = ImGui::GetCursorPosX() + center_offset;
        ImGui::SetCursorPosX(base_x);
        if (ImGui::Button("<<##oct")) { if (g_octave_offset > -2) g_octave_offset--; }
        ImGui::SameLine();
        char oct_label[16];
        snprintf(oct_label, sizeof(oct_label), "C%d-C%d", 3 + g_octave_offset, 6 + g_octave_offset);
        ImGui::Text("%s", oct_label);
        ImGui::SameLine();
        if (ImGui::Button(">>##oct")) { if (g_octave_offset < 4) g_octave_offset++; }

        /* [Snap] [PB wheel] [Mod wheel] [piano keys] — same offset */
        ImGui::SetCursorPosX(base_x);

        /* Helper: draw wheel texture (horizontal grip lines) */
        #define DRAW_WHEEL_TEXTURE(dl, x, y, w, h) do { \
            for (float ly = (y) + 4; ly < (y) + (h) - 2; ly += 4.0f) { \
                (dl)->AddLine(ImVec2((x) + 3, ly), ImVec2((x) + (w) - 3, ly), \
                              IM_COL32(60, 60, 65, 120), 1.0f); \
            } \
        } while(0)

        /* Snap/Hold button — directly left of PB wheel */
        {
            bool snap_mode = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND_SNAP) < 0.5f;
            ImGui::BeginGroup();
            if (ImGui::SmallButton(snap_mode ? "Snap" : "Hold")) {
                oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND_SNAP, snap_mode ? 1.0f : 0.0f);
            }
            ImGui::EndGroup();
        }
        ImGui::SameLine();

        /* Pitch Bend wheel */
        {
            float bend = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND);
            bool snap = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND_SNAP) < 0.5f;
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##pb_whl", ImVec2(whl_w, key_h));
            /* Background */
            dl->AddRectFilled(wp, ImVec2(wp.x + whl_w, wp.y + key_h),
                              IM_COL32(35, 35, 40, 255), 5.0f);
            /* Grip texture */
            DRAW_WHEEL_TEXTURE(dl, wp.x, wp.y, whl_w, key_h);
            /* Center line */
            float cy = wp.y + key_h * 0.5f;
            dl->AddLine(ImVec2(wp.x + 3, cy), ImVec2(wp.x + whl_w - 3, cy),
                        IM_COL32(100, 100, 105, 200), 1.5f);
            /* Thumb */
            float ty = cy - bend * (key_h * 0.42f);
            dl->AddRectFilled(ImVec2(wp.x + 1, ty - 4),
                              ImVec2(wp.x + whl_w - 1, ty + 4),
                              g_accent_color, 2.0f);
            /* Drag */
            if (ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.y != 0) {
                float nb = bend - ImGui::GetIO().MouseDelta.y / (key_h * 0.42f);
                if (nb < -1.0f) nb = -1.0f;
                if (nb > 1.0f) nb = 1.0f;
                oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND, nb);
            } else if (!ImGui::IsItemActive() && !g_pitch_arrow_held &&
                       snap && fabsf(bend) > 0.01f) {
                /* Snap decay when neither mouse nor arrow keys are active */
                float decay = bend * 0.88f;
                if (fabsf(decay) < 0.01f) decay = 0.0f;
                oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND, decay);
            }
            if (ImGui::IsItemHovered()) {
                float range = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND_RANGE);
                ImGui::SetTooltip("Pitch Bend: %.2f (±%.0f st)", bend, range);
            }
        }
        ImGui::SameLine();

        /* Modulation wheel */
        {
            float mod = oxs_synth_get_param(synth, 62); /* OXS_PARAM_LFO_DEPTH */
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##mod_whl", ImVec2(whl_w, key_h));
            /* Background */
            dl->AddRectFilled(wp, ImVec2(wp.x + whl_w, wp.y + key_h),
                              IM_COL32(35, 35, 40, 255), 5.0f);
            /* Grip texture */
            DRAW_WHEEL_TEXTURE(dl, wp.x, wp.y, whl_w, key_h);
            /* Thumb (0=bottom, 1=top) */
            float ty = wp.y + key_h * 0.95f - mod * key_h * 0.9f;
            dl->AddRectFilled(ImVec2(wp.x + 1, ty - 4),
                              ImVec2(wp.x + whl_w - 1, ty + 4),
                              g_accent_color, 2.0f);
            /* Drag */
            if (ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.y != 0) {
                float nm = mod - ImGui::GetIO().MouseDelta.y / (key_h * 0.9f);
                if (nm < 0.0f) nm = 0.0f;
                if (nm > 1.0f) nm = 1.0f;
                oxs_synth_set_param(synth, 62, nm);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Modulation (LFO Depth): %.2f", mod);
            }
        }
        ImGui::SameLine();

        #undef DRAW_WHEEL_TEXTURE

        /* Read pos AFTER wheels */
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
                int oct_num = 3 + g_octave_offset + octave;
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

/* ─── Shared Synth UI Render Function ────────────────────────────────────── */

void oxs_imgui_render_synth_ui(oxs_synth_t *synth, float window_width, float window_height)
{
    (void)window_width;
    (void)window_height;

    static const oxs_ui_layout_t *layout = NULL;
    if (!layout) layout = oxs_ui_build_layout();

    ImGuiIO &io = ImGui::GetIO();

    /* Settings panel state */
    static bool show_settings = false;
    static ImVec2 gear_screen_pos_global = ImVec2(0, 0);

    /* Top toolbar */
    ImGui::SetWindowFontScale(1.2f);
    {
        /* Synth selector — inline in toolbar */
        {
            int &tb_selected = g_tb_selected;
            static char *tb_names[128];
            static int tb_count = -1;
            static char g_factory_dir[512] = "";
            if (tb_count < 0) {
                /* Try multiple paths — CWD, relative, and next to executable */
                static const char *search_paths[] = {
                    "presets/factory",
                    "../presets/factory",
#ifdef _WIN32
                    "C:/Users/Dan Michael/Desktop/0xSYNTH/presets/factory",
#endif
                    NULL
                };
                for (int p = 0; search_paths[p] && tb_count <= 0; p++) {
                    tb_count = oxs_synth_preset_list(search_paths[p], tb_names, 128);
                    if (tb_count > 0) {
                        snprintf(g_factory_dir, sizeof(g_factory_dir), "%s", search_paths[p]);
                    }
                }
                /* Also try next to the module/exe via SDL */
                if (tb_count <= 0) {
                    char *base = SDL_GetBasePath();
                    if (base) {
                        char trypath[512];
                        snprintf(trypath, sizeof(trypath), "%spresets/factory", base);
                        tb_count = oxs_synth_preset_list(trypath, tb_names, 128);
                        if (tb_count > 0) snprintf(g_factory_dir, sizeof(g_factory_dir), "%s", trypath);
                        SDL_free(base);
                    }
                }
                if (tb_count < 0) tb_count = 0;
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
                        char path[512];
                        snprintf(path, sizeof(path), "%s/%s.json", g_factory_dir, tb_names[i]);
                        oxs_synth_panic(synth);
                        oxs_synth_preset_load(synth, path);
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
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s.json", g_factory_dir, tb_names[tb_selected]);
                    oxs_synth_panic(synth);
                    oxs_synth_preset_load(synth, path);
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
}

/* ─── Render keyboard widget (for standalone bottom panel) ───────────────── */

/* Pitch bend wheel widget */
static void render_pitch_bend_wheel(oxs_synth_t *synth)
{
    float bend = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND);
    bool snap = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND_SNAP) < 0.5f;

    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = 24, h = 70;

    ImGui::InvisibleButton("##pitchbend", ImVec2(w, h));

    /* Track background */
    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h),
                        IM_COL32(40, 40, 45, 255), 4.0f);

    /* Center line */
    float center_y = pos.y + h * 0.5f;
    draw->AddLine(ImVec2(pos.x + 4, center_y), ImVec2(pos.x + w - 4, center_y),
                  IM_COL32(80, 80, 85, 255), 1.0f);

    /* Thumb position */
    float thumb_y = center_y - bend * (h * 0.45f);
    draw->AddRectFilled(ImVec2(pos.x + 2, thumb_y - 5),
                        ImVec2(pos.x + w - 2, thumb_y + 5),
                        g_accent_color, 3.0f);

    /* Drag to bend */
    if (ImGui::IsItemActive()) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.MouseDelta.y != 0) {
            float new_bend = bend - io.MouseDelta.y / (h * 0.45f);
            if (new_bend < -1.0f) new_bend = -1.0f;
            if (new_bend > 1.0f) new_bend = 1.0f;
            oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND, new_bend);
        }
    } else if (snap && fabsf(bend) > 0.01f) {
        /* Snap back to center when released */
        float decay = bend * 0.85f;
        if (fabsf(decay) < 0.01f) decay = 0.0f;
        oxs_synth_set_param(synth, OXS_PARAM_PITCH_BEND, decay);
    }

    /* Label */
    ImVec2 tsz = ImGui::CalcTextSize("PB");
    draw->AddText(ImVec2(pos.x + (w - tsz.x) * 0.5f, pos.y + h + 1),
                  IM_COL32(150, 150, 150, 255), "PB");

    /* Tooltip */
    if (ImGui::IsItemHovered()) {
        float range = oxs_synth_get_param(synth, OXS_PARAM_PITCH_BEND_RANGE);
        ImGui::SetTooltip("Pitch Bend: %.2f (±%.0f st)\n%s mode",
                          bend, range, snap ? "Snap" : "Hold");
    }
}

void oxs_imgui_render_keyboard(oxs_synth_t *synth)
{
    static const oxs_ui_layout_t *layout = NULL;
    if (!layout) layout = oxs_ui_build_layout();

    /* Render octave controls from the keyboard widget */
    if (layout && layout->root) {
        const oxs_ui_widget_t *kb = find_keyboard_widget(layout->root);
        if (kb) {
            /* The keyboard widget renders octave buttons + keys.
             * We inject the PB wheel between octave controls and keys
             * by rendering the widget which handles everything. */
            render_widget(kb, synth);
        }
    }
}
