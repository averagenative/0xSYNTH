/*
 * 0xSYNTH ImGui Application Implementation
 *
 * SDL2 window + OpenGL 3.3 + Dear ImGui.
 * Walks the UI layout tree and renders ImGui widgets.
 * Custom knobs via ImDrawList arc drawing.
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

#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Custom Knob Widget ─────────────────────────────────────────────────── */

static bool ImGuiKnob(const char *label, float *value, float min, float max,
                      float radius = 24.0f)
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center(pos.x + radius, pos.y + radius);

    float normalized = (*value - min) / (max - min);
    if (normalized < 0) normalized = 0;
    if (normalized > 1) normalized = 1;

    float start_angle = (float)(0.75 * M_PI);
    float end_angle = (float)(2.25 * M_PI);
    float val_angle = start_angle + normalized * (end_angle - start_angle);

    /* Invisible button for interaction */
    ImGui::InvisibleButton(label, ImVec2(radius * 2, radius * 2 + 16));
    bool changed = false;

    if (ImGui::IsItemActive() && io.MouseDelta.y != 0) {
        float range = max - min;
        *value -= io.MouseDelta.y * range * 0.005f;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        changed = true;
    }

    /* Track arc (dark) */
    draw->PathArcTo(center, radius - 4, start_angle, end_angle, 32);
    draw->PathStroke(IM_COL32(80, 80, 80, 255), 0, 3.0f);

    /* Value arc (bright) */
    if (normalized > 0.001f) {
        draw->PathArcTo(center, radius - 4, start_angle, val_angle, 32);
        draw->PathStroke(IM_COL32(50, 180, 230, 255), 0, 3.0f);
    }

    /* Indicator dot */
    float dot_x = center.x + (radius - 4) * cosf(val_angle);
    float dot_y = center.y + (radius - 4) * sinf(val_angle);
    draw->AddCircleFilled(ImVec2(dot_x, dot_y), 3, IM_COL32(255, 255, 255, 255));

    /* Label below */
    ImVec2 text_size = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(center.x - text_size.x * 0.5f, pos.y + radius * 2 + 2),
                  IM_COL32(200, 200, 200, 255), label);

    /* Value text in center */
    char val_str[16];
    if (max - min > 100)
        snprintf(val_str, sizeof(val_str), "%.0f", *value);
    else
        snprintf(val_str, sizeof(val_str), "%.2f", *value);
    ImVec2 vsize = ImGui::CalcTextSize(val_str);
    draw->AddText(ImVec2(center.x - vsize.x * 0.5f, center.y - vsize.y * 0.5f),
                  IM_COL32(200, 200, 200, 255), val_str);

    return changed;
}

/* ─── Envelope Display ───────────────────────────────────────────────────── */

static void ImGuiEnvelope(const char *label, float a, float d, float s, float r)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = 120, h = 50;

    ImGui::InvisibleButton(label, ImVec2(w, h));

    float total = a + d + 0.3f + r;
    if (total < 0.01f) total = 0.01f;

    float x_a = pos.x + (a / total) * w;
    float x_d = x_a + (d / total) * w;
    float x_s = x_d + (0.3f / total) * w;
    float x_r = x_s + (r / total) * w;

    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(30, 30, 30, 255));

    draw->AddLine(ImVec2(pos.x, pos.y + h), ImVec2(x_a, pos.y), IM_COL32(50, 180, 230, 255), 2);
    draw->AddLine(ImVec2(x_a, pos.y), ImVec2(x_d, pos.y + (1 - s) * h), IM_COL32(50, 180, 230, 255), 2);
    draw->AddLine(ImVec2(x_d, pos.y + (1 - s) * h), ImVec2(x_s, pos.y + (1 - s) * h), IM_COL32(50, 180, 230, 255), 2);
    draw->AddLine(ImVec2(x_s, pos.y + (1 - s) * h), ImVec2(x_r, pos.y + h), IM_COL32(50, 180, 230, 255), 2);
}

/* ─── Level Meter ────────────────────────────────────────────────────────── */

