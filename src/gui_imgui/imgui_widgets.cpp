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
#include "../engine/recorder.h"
#include "../engine/audio_convert.h"
}

/* Param IDs (from params.h — can't include directly in C++ due to _Atomic) */
#define OXS_PARAM_PITCH_BEND       195
#define OXS_PARAM_PITCH_BEND_RANGE 196
#define OXS_PARAM_PITCH_BEND_SNAP  197
#define OXS_PARAM_LFO_BPM_SYNC    64
#define OXS_PARAM_ARP_ENABLED      200
#define OXS_PARAM_ARP_BPM          205
#define OXS_PARAM_SEQ_ENABLED      260
#define OXS_PARAM_SEQ_LENGTH       261
#define OXS_PARAM_SEQ_BPM          262
#define OXS_PARAM_SEQ_SWING        263
#define OXS_PARAM_SEQ_DIRECTION    264
#define OXS_PARAM_SEQ_GATE         265
#define OXS_SEQ_MAX_STEPS          32

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

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
        style.Colors[ImGuiCol_Header]     = ImVec4(0.14f, 0.16f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.24f, 0.38f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.25f, 0.30f, 0.48f, 1.0f);
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
        style.Colors[ImGuiCol_Header]     = ImVec4(0.05f, 0.20f, 0.05f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.08f, 0.30f, 0.08f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.10f, 0.40f, 0.10f, 1.0f);
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
        style.Colors[ImGuiCol_Header]     = ImVec4(0.18f, 0.10f, 0.30f, 1.0f);
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
        style.Colors[ImGuiCol_Header]     = ImVec4(0.22f, 0.14f, 0.04f, 1.0f);
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
        style.Colors[ImGuiCol_Header]     = ImVec4(0.25f, 0.08f, 0.28f, 1.0f);
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
        style.Colors[ImGuiCol_Header]     = ImVec4(0.20f, 0.08f, 0.24f, 1.0f);
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

/* ─── Interactive Envelope Display ────────────────────────────────────────── */

static void ImGuiEnvelope(const char *label, float a, float d, float s, float r,
                          oxs_synth_t *synth = NULL,
                          int32_t a_id = -1, int32_t d_id = -1,
                          int32_t s_id = -1, int32_t r_id = -1)
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

    /* Background */
    ImU32 bg = g_light_theme ? IM_COL32(210, 210, 215, 255) : IM_COL32(30, 30, 30, 255);
    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg);

    /* ADSR curve */
    draw->AddLine(ImVec2(pos.x, pos.y + h), ImVec2(x_a, pos.y), g_accent_color, 2);
    draw->AddLine(ImVec2(x_a, pos.y), ImVec2(x_d, pos.y + (1 - s) * h), g_accent_color, 2);
    draw->AddLine(ImVec2(x_d, pos.y + (1 - s) * h), ImVec2(x_s, pos.y + (1 - s) * h), g_accent_color, 2);
    draw->AddLine(ImVec2(x_s, pos.y + (1 - s) * h), ImVec2(x_r, pos.y + h), g_accent_color, 2);

    /* Draggable control points */
    if (synth && a_id >= 0 && ImGui::IsItemActive()) {
        ImGuiIO &io = ImGui::GetIO();
        float mx = io.MousePos.x;
        float my = io.MousePos.y;

        /* Determine which segment we're closest to */
        float dx_a = fabsf(mx - x_a);
        float dx_d = fabsf(mx - x_d);
        float dx_s = (mx >= x_d && mx <= x_s) ? 0.0f : 99999.0f;
        float dx_r = fabsf(mx - x_r);

        /* Find closest control point */
        static int dragging_segment = -1;
        if (ImGui::IsMouseClicked(0)) {
            float min_dist = dx_a;
            dragging_segment = 0; /* attack */
            if (dx_d < min_dist) { min_dist = dx_d; dragging_segment = 1; } /* decay */
            if (dx_s < min_dist) { min_dist = dx_s; dragging_segment = 2; } /* sustain */
            if (dx_r < min_dist) { dragging_segment = 3; } /* release */
        }

        if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
            float time_scale = total / w * 2.0f; /* mouse px to seconds */
            switch (dragging_segment) {
            case 0: { /* Attack — drag right = longer */
                float na = a + io.MouseDelta.x * time_scale;
                if (na < 0.001f) na = 0.001f;
                if (na > 10.0f) na = 10.0f;
                oxs_synth_set_param(synth, (uint32_t)a_id, na);
                break;
            }
            case 1: { /* Decay — drag right = longer */
                float nd = d + io.MouseDelta.x * time_scale;
                if (nd < 0.001f) nd = 0.001f;
                if (nd > 10.0f) nd = 10.0f;
                oxs_synth_set_param(synth, (uint32_t)d_id, nd);
                break;
            }
            case 2: { /* Sustain — drag up = higher level */
                float ns = s - io.MouseDelta.y / h;
                if (ns < 0.0f) ns = 0.0f;
                if (ns > 1.0f) ns = 1.0f;
                oxs_synth_set_param(synth, (uint32_t)s_id, ns);
                break;
            }
            case 3: { /* Release — drag right = longer */
                float nr = r + io.MouseDelta.x * time_scale;
                if (nr < 0.001f) nr = 0.001f;
                if (nr > 10.0f) nr = 10.0f;
                oxs_synth_set_param(synth, (uint32_t)r_id, nr);
                break;
            }
            }
        }
    }

    /* Tooltip */
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("A: %.3fs  D: %.3fs  S: %.2f  R: %.3fs\nDrag to adjust", a, d, s, r);
    }

    /* Draw control point dots */
    ImU32 dot_col = IM_COL32(255, 255, 255, 180);
    draw->AddCircleFilled(ImVec2(x_a, pos.y), 3, dot_col);
    draw->AddCircleFilled(ImVec2(x_d, pos.y + (1 - s) * h), 3, dot_col);
    draw->AddCircleFilled(ImVec2(x_s, pos.y + (1 - s) * h), 3, dot_col);
    draw->AddCircleFilled(ImVec2(x_r, pos.y + h), 3, dot_col);
}

/* ─── Level Meter ────────────────────────────────────────────────────────── */

static float g_peak_l = 0, g_peak_r = 0;

/* Global toolbar synth selector index (shared so randomize can reset it) */
static int g_tb_selected = -1;
static char g_tb_random_label[32] = "";
static int g_user_patch_selected = -1;

/* Recorder state (set by standalone app, NULL in plugin) */
static oxs_recorder_t *g_recorder = NULL;
static uint32_t g_rec_sample_rate = 44100;

/* Recording format settings */
static int g_rec_format = OXS_REC_FORMAT_WAV; /* 0=WAV, 1=FLAC, 2=MP3 */
static int g_rec_bit_depth = 24;              /* 16, 24, or 32 (WAV/FLAC) */
static int g_rec_mp3_bitrate = 192;           /* 128, 192, 256, 320 (MP3) */

/* Settings panel state (shared between toolbar and render functions) */
static bool g_show_settings = false;
static ImVec2 g_gear_screen_pos = ImVec2(0, 0);

/* Device info for Settings panel */
static int g_audio_device_count = 0;
static const char **g_audio_device_names = NULL;
static int g_audio_device_selected = 0;
static int g_midi_device_count = 0;
static const char **g_midi_device_names = NULL;
static int g_midi_device_selected = 0;

void oxs_imgui_set_recorder(void *recorder, uint32_t sample_rate)
{
    g_recorder = (oxs_recorder_t *)recorder;
    g_rec_sample_rate = sample_rate;
}

void oxs_imgui_set_device_info(int audio_count, const char **audio_names,
                                int midi_count, const char **midi_names)
{
    g_audio_device_count = audio_count;
    g_audio_device_names = audio_names;
    g_midi_device_count = midi_count;
    g_midi_device_names = midi_names;
}

/* MIDI note output callback (set by plugin for DAW recording) */
static oxs_note_output_cb g_note_output_cb = NULL;
static void *g_note_output_ctx = NULL;

void oxs_imgui_set_note_output(oxs_note_output_cb cb, void *ctx)
{
    g_note_output_cb = cb;
    g_note_output_ctx = ctx;
}

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
/* Track which scancodes are physically held (for re-triggering on octave change) */
static SDL_Scancode g_held_scancodes[32];
static int g_held_scancode_count = 0;

static void held_scancode_add(SDL_Scancode sc) {
    for (int i = 0; i < g_held_scancode_count; i++)
        if (g_held_scancodes[i] == sc) return;
    if (g_held_scancode_count < 32) g_held_scancodes[g_held_scancode_count++] = sc;
}
static void held_scancode_remove(SDL_Scancode sc) {
    for (int i = 0; i < g_held_scancode_count; i++) {
        if (g_held_scancodes[i] == sc) {
            g_held_scancodes[i] = g_held_scancodes[--g_held_scancode_count];
            return;
        }
    }
}

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
        held_scancode_add(sc);
        oxs_synth_note_on(synth, (uint8_t)note, 100, 0);
        if (g_note_output_cb)
            g_note_output_cb(g_note_output_ctx, (uint8_t)note, 100, true);
    } else if (!pressed && g_qwerty_key_state[note]) {
        g_qwerty_key_state[note] = false;
        held_scancode_remove(sc);
        oxs_synth_note_off(synth, (uint8_t)note, 0);
        if (g_note_output_cb)
            g_note_output_cb(g_note_output_ctx, (uint8_t)note, 0, false);
    }
}

/* Public QWERTY API */
void oxs_imgui_qwerty_key(oxs_synth_t *synth, int scancode, bool pressed)
{
    qwerty_handle_key(synth, (SDL_Scancode)scancode, pressed);
}

int oxs_imgui_get_octave_offset(void) { return g_octave_offset; }

/* Release all held QWERTY notes before changing octave */
static void release_all_qwerty_notes(oxs_synth_t *synth)
{
    for (int i = 0; i < 128; i++) {
        if (g_qwerty_key_state[i]) {
            oxs_synth_note_off(synth, (uint8_t)i, 0);
            g_qwerty_key_state[i] = false;
        }
    }
}

void oxs_imgui_set_octave_offset_with_synth(oxs_synth_t *synth, int offset)
{
    if (offset != g_octave_offset) {
        /* Save currently held scancodes */
        SDL_Scancode held[32];
        int held_count = g_held_scancode_count;
        for (int i = 0; i < held_count; i++) held[i] = g_held_scancodes[i];

        /* Release old notes */
        release_all_qwerty_notes(synth);

        /* Change octave */
        g_octave_offset = offset;

        /* Re-trigger held keys at new octave */
        for (int i = 0; i < held_count; i++) {
            qwerty_handle_key(synth, held[i], true);
        }
    }
}

void oxs_imgui_set_octave_offset(int offset) { g_octave_offset = offset; }
void oxs_imgui_set_pitch_arrow_held(bool held) { g_pitch_arrow_held = held; }

/* ─── Accent Collapsing Header ───────────────────────────────────────────── */