static float g_peak_l = 0, g_peak_r = 0;

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
    float w = 12, h = 80;

    ImGui::InvisibleButton("##meter", ImVec2(w * 2 + 4, h));

    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(20, 20, 20, 255));
    draw->AddRectFilled(ImVec2(pos.x + w + 4, pos.y), ImVec2(pos.x + w * 2 + 4, pos.y + h), IM_COL32(20, 20, 20, 255));

    float h_l = g_peak_l * h; if (h_l > h) h_l = h;
    float h_r = g_peak_r * h; if (h_r > h) h_r = h;

    draw->AddRectFilled(ImVec2(pos.x, pos.y + h - h_l), ImVec2(pos.x + w, pos.y + h), IM_COL32(30, 200, 80, 255));
    draw->AddRectFilled(ImVec2(pos.x + w + 4, pos.y + h - h_r), ImVec2(pos.x + w * 2 + 4, pos.y + h), IM_COL32(30, 200, 80, 255));
}

/* ─── Layout Tree Walker ─────────────────────────────────────────────────── */

static void render_widget(const oxs_ui_widget_t *w, oxs_synth_t *synth)
{
    switch (w->type) {
    case OXS_UI_GROUP: {
        if (w->label[0] != '\0' && strcmp(w->label, "0xSYNTH") != 0) {
            if (ImGui::CollapsingHeader(w->label, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (w->direction == OXS_UI_HORIZONTAL) {
                    for (int i = 0; i < w->num_children; i++) {
                        if (i > 0) ImGui::SameLine();
                        ImGui::BeginGroup();
                        render_widget(w->children[i], synth);
                        ImGui::EndGroup();
                    }
                } else {
                    for (int i = 0; i < w->num_children; i++)
                        render_widget(w->children[i], synth);
                }
            }
        } else {
            for (int i = 0; i < w->num_children; i++)
                render_widget(w->children[i], synth);
        }
        break;
    }

    case OXS_UI_KNOB: {
        oxs_param_info_t info;
        if (oxs_synth_param_info(synth, (uint32_t)w->param_id, &info)) {
            float val = oxs_synth_get_param(synth, (uint32_t)w->param_id);
            char id[64];
            snprintf(id, sizeof(id), "##knob_%d", w->param_id);
            if (ImGuiKnob(w->label, &val, info.min, info.max)) {
                oxs_synth_set_param(synth, (uint32_t)w->param_id, val);
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

        ImGui::PushItemWidth(100);
        char id[64];
        snprintf(id, sizeof(id), "%s##dd_%d", w->label, w->param_id);
        if (ImGui::BeginCombo(id, preview)) {
            for (int i = 0; i < w->num_options; i++) {
                bool selected = (i == cur);
                if (ImGui::Selectable(w->options[i].label, selected)) {
                    oxs_synth_set_param(synth, (uint32_t)w->param_id, (float)i);
                }
            }
            ImGui::EndCombo();
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
        /* Draw current oscillator waveform */
        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float ww = 120, wh = 50;
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
                          IM_COL32(50, 180, 230, 255), 1.5f);
        }
        break;
    }

    case OXS_UI_PRESET_BROWSER: {
        /* Inline preset browser */
        static int selected_preset = -1;
        static char *preset_names[128];
        static int preset_count = -1;

        /* Load preset list once */
        if (preset_count < 0) {
            preset_count = oxs_synth_preset_list("presets/factory", preset_names, 128);
            if (preset_count <= 0)
                preset_count = oxs_synth_preset_list("../presets/factory", preset_names, 128);
        }

        ImGui::Text("Presets (%d)", preset_count);
        ImGui::SameLine();
        if (ImGui::Button("Save User")) {
            const char *dir = oxs_synth_preset_user_dir();
            char path[512];
            snprintf(path, sizeof(path), "%s/User Patch.json", dir);
            oxs_synth_preset_save(synth, path, "User Patch", "User", "Custom");
        }

        if (ImGui::BeginListBox("##presets", ImVec2(200, 150))) {
            for (int i = 0; i < preset_count; i++) {
                bool is_selected = (selected_preset == i);
                if (ImGui::Selectable(preset_names[i], is_selected)) {
                    selected_preset = i;
                    char path[256];
                    snprintf(path, sizeof(path), "presets/factory/%s.json", preset_names[i]);
                    if (!oxs_synth_preset_load(synth, path)) {
                        snprintf(path, sizeof(path), "../presets/factory/%s.json", preset_names[i]);
                        oxs_synth_preset_load(synth, path);
                    }
                }
            }
            ImGui::EndListBox();
        }
        break;
    }

    case OXS_UI_KEYBOARD: {
        /* Virtual piano keyboard */
        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float key_w = 24, key_h = 80;
        int num_keys = 24; /* 2 octaves starting from C3 */
        int start_note = 48; /* C3 */

        /* White key pattern in one octave: C D E F G A B */
        static const int white_notes[] = {0, 2, 4, 5, 7, 9, 11};
        /* Black key offsets (relative to white key position) */
        static const int black_notes[] = {1, 3, -1, 6, 8, 10, -1};

        ImGui::InvisibleButton("##keyboard", ImVec2(key_w * num_keys, key_h));
        ImGuiIO &io = ImGui::GetIO();

        /* Draw white keys */
        for (int i = 0; i < num_keys; i++) {
            int octave = i / 7;
            int note_in_octave = white_notes[i % 7];
            int midi_note = start_note + octave * 12 + note_in_octave;

            ImVec2 key_pos(pos.x + i * key_w, pos.y);
            ImVec2 key_end(key_pos.x + key_w - 1, key_pos.y + key_h);

            bool hovered = ImGui::IsItemHovered() &&
                io.MousePos.x >= key_pos.x && io.MousePos.x < key_end.x &&
                io.MousePos.y >= key_pos.y && io.MousePos.y < key_end.y;

            ImU32 col = hovered && io.MouseDown[0] ? IM_COL32(100, 200, 255, 255) : IM_COL32(240, 240, 240, 255);
            draw->AddRectFilled(key_pos, key_end, col);
            draw->AddRect(key_pos, key_end, IM_COL32(100, 100, 100, 255));

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
        for (int i = 0; i < num_keys; i++) {
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

            ImU32 col = hovered && io.MouseDown[0] ? IM_COL32(80, 150, 200, 255) : IM_COL32(30, 30, 30, 255);
            draw->AddRectFilled(bk_pos, bk_end, col);

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
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1); /* vsync */

    /* ImGui setup */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    /* Dark theme */
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    /* Build layout tree */
    const oxs_ui_layout_t *layout = oxs_ui_build_layout();

    /* Main loop */
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE)
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        /* Full-window ImGui panel */
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
        ImGui::Begin("0xSYNTH", NULL,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        /* Top bar: randomize + reset buttons */
        {
            static float dice_anim = 0.0f;
            static const char *dice_faces[] = {
                "\xe2\x9a\x80", "\xe2\x9a\x81", "\xe2\x9a\x82",
                "\xe2\x9a\x83", "\xe2\x9a\x84", "\xe2\x9a\x85"
            };

            if (dice_anim > 0.0f) {
                /* Rolling animation — cycle through dice faces */
                int face = (int)(dice_anim * 20) % 6;
                ImGui::Text("%s", dice_faces[face]);
                ImGui::SameLine();
                ImGui::Text("Rolling...");
                dice_anim -= ImGui::GetIO().DeltaTime;
                if (dice_anim <= 0.0f) {
                    oxs_synth_randomize(synth);
                }
            } else {
                /* Normal state — show dice button */
                if (ImGui::Button("Randomize")) {
                    dice_anim = 0.4f; /* 400ms roll animation */
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                oxs_synth_reset_to_default(synth);
            }

            ImGui::Separator();
        }

        /* Render layout tree */
        if (layout && layout->root) {
            render_widget(layout->root, synth);
        }

        ImGui::End();
        ImGui::Render();

        glViewport(0, 0, w, h);
        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
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