/* Draws a collapsing header with a colored left accent bar for visibility */
static bool AccentCollapsingHeader(const char *label, ImGuiTreeNodeFlags flags = 0)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = ImGui::GetFrameHeight();

    bool open = ImGui::CollapsingHeader(label, flags);

    /* Draw accent bar on the left edge */
    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImU32 bar_col = g_accent_color;
    /* Slightly dim the bar when collapsed */
    if (!open) {
        int r = (bar_col >> 0) & 0xFF;
        int g = (bar_col >> 8) & 0xFF;
        int b = (bar_col >> 16) & 0xFF;
        bar_col = IM_COL32(r * 3 / 4, g * 3 / 4, b * 3 / 4, 200);
    }
    draw->AddRectFilled(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + 3.0f, pos.y + height),
        bar_col);

    return open;
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

            if (AccentCollapsingHeader(w->label,
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

            /* Check if this param is currently learning */
            int32_t learning = oxs_synth_midi_learn_active(synth);
            bool is_learning = (learning == w->param_id);

            /* Draw learn indicator ring if active */
            if (is_learning) {
                ImDrawList *dl = ImGui::GetWindowDrawList();
                ImVec2 kpos = ImGui::GetCursorScreenPos();
                float t = (float)SDL_GetTicks() / 1000.0f;
                float pulse = 0.5f + 0.5f * sinf(t * 4.0f);
                ImU32 learn_col = IM_COL32(255, 200, 0, (int)(pulse * 255));
                dl->AddRect(ImVec2(kpos.x - 2, kpos.y - 2),
                            ImVec2(kpos.x + 42, kpos.y + 52),
                            learn_col, 4.0f, 0, 2.0f);
            }

            if (ImGuiKnob(display_label, &val, info.min, info.max)) {
                oxs_synth_set_param(synth, (uint32_t)w->param_id, val);
            }

            /* Right-click → MIDI learn */
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                if (is_learning) {
                    oxs_synth_midi_learn_cancel(synth);
                } else {
                    oxs_synth_midi_learn_start(synth, w->param_id);
                }
            }

            /* Tooltip on hover */
            if (ImGui::IsItemHovered()) {
                char tip[256];
                if (is_learning) {
                    snprintf(tip, sizeof(tip),
                             "MIDI LEARN: Move a knob/fader on your controller...\n"
                             "Right-click to cancel");
                } else if (w->param_id == OXS_PARAM_ARP_BPM) {
                    snprintf(tip, sizeof(tip),
                             "Arpeggiator tempo: %.0f BPM\n"
                             "Sets the clock speed for arp note patterns.\n"
                             "In a plugin, the DAW tempo is used instead.\n"
                             "Right-click: MIDI learn",
                             val);
                } else if (info.units[0]) {
                    snprintf(tip, sizeof(tip), "%s: %.2f %s\nRight-click: MIDI learn", tooltip, val, info.units);
                } else {
                    snprintf(tip, sizeof(tip), "%s: %.2f\nRight-click: MIDI learn", tooltip, val);
                }
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

        /* Label to the left, vertically aligned with combo */
        ImGui::AlignTextToFramePadding();
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

        /* Arp enable gets a glowing press-in button */
        if (w->param_id == OXS_PARAM_ARP_ENABLED) {
            /* Label before button, matching dropdown style ("Mode [v]") */
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", w->label);
            ImGui::SameLine();

            ImDrawList *draw = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float btn_w = 52.0f;
            float btn_h = ImGui::GetFrameHeight(); /* match dropdown height */
            float rounding = 3.0f;

            /* Clickable invisible button */
            if (ImGui::InvisibleButton("##arp_sw", ImVec2(btn_w, btn_h))) {
                on = !on;
                oxs_synth_set_param(synth, (uint32_t)w->param_id, on ? 1.0f : 0.0f);
            }

            /* Subtle red glow behind button when active */
            if (on) {
                float t = (float)SDL_GetTicks() / 1000.0f;
                float pulse = 0.15f + 0.10f * sinf(t * 2.0f);
                ImU32 glow_col = IM_COL32(255, 40, 40, (int)(pulse * 255));
                draw->AddRectFilled(
                    ImVec2(pos.x - 3, pos.y - 3),
                    ImVec2(pos.x + btn_w + 3, pos.y + btn_h + 3),
                    glow_col, rounding + 3);
            }

            /* Button body — pressed-in look when on */
            ImU32 bg_col = on ? IM_COL32(180, 30, 30, 255) : IM_COL32(60, 60, 65, 255);
            ImU32 border_col = on ? IM_COL32(220, 60, 60, 255) : IM_COL32(90, 90, 95, 255);
            draw->AddRectFilled(pos, ImVec2(pos.x + btn_w, pos.y + btn_h), bg_col, rounding);
            draw->AddRect(pos, ImVec2(pos.x + btn_w, pos.y + btn_h), border_col, rounding);

            /* Inset shadow when pressed */
            if (on) {
                draw->AddRectFilled(pos, ImVec2(pos.x + btn_w, pos.y + 2),
                                    IM_COL32(0, 0, 0, 80), rounding);
            }

            /* Label centered in button */
            const char *lbl = on ? "ON" : "OFF";
            ImVec2 tsz = ImGui::CalcTextSize(lbl);
            ImU32 txt_col = on ? IM_COL32(255, 220, 220, 255) : IM_COL32(160, 160, 160, 255);
            draw->AddText(ImVec2(pos.x + (btn_w - tsz.x) * 0.5f,
                                 pos.y + (btn_h - tsz.y) * 0.5f), txt_col, lbl);
        } else {
            if (ImGui::Checkbox(w->label, &on)) {
                oxs_synth_set_param(synth, (uint32_t)w->param_id, on ? 1.0f : 0.0f);
            }
        }

        /* Descriptive tooltips for specific toggles */
        if (ImGui::IsItemHovered()) {
            if (w->param_id == OXS_PARAM_LFO_BPM_SYNC) {
                ImGui::SetTooltip("Locks LFO rate to musical divisions of the BPM\n"
                                  "(1/1, 1/2, 1/4, 1/8, 1/16, 1/32).\n"
                                  "In a plugin, syncs to the DAW tempo.");
            } else if (w->param_id == OXS_PARAM_ARP_ENABLED) {
                ImGui::SetTooltip("Enable arpeggiator — held notes play back\n"
                                  "as a pattern (up, down, up-down, random).");
            }
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
        ImGuiEnvelope(id, a, d, s, r, synth,
                      w->env_attack_id, w->env_decay_id,
                      w->env_sustain_id, w->env_release_id);
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
            const char *u_preview = (g_user_patch_selected >= 0 && g_user_patch_selected < user_count
                                     && user_names[g_user_patch_selected])
                                    ? user_names[g_user_patch_selected] : "User Patches...";
            ImGui::PushItemWidth(150);
            if (ImGui::BeginCombo("##user_patches", u_preview)) {
                for (int i = 0; i < user_count; i++) {
                    if (!user_names[i]) continue; /* safety */
                    if (ImGui::Selectable(user_names[i], g_user_patch_selected == i)) {
                        g_user_patch_selected = i;
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
            static bool confirm_overwrite = false;

            if (!confirm_overwrite) {
                ImGui::Text("Preset Name:");
                ImGui::PushItemWidth(200);
                bool enter_pressed = ImGui::InputText("##save_preset_name", save_name,
                    sizeof(save_name), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();

                bool do_save = enter_pressed || ImGui::Button("Save", ImVec2(100, 0));

                if (do_save && save_name[0] != '\0') {
                    /* Sanitize */
                    for (char *p = save_name; *p; p++) {
                        if (*p == '/' || *p == '\\' || *p == '.' || *p == ':')
                            *p = '_';
                    }
                    if (save_name[0] == '\0') strncpy(save_name, "Untitled", sizeof(save_name));

                    /* Check if file exists */
                    const char *dir = oxs_synth_preset_user_dir();
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s.json", dir, save_name);
                    FILE *check = fopen(path, "r");
                    if (check) {
                        fclose(check);
                        confirm_overwrite = true; /* show overwrite prompt */
                    } else {
                        /* Save directly */
                        oxs_synth_preset_save(synth, path, save_name, "User", "Custom");
                        save_popup_open = false;
                        user_needs_refresh = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!enter_pressed) {
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                        save_popup_open = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            } else {
                /* Overwrite confirmation */
                ImGui::Text("'%s' already exists.", save_name);
                ImGui::Text("Overwrite?");
                if (ImGui::Button("Yes, Overwrite", ImVec2(140, 0))) {
                    const char *dir = oxs_synth_preset_user_dir();
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s.json", dir, save_name);
                    oxs_synth_preset_save(synth, path, save_name, "User", "Custom");
                    save_popup_open = false;
                    confirm_overwrite = false;
                    user_needs_refresh = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                    confirm_overwrite = false;
                }
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
        if (ImGui::Button("<<##oct")) {
            if (g_octave_offset > -2) oxs_imgui_set_octave_offset_with_synth(synth, g_octave_offset - 1);
        }
        ImGui::SameLine();
        char oct_label[16];
        snprintf(oct_label, sizeof(oct_label), "C%d-C%d", 3 + g_octave_offset, 6 + g_octave_offset);
        ImGui::Text("%s", oct_label);
        ImGui::SameLine();
        if (ImGui::Button(">>##oct")) {
            if (g_octave_offset < 4) oxs_imgui_set_octave_offset_with_synth(synth, g_octave_offset + 1);
        }

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
                if (g_note_output_cb)
                    g_note_output_cb(g_note_output_ctx, (uint8_t)midi_note, 100, true);
                key_state[midi_note] = true;
            }
            if (key_state[midi_note] && (!io.MouseDown[0] || !hovered)) {
                oxs_synth_note_off(synth, (uint8_t)midi_note, 0);
                if (g_note_output_cb)
                    g_note_output_cb(g_note_output_ctx, (uint8_t)midi_note, 0, false);
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
                if (g_note_output_cb)
                    g_note_output_cb(g_note_output_ctx, (uint8_t)midi_note, 100, true);
                bkey_state[midi_note] = true;
            }
            if (bkey_state[midi_note] && (!io.MouseDown[0] || !hovered)) {
                oxs_synth_note_off(synth, (uint8_t)midi_note, 0);
                if (g_note_output_cb)
                    g_note_output_cb(g_note_output_ctx, (uint8_t)midi_note, 0, false);
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

/* ─── Toolbar Render Function ────────────────────────────────────────────── */

void oxs_imgui_render_toolbar(oxs_synth_t *synth)
{
    ImGuiIO &io = ImGui::GetIO();

    ImGui::SetWindowFontScale(1.2f);
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
            g_user_patch_selected = -1; /* reset user patch selector */
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

    /* MIDI Learn indicator — flashing yellow when active */
    {
        int32_t learn_param = oxs_synth_midi_learn_active(synth);
        if (learn_param >= 0) {
            ImGui::SameLine();
            float t = (float)SDL_GetTicks() / 1000.0f;
            float pulse = 0.6f + 0.4f * sinf(t * 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f * pulse, 0.6f * pulse, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            if (ImGui::Button("MIDI LEARN")) {
                oxs_synth_midi_learn_cancel(synth);
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) {
                oxs_param_info_t li;
                if (oxs_synth_param_info(synth, (uint32_t)learn_param, &li)) {
                    ImGui::SetTooltip("Learning: %s\nMove a knob/fader on your MIDI controller...\nClick to cancel", li.name);
                }
            }
        }
    }

    /* REC button (standalone only — recorder must be set) */
    if (g_recorder) {
        ImGui::SameLine();
        bool is_rec = (g_recorder->state == OXS_REC_ACTIVE);

        /* Glow behind button when recording */
        if (is_rec) {
            ImVec2 gpos = ImGui::GetCursorScreenPos();
            float t = (float)SDL_GetTicks() / 1000.0f;
            float pulse = 0.15f + 0.10f * sinf(t * 2.0f);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(gpos.x - 2, gpos.y - 2),
                ImVec2(gpos.x + 52, gpos.y + 22),
                IM_COL32(255, 30, 30, (int)(pulse * 255)), 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.12f, 0.12f, 1.0f));
        }

        /* Build label with recording time */
        char rec_label[32];
        if (is_rec) {
            uint64_t secs = g_recorder->frames_written / g_rec_sample_rate;
            snprintf(rec_label, sizeof(rec_label), "REC %lu:%02lu###rec",
                     (unsigned long)(secs / 60), (unsigned long)(secs % 60));
        } else {
            snprintf(rec_label, sizeof(rec_label), "REC###rec");
        }

        if (ImGui::Button(rec_label)) {
            if (is_rec) {
                char finished_path[512];
                snprintf(finished_path, sizeof(finished_path), "%s", g_recorder->filepath);
                oxs_recorder_stop(g_recorder);

                /* Convert to target format if not WAV */
                if (g_rec_format == OXS_REC_FORMAT_FLAC) {
                    oxs_convert_wav_to_flac(finished_path, g_rec_bit_depth);
                } else if (g_rec_format == OXS_REC_FORMAT_MP3) {
                    oxs_convert_wav_to_mp3(finished_path, g_rec_mp3_bitrate);
                }
            } else {
                char rec_path[512];
                const char *rec_dir = oxs_recorder_output_dir();
                oxs_recorder_timestamp_filename(rec_dir, "0xsynth",
                                                rec_path, sizeof(rec_path));
                /* Always record to WAV internally (bit_depth for WAV output) */
                int wav_depth = (g_rec_format == OXS_REC_FORMAT_WAV) ? g_rec_bit_depth : 24;
                oxs_recorder_start(g_recorder, rec_path,
                                   g_rec_sample_rate, (uint32_t)wav_depth);
            }
        }
        if (is_rec) ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
            static const char *fmt_names[] = {"WAV", "FLAC", "MP3"};
            if (is_rec) {
                float secs = (float)g_recorder->frames_written / g_rec_sample_rate;
                ImGui::SetTooltip("Recording: %.1fs\n%s\nClick to stop",
                                  secs, g_recorder->filepath);
            } else {
                ImGui::SetTooltip("Record audio (%s)\nSaves to: %s",
                                  fmt_names[g_rec_format],
                                  oxs_recorder_output_dir());
            }
        }

        /* Handle error state */
        if (g_recorder->state == OXS_REC_ERROR) {
            g_recorder->state = OXS_REC_IDLE;
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
        g_gear_screen_pos = gpos;
        ImGui::InvisibleButton("##gear", ImVec2(gear_sz, gear_sz));
        if (ImGui::IsItemClicked()) {
            g_show_settings = !g_show_settings;
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

    /* Settings panel — anchored near gear icon, not at mouse position */
    if (g_show_settings) {
        ImGui::SetNextWindowPos(ImVec2(g_gear_screen_pos.x - 220, g_gear_screen_pos.y + 28),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(260, 0));
        ImGui::Begin("Settings##panel", &g_show_settings,
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

        /* Audio device selector */
        if (g_audio_device_count > 0 && g_audio_device_names) {
            ImGui::Separator();
            ImGui::Text("Audio Output:");
            ImGui::PushItemWidth(200);
            const char *audio_preview = (g_audio_device_selected >= 0 &&
                                          g_audio_device_selected < g_audio_device_count)
                                         ? g_audio_device_names[g_audio_device_selected]
                                         : "(default)";
            if (ImGui::BeginCombo("##audio_device", audio_preview)) {
                for (int i = 0; i < g_audio_device_count; i++) {
                    bool selected = (i == g_audio_device_selected);
                    if (ImGui::Selectable(g_audio_device_names[i], selected)) {
                        g_audio_device_selected = i;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(restart to apply)");
        }

        /* MIDI device selector */
        if (g_midi_device_count > 0 && g_midi_device_names) {
            ImGui::Separator();
            ImGui::Text("MIDI Input:");
            ImGui::PushItemWidth(200);
            const char *midi_preview = (g_midi_device_selected >= 0 &&
                                         g_midi_device_selected < g_midi_device_count)
                                        ? g_midi_device_names[g_midi_device_selected]
                                        : "(none)";
            if (ImGui::BeginCombo("##midi_device", midi_preview)) {
                for (int i = 0; i < g_midi_device_count; i++) {
                    bool selected = (i == g_midi_device_selected);
                    if (ImGui::Selectable(g_midi_device_names[i], selected)) {
                        g_midi_device_selected = i;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(restart to apply)");
        }

        /* MIDI Learn help */
        ImGui::Separator();
        ImGui::Text("MIDI Learn:");
        ImGui::TextWrapped("Right-click any knob, then move a knob or fader on your MIDI controller to map it.");
        {
            int32_t lp = oxs_synth_midi_learn_active(synth);
            if (lp >= 0) {
                oxs_param_info_t li;
                float t = (float)SDL_GetTicks() / 1000.0f;
                float pulse = 0.6f + 0.4f * sinf(t * 4.0f);
                ImGui::TextColored(ImVec4(1.0f * pulse, 0.8f * pulse, 0.0f, 1.0f),
                    "Waiting for CC...");
                if (oxs_synth_param_info(synth, (uint32_t)lp, &li)) {
                    ImGui::SameLine();
                    ImGui::Text("(%s)", li.name);
                }
                if (ImGui::Button("Cancel Learn")) {
                    oxs_synth_midi_learn_cancel(synth);
                }
            }
        }

        /* Recording settings */
        if (g_recorder) {
            ImGui::Separator();
            ImGui::Text("Recording:");

            ImGui::PushItemWidth(200);
            static const char *fmt_labels[] = {"WAV", "FLAC", "MP3"};
            if (ImGui::BeginCombo("##rec_fmt", fmt_labels[g_rec_format])) {
                for (int i = 0; i < OXS_REC_FORMAT_COUNT; i++) {
                    if (ImGui::Selectable(fmt_labels[i], i == g_rec_format))
                        g_rec_format = i;
                }
                ImGui::EndCombo();
            }

            if (g_rec_format == OXS_REC_FORMAT_WAV || g_rec_format == OXS_REC_FORMAT_FLAC) {
                static const char *depth_labels[] = {"16-bit", "24-bit", "32-bit float"};
                int depth_idx = (g_rec_bit_depth == 24) ? 1 : (g_rec_bit_depth == 32) ? 2 : 0;
                /* FLAC only supports 16/24 */
                int max_depth = (g_rec_format == OXS_REC_FORMAT_FLAC) ? 2 : 3;
                if (ImGui::BeginCombo("##rec_depth", depth_labels[depth_idx])) {
                    for (int i = 0; i < max_depth; i++) {
                        if (ImGui::Selectable(depth_labels[i], i == depth_idx)) {
                            g_rec_bit_depth = (i == 0) ? 16 : (i == 1) ? 24 : 32;
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            if (g_rec_format == OXS_REC_FORMAT_MP3) {
                static const char *br_labels[] = {"128 kbps", "192 kbps", "256 kbps", "320 kbps"};
                static const int br_values[] = {128, 192, 256, 320};
                int br_idx = 1;
                for (int i = 0; i < 4; i++) { if (br_values[i] == g_rec_mp3_bitrate) br_idx = i; }
                if (ImGui::BeginCombo("##rec_bitrate", br_labels[br_idx])) {
                    for (int i = 0; i < 4; i++) {
                        if (ImGui::Selectable(br_labels[i], i == br_idx))
                            g_rec_mp3_bitrate = br_values[i];
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::PopItemWidth();
        }

        ImGui::Separator();

        /* Close button — large, red with white text */
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::SetWindowFontScale(1.3f);
        float close_w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Close", ImVec2(close_w, 30))) {
            g_show_settings = false;
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor(3);

        ImGui::End();
    }
}

/* ─── Shared Synth UI Render Function ────────────────────────────────────── */

void oxs_imgui_render_synth_ui(oxs_synth_t *synth, float window_width, float window_height)
{
    (void)window_width;
    (void)window_height;

    static const oxs_ui_layout_t *layout = NULL;
    if (!layout) layout = oxs_ui_build_layout();

    /* Render layout tree in 2-column modular layout */
    if (layout && layout->root) {
        render_layout_2col(layout->root, synth);
    }

    /* ── Wavetable Import (only in wavetable mode) ───────────────────── */
    {
        int mode = (int)oxs_synth_get_param(synth, 1); /* SYNTH_MODE */
        if (mode == 2) {
            if (ImGui::Button("Load Wavetable (.wav)...")) {
#ifdef _WIN32
                /* Windows file open dialog */
                char filepath[512] = "";
                OPENFILENAMEA ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFilter = "WAV Files\0*.wav\0All Files\0*.*\0";
                ofn.lpstrFile = filepath;
                ofn.nMaxFile = sizeof(filepath);
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameA(&ofn)) {
                    int idx = oxs_synth_load_wavetable(synth, filepath, 0);
                    if (idx >= 0) {
                        /* Switch to the newly loaded bank */
                        oxs_synth_set_param(synth, 110, (float)idx); /* OXS_PARAM_WT_BANK */
                    }
                }
#endif
            }
            /* Show loaded user bank names */
            uint32_t bank_count = oxs_synth_wavetable_bank_count(synth);
            if (bank_count > 4) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%u user banks loaded)", bank_count - 4);
            }
        }
    }

    /* ── Mod Matrix Section ──────────────────────────────────────────── */
    if (AccentCollapsingHeader("Mod Matrix")) {
        static const char *src_names[] = {
            "None", "LFO 1", "LFO 2", "Amp Env", "Filter Env",
            "Mod Wheel", "Velocity", "Aftertouch", "Key Track",
            "Macro 1", "Macro 2", "Macro 3", "Macro 4", "LFO 3"
        };
        static const int src_count = 14;

        /* Build destination list from param registry */
        static const char *dst_labels[OXS_PARAM_SLOT_COUNT + 1];
        static char dst_label_buf[OXS_PARAM_SLOT_COUNT][32];
        static bool dst_init = false;
        if (!dst_init) {
            dst_labels[0] = "None";
            for (int i = 0; i < OXS_PARAM_SLOT_COUNT; i++) {
                oxs_param_info_t info;
                if (oxs_synth_param_info(synth, (uint32_t)i, &info)) {
                    snprintf(dst_label_buf[i], sizeof(dst_label_buf[i]), "%s", info.name);
                    dst_labels[i + 1] = dst_label_buf[i];
                } else {
                    dst_label_buf[i][0] = '\0';
                    dst_labels[i + 1] = NULL;
                }
            }
            dst_init = true;
        }

        /* Column header labels */
        float avail = ImGui::GetContentRegionAvail().x;
        float src_w = avail * 0.28f;
        float dst_w = avail * 0.40f;
        float depth_w = avail * 0.22f;
        float arrow_w = avail * 0.06f;

        /* Header row */
        ImGui::TextDisabled("Source");
        ImGui::SameLine(src_w + arrow_w);
        ImGui::TextDisabled("Destination");
        ImGui::SameLine(src_w + arrow_w + dst_w);
        ImGui::TextDisabled("Depth");

        for (int slot = 0; slot < 8; slot++) {
            uint32_t base = 210 + (uint32_t)slot * 3;
            int src = (int)oxs_synth_get_param(synth, base);
            int dst = (int)oxs_synth_get_param(synth, base + 1);
            float depth = oxs_synth_get_param(synth, base + 2);

            ImGui::PushID(slot);

            /* Source dropdown */
            if (src < 0) src = 0;
            if (src >= src_count) src = src_count - 1;
            ImGui::PushItemWidth(src_w);
            if (ImGui::BeginCombo("##src", src_names[src])) {
                for (int s = 0; s < src_count; s++) {
                    if (ImGui::Selectable(src_names[s], s == src))
                        oxs_synth_set_param(synth, base, (float)s);
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::TextDisabled("->");
            ImGui::SameLine();

            /* Destination dropdown */
            const char *cur_dst_name = "None";
            {
                oxs_param_info_t dinfo;
                if (dst >= 0 && dst < OXS_PARAM_SLOT_COUNT &&
                    oxs_synth_param_info(synth, (uint32_t)dst, &dinfo)) {
                    cur_dst_name = dst_label_buf[dst];
                }
            }

            ImGui::PushItemWidth(dst_w);
            if (ImGui::BeginCombo("##dst", cur_dst_name)) {
                for (int d = 0; d < OXS_PARAM_SLOT_COUNT; d++) {
                    if (!dst_labels[d + 1] || dst_labels[d + 1][0] == '\0')
                        continue;
                    if (ImGui::Selectable(dst_labels[d + 1], d == dst))
                        oxs_synth_set_param(synth, base + 1, (float)d);
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();

            /* Depth slider */
            ImGui::PushItemWidth(depth_w);
            if (ImGui::SliderFloat("##depth", &depth, -1.0f, 1.0f, "%.2f")) {
                oxs_synth_set_param(synth, base + 2, depth);
            }
            ImGui::PopItemWidth();

            ImGui::PopID();
        }
    }

    /* ── Step Sequencer ──────────────────────────────────────────────── */
    /* Step Sequencer — rainbow shimmer header, default open */
    {
        ImVec2 hdr_pos = ImGui::GetCursorScreenPos();
        float hdr_w = ImGui::GetContentRegionAvail().x;
        float hdr_h = ImGui::GetFrameHeight();

        /* Render the header first (so ImGui draws its background) */
        bool seq_open = AccentCollapsingHeader("Step Sequencer", ImGuiTreeNodeFlags_DefaultOpen);

        /* Now overlay the rainbow gradient ON TOP with additive blending look */
        ImDrawList *hdl = ImGui::GetWindowDrawList();
        float t = (float)SDL_GetTicks() / 1000.0f;

        for (float px = 0; px < hdr_w; px += 1.0f) {
            float hue = fmodf((px / hdr_w) * 1.2f + t * 0.1f, 1.0f);
            /* Shimmer: oscillate brightness per-pixel */
            float shimmer = 0.5f + 0.5f * sinf(px * 0.15f + t * 5.0f);
            float intensity = 0.25f + shimmer * 0.2f;

            /* HSV to RGB */
            float h6 = hue * 6.0f;
            float r1 = 0, g1 = 0, b1 = 0;
            float c2 = intensity;
            float x2 = c2 * (1.0f - fabsf(fmodf(h6, 2.0f) - 1.0f));
            if (h6 < 1)      { r1 = c2; g1 = x2; }
            else if (h6 < 2) { r1 = x2; g1 = c2; }
            else if (h6 < 3) { g1 = c2; b1 = x2; }
            else if (h6 < 4) { g1 = x2; b1 = c2; }
            else if (h6 < 5) { r1 = x2; b1 = c2; }
            else              { r1 = c2; b1 = x2; }

            ImU32 col = IM_COL32((int)(r1*255), (int)(g1*255), (int)(b1*255), 140);
            hdl->AddLine(ImVec2(hdr_pos.x + px, hdr_pos.y),
                         ImVec2(hdr_pos.x + px, hdr_pos.y + hdr_h), col);
        }

        /* Bright shimmer sparkle highlights */
        for (int sp = 0; sp < 6; sp++) {
            float sx = fmodf(t * 80.0f + sp * hdr_w / 6.0f, hdr_w);
            float spark = 0.5f + 0.5f * sinf(t * 8.0f + sp * 1.7f);
            if (spark > 0.85f) {
                hdl->AddCircleFilled(
                    ImVec2(hdr_pos.x + sx, hdr_pos.y + hdr_h * 0.5f),
                    2.0f, IM_COL32(255, 255, 255, (int)(spark * 180)));
            }
        }

        if (!seq_open) goto seq_end;
    }
    {
        /* Controls row */
        {
            float seq_en = oxs_synth_get_param(synth, OXS_PARAM_SEQ_ENABLED);
            bool seq_on = seq_en > 0.5f;

            /* Enable toggle (same press-in button style as arp) */
            ImDrawList *draw = ImGui::GetWindowDrawList();
            ImVec2 bpos = ImGui::GetCursorScreenPos();
            float btn_w = 52.0f, btn_h = ImGui::GetFrameHeight();
            float rounding = 3.0f;

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Enable");
            ImGui::SameLine();

            if (ImGui::InvisibleButton("##seq_sw", ImVec2(btn_w, btn_h))) {
                seq_on = !seq_on;
                oxs_synth_set_param(synth, OXS_PARAM_SEQ_ENABLED, seq_on ? 1.0f : 0.0f);
            }
            ImVec2 spos = ImGui::GetItemRectMin();
            if (seq_on) {
                float t = (float)SDL_GetTicks() / 1000.0f;
                float pulse = 0.15f + 0.10f * sinf(t * 2.0f);
                draw->AddRectFilled(ImVec2(spos.x-3,spos.y-3), ImVec2(spos.x+btn_w+3,spos.y+btn_h+3),
                    IM_COL32(40, 200, 255, (int)(pulse*255)), rounding+3);
            }
            ImU32 bg = seq_on ? IM_COL32(30, 120, 180, 255) : IM_COL32(60, 60, 65, 255);
            draw->AddRectFilled(spos, ImVec2(spos.x+btn_w, spos.y+btn_h), bg, rounding);
            const char *lbl = seq_on ? "ON" : "OFF";
            ImVec2 tsz = ImGui::CalcTextSize(lbl);
            ImU32 tcol = seq_on ? IM_COL32(255,255,255,255) : IM_COL32(160,160,160,255);
            draw->AddText(ImVec2(spos.x+(btn_w-tsz.x)*0.5f, spos.y+(btn_h-tsz.y)*0.5f), tcol, lbl);

            /* Length, Direction, BPM, Swing, Gate */
            ImGui::SameLine(0, 12);
            static const char *len_names[] = {"8", "16", "32"};
            int seq_len_idx = (int)oxs_synth_get_param(synth, OXS_PARAM_SEQ_LENGTH);
            if (seq_len_idx < 0) seq_len_idx = 0;
            if (seq_len_idx > 2) seq_len_idx = 2;
            ImGui::PushItemWidth(50);
            ImGui::Text("Steps");
            ImGui::SameLine();
            if (ImGui::BeginCombo("##seq_len", len_names[seq_len_idx])) {
                for (int i = 0; i < 3; i++)
                    if (ImGui::Selectable(len_names[i], i == seq_len_idx))
                        oxs_synth_set_param(synth, OXS_PARAM_SEQ_LENGTH, (float)i);
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine(0, 8);
            static const char *dir_names[] = {"Fwd", "Rev", "PingPong", "Random"};
            int seq_dir = (int)oxs_synth_get_param(synth, OXS_PARAM_SEQ_DIRECTION);
            if (seq_dir < 0) seq_dir = 0; if (seq_dir > 3) seq_dir = 3;
            ImGui::PushItemWidth(80);
            if (ImGui::BeginCombo("##seq_dir", dir_names[seq_dir])) {
                for (int i = 0; i < 4; i++)
                    if (ImGui::Selectable(dir_names[i], i == seq_dir))
                        oxs_synth_set_param(synth, OXS_PARAM_SEQ_DIRECTION, (float)i);
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine(0, 8);
            float seq_bpm = oxs_synth_get_param(synth, OXS_PARAM_SEQ_BPM);
            ImGui::PushItemWidth(60);
            if (ImGui::DragFloat("BPM##seq", &seq_bpm, 1.0f, 20, 300, "%.0f"))
                oxs_synth_set_param(synth, OXS_PARAM_SEQ_BPM, seq_bpm);
            ImGui::PopItemWidth();

            ImGui::SameLine(0, 8);
            float seq_swing = oxs_synth_get_param(synth, OXS_PARAM_SEQ_SWING);
            ImGui::PushItemWidth(50);
            int swing_pct = (int)(seq_swing * 100);
            if (ImGui::SliderInt("Swing##seq", &swing_pct, 0, 100, "%d%%"))
                oxs_synth_set_param(synth, OXS_PARAM_SEQ_SWING, (float)swing_pct / 100.0f);
            ImGui::PopItemWidth();

            /* Randomize sequence button */
            ImGui::SameLine(0, 8);
            if (ImGui::Button("Dice##seq")) {
                int li = (int)oxs_synth_get_param(synth, OXS_PARAM_SEQ_LENGTH);
                int ns = (li == 0) ? 8 : (li == 1) ? 16 : 32;

                /* Random scale — pick a root note and scale */
                static const int scales[][8] = {
                    {0,2,4,5,7,9,11,-1},  /* major */
                    {0,2,3,5,7,8,10,-1},  /* natural minor */
                    {0,3,5,7,10,-1,-1,-1},/* minor pentatonic */
                    {0,2,4,7,9,-1,-1,-1}, /* major pentatonic */
                    {0,3,6,7,10,-1,-1,-1},/* blues (no b5 for simplicity) */
                };
                int scale_idx = rand() % 5;
                int root = 36 + (rand() % 24); /* C2-B3 range */

                /* Count notes in chosen scale */
                int scale_len = 0;
                while (scale_len < 8 && scales[scale_idx][scale_len] >= 0) scale_len++;

                for (int s = 0; s < ns; s++) {
                    /* ~25% chance of rest */
                    bool rest = (rand() % 4) == 0;
                    uint8_t note = 0, vel = 0, slide = 0, accent = 0;
                    float gate = 0.5f;
                    if (!rest) {
                        int octave_offset = (rand() % 2) * 12; /* 0 or +1 octave */
                        int degree = rand() % scale_len;
                        note = (uint8_t)(root + scales[scale_idx][degree] + octave_offset);
                        if (note > 127) note = 127;
                        vel = (uint8_t)(60 + rand() % 68);  /* 60-127 */
                        slide = (rand() % 5) == 0 ? 1 : 0;  /* ~20% slide */
                        accent = (rand() % 4) == 0 ? 1 : 0; /* ~25% accent */
                        gate = 0.2f + (float)(rand() % 80) / 100.0f; /* 0.2-1.0 */
                    }
                    oxs_synth_seq_set_step(synth, s, note, vel, slide, accent, gate);
                }

                /* Randomize direction */
                oxs_synth_set_param(synth, OXS_PARAM_SEQ_DIRECTION, (float)(rand() % 4));

                /* Randomize BPM (45-200) */
                float bpm = 45.0f + (float)(rand() % 156);
                oxs_synth_set_param(synth, OXS_PARAM_SEQ_BPM, bpm);

                /* Randomize swing (0-65%) */
                float sw = (float)(rand() % 66) / 100.0f;
                oxs_synth_set_param(synth, OXS_PARAM_SEQ_SWING, sw);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Randomize pattern, direction, BPM, and swing");
        }

        /* Step grid */
        {
            int len_idx = (int)oxs_synth_get_param(synth, OXS_PARAM_SEQ_LENGTH);
            int num_steps = (len_idx == 0) ? 8 : (len_idx == 1) ? 16 : 32;
            int current = oxs_synth_seq_current_step(synth);
            bool seq_on = oxs_synth_get_param(synth, OXS_PARAM_SEQ_ENABLED) > 0.5f;

            float avail_w = ImGui::GetContentRegionAvail().x;
            float cell_w = avail_w / (float)num_steps - 2.0f;
            if (cell_w < 14) cell_w = 14;
            if (cell_w > 40) cell_w = 40;
            float cell_h = 60.0f;

            static const char *note_names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

            ImDrawList *draw = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();

            for (int s = 0; s < num_steps; s++) {
                uint8_t note, vel, slide, accent;
                float gate;
                oxs_synth_seq_get_step(synth, s, &note, &vel, &slide, &accent, &gate);

                float x = origin.x + s * (cell_w + 2);
                float y = origin.y;
                ImVec2 p0(x, y);
                ImVec2 p1(x + cell_w, y + cell_h);

                float t = (float)SDL_GetTicks() / 1000.0f;
                bool is_playhead = seq_on && s == current;

                /* Background */
                ImU32 bg_col;
                if (vel == 0) {
                    bg_col = IM_COL32(20, 20, 25, 255);
                } else {
                    /* Subtle gradient based on note pitch */
                    int hue_shift = (note % 12) * 8;
                    bg_col = IM_COL32(30 + hue_shift/3, 30, 40 + hue_shift/4, 255);
                }
                draw->AddRectFilled(p0, p1, bg_col, 3.0f);

                /* Velocity bar — gradient from dark to accent color */
                if (vel > 0) {
                    float vel_h = (float)vel / 127.0f * (cell_h - 16);
                    int r = (g_accent_color >> 0) & 0xFF;
                    int g = (g_accent_color >> 8) & 0xFF;
                    int b = (g_accent_color >> 16) & 0xFF;
                    if (accent) { r = 255; g = 100; b = 50; }
                    ImU32 top_col = IM_COL32(r/3, g/3, b/3, 120);
                    ImU32 bot_col = IM_COL32(r, g, b, 160);
                    draw->AddRectFilledMultiColor(
                        ImVec2(x + 1, y + cell_h - vel_h),
                        ImVec2(x + cell_w - 1, y + cell_h),
                        top_col, top_col, bot_col, bot_col);
                }

                /* Playhead glow */
                if (is_playhead) {
                    float pulse = 0.6f + 0.4f * sinf(t * 6.0f);
                    int ar = (g_accent_color >> 0) & 0xFF;
                    int ag = (g_accent_color >> 8) & 0xFF;
                    int ab = (g_accent_color >> 16) & 0xFF;
                    ImU32 glow = IM_COL32(ar, ag, ab, (int)(pulse * 100));
                    draw->AddRectFilled(ImVec2(x-2, y-2), ImVec2(x+cell_w+2, y+cell_h+2),
                                        glow, 4.0f);
                    /* Bright top edge */
                    draw->AddLine(ImVec2(x, y), ImVec2(x + cell_w, y),
                                  IM_COL32(ar, ag, ab, (int)(pulse * 200)), 2.0f);
                }

                /* Note name */
                if (vel > 0) {
                    char nlbl[8];
                    int oct = note / 12 - 1;
                    snprintf(nlbl, sizeof(nlbl), "%s%d", note_names[note % 12], oct);
                    ImVec2 nsz = ImGui::CalcTextSize(nlbl);
                    ImU32 txt_col = is_playhead ? IM_COL32(255, 255, 255, 255)
                                                : IM_COL32(200, 200, 210, 255);
                    draw->AddText(ImVec2(x + (cell_w - nsz.x) * 0.5f, y + 2), txt_col, nlbl);
                }

                /* Slide indicator — arrow connecting to next step */
                if (slide) {
                    draw->AddTriangleFilled(
                        ImVec2(x + cell_w - 2, y + cell_h - 8),
                        ImVec2(x + cell_w + 4, y + cell_h - 4),
                        ImVec2(x + cell_w - 2, y + cell_h),
                        IM_COL32(100, 255, 140, 200));
                }

                /* Accent marker — small diamond */
                if (accent) {
                    float cx = x + cell_w * 0.5f, cy = y + 15;
                    draw->AddQuadFilled(
                        ImVec2(cx, cy - 3), ImVec2(cx + 3, cy),
                        ImVec2(cx, cy + 3), ImVec2(cx - 3, cy),
                        IM_COL32(255, 120, 50, 255));
                }

                /* Border — brighter on playhead */
                ImU32 brd = is_playhead ? g_accent_color : IM_COL32(55, 55, 65, 255);
                draw->AddRect(p0, p1, brd, 3.0f);
            }

            /* Invisible button over entire grid for interaction */
            ImGui::InvisibleButton("##seq_grid",
                ImVec2(num_steps * (cell_w + 2), cell_h));

            /* Click/scroll handling — use AllowWhenBlockedByPopup to ensure hover works */
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
                /* Capture scroll wheel so parent window doesn't scroll */
                ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
                ImVec2 mpos = ImGui::GetIO().MousePos;
                int hovered_step = (int)((mpos.x - origin.x) / (cell_w + 2));
                if (hovered_step >= 0 && hovered_step < num_steps) {
                    uint8_t note, vel, slide, accent;
                    float gate;
                    oxs_synth_seq_get_step(synth, hovered_step, &note, &vel, &slide, &accent, &gate);

                    /* Left click: toggle rest/active */
                    if (ImGui::IsMouseClicked(0)) {
                        if (vel == 0) vel = 100; else vel = 0;
                        oxs_synth_seq_set_step(synth, hovered_step, note, vel, slide, accent, gate);
                    }

                    /* Right click: toggle slide */
                    if (ImGui::IsMouseClicked(1)) {
                        slide = slide ? 0 : 1;
                        oxs_synth_seq_set_step(synth, hovered_step, note, vel, slide, accent, gate);
                    }

                    /* Middle click: toggle accent */
                    if (ImGui::IsMouseClicked(2)) {
                        accent = accent ? 0 : 1;
                        oxs_synth_seq_set_step(synth, hovered_step, note, vel, slide, accent, gate);
                    }

                    /* Scroll wheel: change note */
                    float wheel = ImGui::GetIO().MouseWheel;
                    if (wheel != 0 && vel > 0) {
                        int new_note = (int)note + (wheel > 0 ? 1 : -1);
                        if (new_note < 0) new_note = 0;
                        if (new_note > 127) new_note = 127;
                        oxs_synth_seq_set_step(synth, hovered_step, (uint8_t)new_note, vel, slide, accent, gate);
                    }

                    /* Tooltip */
                    if (vel > 0) {
                        int oct = note / 12 - 1;
                        ImGui::SetTooltip("Step %d: %s%d vel:%d gate:%.0f%%\n%s%s\n"
                                          "Left-click: toggle  Scroll: pitch\n"
                                          "Right-click: slide  Middle: accent",
                                          hovered_step + 1, note_names[note % 12], oct,
                                          vel, gate * 100,
                                          slide ? "[SLIDE] " : "",
                                          accent ? "[ACCENT]" : "");
                    } else {
                        ImGui::SetTooltip("Step %d: REST\n"
                                          "Left-click: activate  Scroll: pitch",
                                          hovered_step + 1);
                    }
                }
            }
        }
    }

    seq_end:;
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

static bool g_is_plugin_context = false;

void oxs_imgui_set_plugin_mode(bool is_plugin)
{
    g_is_plugin_context = is_plugin;
}

void oxs_imgui_render_keyboard(oxs_synth_t *synth)
{
    static const oxs_ui_layout_t *layout = NULL;
    if (!layout) layout = oxs_ui_build_layout();

    /* In plugin context, show a subtle note that keyboard is for preview */
    if (g_is_plugin_context) {
        ImGui::TextDisabled("Preview keyboard (QWERTY) - use MIDI controller to record");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This keyboard is for auditioning sounds only.\n"
                              "Notes played here are not recorded by the DAW.\n"
                              "Use a MIDI keyboard or draw notes in the piano roll.");
        }
    }

    /* Render octave controls from the keyboard widget */
    if (layout && layout->root) {
        const oxs_ui_widget_t *kb = find_keyboard_widget(layout->root);
        if (kb) {
            render_widget(kb, synth);
        }
    }
}

/* ─── Session UI State ──────────────────────────────────────────────────── */

extern "C" {
#include "../engine/session.h"
}

void oxs_imgui_save_session_ui(void)
{
    oxs_session_ui_t ui = {};
    ui.theme_id = g_current_theme;
    ui.octave_offset = g_octave_offset;
    strncpy(ui.preset_name, "", sizeof(ui.preset_name)); /* TODO: track selected preset name */
    strncpy(ui.version, "0.1.0", sizeof(ui.version));
    /* window_x/y/w/h are set by the caller (imgui_app) before calling */
    oxs_session_ui_save(&ui, oxs_session_ui_path());
}

void oxs_imgui_load_session_ui(void)
{
    oxs_session_ui_t ui = {};
    if (oxs_session_ui_load(&ui, oxs_session_ui_path())) {
        if (ui.theme_id >= 0 && ui.theme_id < THEME_COUNT) {
            apply_theme(ui.theme_id);
        }
        g_octave_offset = ui.octave_offset;
    }
}

/* Extended save that includes window geometry from the caller */
void oxs_imgui_save_session_ui_full(int win_x, int win_y, int win_w, int win_h, bool kb_visible)
{
    oxs_session_ui_t ui = {};
    ui.theme_id = g_current_theme;
    ui.octave_offset = g_octave_offset;
    ui.window_x = win_x;
    ui.window_y = win_y;
    ui.window_w = win_w;
    ui.window_h = win_h;
    ui.keyboard_visible = kb_visible;
    strncpy(ui.version, "0.1.0", sizeof(ui.version));

    /* Grab selected preset name from toolbar state */
    if (g_tb_selected >= 0) {
        /* Preset name is in the toolbar dropdown — we store the index */
    }

    oxs_session_ui_save(&ui, oxs_session_ui_path());
}

/* Extended load that returns window geometry to the caller */
bool oxs_imgui_load_session_ui_full(int *win_x, int *win_y, int *win_w, int *win_h, bool *kb_visible)
{
    oxs_session_ui_t ui = {};
    if (!oxs_session_ui_load(&ui, oxs_session_ui_path()))
        return false;

    if (ui.theme_id >= 0 && ui.theme_id < THEME_COUNT) {
        apply_theme(ui.theme_id);
    }
    g_octave_offset = ui.octave_offset;

    if (win_x) *win_x = ui.window_x;
    if (win_y) *win_y = ui.window_y;
    if (win_w) *win_w = ui.window_w;
    if (win_h) *win_h = ui.window_h;
    if (kb_visible) *kb_visible = ui.keyboard_visible;

    return true;
}

/* ─── Oscilloscope (rendered as fixed panel, not scrollable) ─────────────── */

void oxs_imgui_render_scope(oxs_synth_t *synth)
{
    float scope_buf[512];
    uint32_t scope_n = oxs_synth_get_scope(synth, scope_buf, 512);
    if (scope_n == 0) return;

    float avail_w = ImGui::GetContentRegionAvail().x;
    float scope_h = 50.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    /* Background */
    draw->AddRectFilled(pos, ImVec2(pos.x + avail_w, pos.y + scope_h),
                        IM_COL32(10, 10, 15, 255));

    /* Center line */
    float cy = pos.y + scope_h * 0.5f;
    draw->AddLine(ImVec2(pos.x, cy), ImVec2(pos.x + avail_w, cy),
                  IM_COL32(40, 40, 50, 255));

    /* Grid */
    draw->AddLine(ImVec2(pos.x, pos.y + scope_h * 0.25f),
                  ImVec2(pos.x + avail_w, pos.y + scope_h * 0.25f),
                  IM_COL32(30, 30, 40, 255));
    draw->AddLine(ImVec2(pos.x, pos.y + scope_h * 0.75f),
                  ImVec2(pos.x + avail_w, pos.y + scope_h * 0.75f),
                  IM_COL32(30, 30, 40, 255));

    /* Waveform */
    float step = (float)scope_n / avail_w;
    ImVec2 prev(pos.x, cy);
    for (float x = 0; x < avail_w; x += 1.0f) {
        int idx = (int)(x * step);
        if (idx >= (int)scope_n) idx = (int)scope_n - 1;
        float v = scope_buf[idx];
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        float y = cy - v * (scope_h * 0.45f);
        ImVec2 cur(pos.x + x, y);
        if (x > 0) draw->AddLine(prev, cur, g_accent_color, 1.5f);
        prev = cur;
    }

    ImGui::Dummy(ImVec2(avail_w, scope_h));
}
