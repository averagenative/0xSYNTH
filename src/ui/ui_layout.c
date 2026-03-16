/*
 * 0xSYNTH UI Layout Builder
 *
 * Defines the full synth UI as a static widget tree.
 * This is data — no rendering, no toolkit dependency.
 */

#include "ui_types.h"
#include "../engine/params.h"
#include <string.h>

/* Static widget pool — avoids heap allocation for layout */
#define WIDGET_POOL_SIZE 256
static oxs_ui_widget_t widget_pool[WIDGET_POOL_SIZE];
static int widget_pool_idx = 0;

static oxs_ui_widget_t *alloc_widget(void)
{
    if (widget_pool_idx >= WIDGET_POOL_SIZE) return NULL;
    oxs_ui_widget_t *w = &widget_pool[widget_pool_idx++];
    memset(w, 0, sizeof(*w));
    w->param_id = -1;
    w->col_span = 1;
    return w;
}

/* Helpers */
static oxs_ui_widget_t *group(const char *label, oxs_ui_direction_t dir)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_GROUP;
    strncpy(w->label, label, OXS_UI_MAX_LABEL - 1);
    w->direction = dir;
    return w;
}

static void add_child(oxs_ui_widget_t *parent, oxs_ui_widget_t *child)
{
    if (parent->num_children < OXS_UI_MAX_CHILDREN) {
        parent->children[parent->num_children++] = child;
    }
}

static oxs_ui_widget_t *knob(const char *label, int32_t param_id)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_KNOB;
    strncpy(w->label, label, OXS_UI_MAX_LABEL - 1);
    w->param_id = param_id;
    return w;
}

static oxs_ui_widget_t *dropdown(const char *label, int32_t param_id,
                                  const char **options, int count)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_DROPDOWN;
    strncpy(w->label, label, OXS_UI_MAX_LABEL - 1);
    w->param_id = param_id;
    w->num_options = count < OXS_UI_MAX_OPTIONS ? count : OXS_UI_MAX_OPTIONS;
    for (int i = 0; i < w->num_options; i++) {
        strncpy(w->options[i].label, options[i], OXS_UI_MAX_LABEL - 1);
        w->options[i].value = i;
    }
    return w;
}

static oxs_ui_widget_t *toggle(const char *label, int32_t param_id)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_TOGGLE;
    strncpy(w->label, label, OXS_UI_MAX_LABEL - 1);
    w->param_id = param_id;
    return w;
}

static oxs_ui_widget_t *lbl(const char *text)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_LABEL;
    strncpy(w->label, text, OXS_UI_MAX_LABEL - 1);
    return w;
}

static oxs_ui_widget_t *envelope_display(const char *label,
                                          int32_t a, int32_t d,
                                          int32_t s, int32_t r)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_ENVELOPE;
    strncpy(w->label, label, OXS_UI_MAX_LABEL - 1);
    w->env_attack_id = a;
    w->env_decay_id = d;
    w->env_sustain_id = s;
    w->env_release_id = r;
    return w;
}

static oxs_ui_widget_t *meter(void)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_METER;
    strncpy(w->label, "Level", OXS_UI_MAX_LABEL - 1);
    return w;
}

static oxs_ui_widget_t *waveform_display(void)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_WAVEFORM;
    strncpy(w->label, "Waveform", OXS_UI_MAX_LABEL - 1);
    return w;
}

static oxs_ui_widget_t *preset_browser(void)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_PRESET_BROWSER;
    strncpy(w->label, "Presets", OXS_UI_MAX_LABEL - 1);
    return w;
}

static oxs_ui_widget_t *keyboard(void)
{
    oxs_ui_widget_t *w = alloc_widget();
    w->type = OXS_UI_KEYBOARD;
    strncpy(w->label, "Keyboard", OXS_UI_MAX_LABEL - 1);
    return w;
}

/* ─── Build the full layout ──────────────────────────────────────────────── */

static oxs_ui_layout_t layout;

const oxs_ui_layout_t *oxs_ui_build_layout(void)
{
    widget_pool_idx = 0; /* reset pool */

    static const char *wave_names[] = {"Saw", "Square", "Triangle", "Sine"};
    static const char *filter_names[] = {"Lowpass", "Highpass", "Bandpass"};
    static const char *mode_names[] = {"Subtractive", "FM", "Wavetable"};
    static const char *lfo_dest_names[] = {"Off", "Pitch", "Filter", "Amp"};
    static const char *lfo_wave_names[] = {"Sine", "Triangle", "Square", "Saw"};
    static const char *algo_names[] = {
        "1: Serial",    /* 3→2→1→0, carrier: 0 */
        "2: Branch",    /* 2→1→0 + 3→0, carrier: 0 */
        "3: Dual Pair", /* 3→2, 1→0, carriers: 0,2 */
        "4: Split",     /* 3→2→1 + 0, carriers: 0,1 */
        "5: Triple",    /* 1→0, 2+3 free, carriers: 0,2,3 */
        "6: Stack",     /* 3→2 + 1 + 0, carriers: 0,1,2 */
        "7: Additive",  /* all carriers, organ-like */
        "8: Y-Split",   /* 3→(1,2) + 0, carriers: 0,1,2 */
    };
    static const char *wt_bank_names[] = {"Analog", "Harmonics", "PWM", "Formant"};

    /* Root */
    oxs_ui_widget_t *root = group("0xSYNTH", OXS_UI_VERTICAL);

    /* === Top bar: mode + volume + meter on one row === */
    oxs_ui_widget_t *top = group("Top", OXS_UI_HORIZONTAL);
    add_child(top, dropdown("Mode", OXS_PARAM_SYNTH_MODE, mode_names, 3));
    add_child(top, knob("Volume", OXS_PARAM_MASTER_VOLUME));
    add_child(top, meter());
    add_child(top, preset_browser());
    add_child(root, top);

    /* === Oscillator section (subtractive only) === */
    oxs_ui_widget_t *osc = group("Oscillator", OXS_UI_VERTICAL);
    /* Row 1: wave selectors */
    oxs_ui_widget_t *osc_waves = group("", OXS_UI_HORIZONTAL);
    add_child(osc_waves, dropdown("Osc 1", OXS_PARAM_OSC1_WAVE, wave_names, 4));
    add_child(osc_waves, dropdown("Osc 2", OXS_PARAM_OSC2_WAVE, wave_names, 4));
    add_child(osc, osc_waves);
    /* Row 2: knobs */
    oxs_ui_widget_t *osc_knobs = group("", OXS_UI_HORIZONTAL);
    add_child(osc_knobs, knob("Mix", OXS_PARAM_OSC_MIX));
    add_child(osc_knobs, knob("Detune", OXS_PARAM_OSC2_DETUNE));
    add_child(osc_knobs, knob("Unison", OXS_PARAM_UNISON_VOICES));
    add_child(osc_knobs, knob("Spread", OXS_PARAM_UNISON_DETUNE));
    add_child(osc_knobs, waveform_display());
    add_child(osc, osc_knobs);
    add_child(root, osc);

    /* === Filter section === */
    oxs_ui_widget_t *filt = group("Filter", OXS_UI_VERTICAL);
    oxs_ui_widget_t *filt_row1 = group("", OXS_UI_HORIZONTAL);
    add_child(filt_row1, dropdown("Type", OXS_PARAM_FILTER_TYPE, filter_names, 3));
    add_child(filt, filt_row1);
    oxs_ui_widget_t *filt_knobs = group("", OXS_UI_HORIZONTAL);
    add_child(filt_knobs, knob("Cutoff", OXS_PARAM_FILTER_CUTOFF));
    add_child(filt_knobs, knob("Resonance", OXS_PARAM_FILTER_RESONANCE));
    add_child(filt_knobs, knob("Env Depth", OXS_PARAM_FILTER_ENV_DEPTH));
    add_child(filt, filt_knobs);
    add_child(root, filt);

    /* === Envelopes — stacked vertically === */
    oxs_ui_widget_t *amp_env = group("Amp Envelope", OXS_UI_HORIZONTAL);
    add_child(amp_env, knob("A", OXS_PARAM_AMP_ATTACK));
    add_child(amp_env, knob("D", OXS_PARAM_AMP_DECAY));
    add_child(amp_env, knob("S", OXS_PARAM_AMP_SUSTAIN));
    add_child(amp_env, knob("R", OXS_PARAM_AMP_RELEASE));
    add_child(amp_env, envelope_display("Amp",
        OXS_PARAM_AMP_ATTACK, OXS_PARAM_AMP_DECAY,
        OXS_PARAM_AMP_SUSTAIN, OXS_PARAM_AMP_RELEASE));
    add_child(root, amp_env);

    oxs_ui_widget_t *filt_env = group("Filter Envelope", OXS_UI_HORIZONTAL);
    add_child(filt_env, knob("A", OXS_PARAM_FILT_ATTACK));
    add_child(filt_env, knob("D", OXS_PARAM_FILT_DECAY));
    add_child(filt_env, knob("S", OXS_PARAM_FILT_SUSTAIN));
    add_child(filt_env, knob("R", OXS_PARAM_FILT_RELEASE));
    add_child(filt_env, envelope_display("Filter",
        OXS_PARAM_FILT_ATTACK, OXS_PARAM_FILT_DECAY,
        OXS_PARAM_FILT_SUSTAIN, OXS_PARAM_FILT_RELEASE));
    add_child(root, filt_env);

    /* === LFO section === */
    oxs_ui_widget_t *lfo = group("LFO", OXS_UI_HORIZONTAL);
    add_child(lfo, dropdown("Wave", OXS_PARAM_LFO_WAVE, lfo_wave_names, 4));
    add_child(lfo, knob("Rate", OXS_PARAM_LFO_RATE));
    add_child(lfo, knob("Depth", OXS_PARAM_LFO_DEPTH));
    add_child(lfo, dropdown("Dest", OXS_PARAM_LFO_DEST, lfo_dest_names, 4));
    add_child(lfo, toggle("BPM Sync", OXS_PARAM_LFO_BPM_SYNC));
    add_child(root, lfo);

    /* === FM section === */
    oxs_ui_widget_t *fm = group("FM Synthesis", OXS_UI_VERTICAL);
    add_child(fm, dropdown("Algorithm", OXS_PARAM_FM_ALGORITHM, algo_names, 8));

    static const char *op_labels[] = {
        "Op 1 (Carrier)",
        "Op 2 (Mod/Carrier)",
        "Op 3 (Modulator)",
        "Op 4 (Modulator)"
    };
    for (int op = 0; op < 4; op++) {
        oxs_ui_widget_t *op_grp = group(op_labels[op], OXS_UI_HORIZONTAL);
        uint32_t base = OXS_PARAM_FM_OP0_RATIO + (uint32_t)(op * 7);
        add_child(op_grp, knob("Freq", (int32_t)base));
        add_child(op_grp, knob("Vol", (int32_t)(base + 1)));
        add_child(op_grp, knob("Feedback", (int32_t)(base + 2)));
        add_child(op_grp, knob("Atk", (int32_t)(base + 3)));
        add_child(op_grp, knob("Dec", (int32_t)(base + 4)));
        add_child(op_grp, knob("Sus", (int32_t)(base + 5)));
        add_child(op_grp, knob("Rel", (int32_t)(base + 6)));
        add_child(fm, op_grp);
    }
    add_child(root, fm);

    /* === Wavetable section === */
    oxs_ui_widget_t *wt = group("Wavetable", OXS_UI_HORIZONTAL);
    add_child(wt, dropdown("Bank", OXS_PARAM_WT_BANK, wt_bank_names, 4));
    add_child(wt, knob("Position", OXS_PARAM_WT_POSITION));
    add_child(wt, knob("Env Depth", OXS_PARAM_WT_ENV_DEPTH));
    add_child(wt, knob("LFO Depth", OXS_PARAM_WT_LFO_DEPTH));
    add_child(root, wt);

    /* === Effects section === */
    oxs_ui_widget_t *efx = group("Effects", OXS_UI_VERTICAL);
    static const char *efx_names[] = {
        "None", "Filter", "Delay", "Reverb", "Overdrive", "Fuzz",
        "Chorus", "Bitcrusher", "Compressor", "Phaser", "Flanger",
        "Tremolo", "Ring Mod", "Tape", "Shimmer"
    };

    for (int slot = 0; slot < 3; slot++) {
        char slot_label[16];
        snprintf(slot_label, sizeof(slot_label), "Slot %d", slot + 1);
        uint32_t base = (slot == 0) ? OXS_PARAM_EFX0_TYPE :
                        (slot == 1) ? OXS_PARAM_EFX1_TYPE : OXS_PARAM_EFX2_TYPE;
        oxs_ui_widget_t *s = group(slot_label, OXS_UI_HORIZONTAL);
        add_child(s, dropdown("Type", (int32_t)base, efx_names, 15));
        add_child(s, toggle("Bypass", (int32_t)(base + 1)));
        add_child(s, knob("Mix", (int32_t)(base + 2)));
        /* Generic param knobs P0-P3 */
        add_child(s, knob("P0", (int32_t)(base + 3)));
        add_child(s, knob("P1", (int32_t)(base + 4)));
        add_child(s, knob("P2", (int32_t)(base + 5)));
        add_child(s, knob("P3", (int32_t)(base + 6)));
        add_child(efx, s);
    }
    add_child(root, efx);

    /* === Sampler section === */
    oxs_ui_widget_t *samp = group("Sampler", OXS_UI_HORIZONTAL);
    add_child(samp, knob("Root Note", OXS_PARAM_SAMPLER_ROOT_NOTE));
    add_child(samp, knob("Tune", OXS_PARAM_SAMPLER_TUNE));
    add_child(samp, knob("Volume", OXS_PARAM_SAMPLER_VOLUME));
    add_child(samp, knob("Pan", OXS_PARAM_SAMPLER_PAN));
    add_child(root, samp);

    /* === Polyphony section === */
    oxs_ui_widget_t *poly = group("Polyphony", OXS_UI_HORIZONTAL);
    add_child(poly, knob("Voices", OXS_PARAM_POLY_VOICES));
    static const char *steal_names[] = {"Oldest", "Quietest", "Lowest", "Highest"};
    add_child(poly, dropdown("Steal", OXS_PARAM_POLY_STEAL_MODE, steal_names, 4));
    add_child(root, poly);

    /* === Virtual keyboard === */
    add_child(root, keyboard());

    layout.root = root;
    layout.total_widgets = widget_pool_idx;

    return &layout;
}

/* ─── Validation ─────────────────────────────────────────────────────────── */

static int count_param_bindings(const oxs_ui_widget_t *w, int *bound_ids, int *count,
                                 int max)
{
    if (w->param_id >= 0 && *count < max) {
        bound_ids[(*count)++] = w->param_id;
    }
    /* Also check envelope bindings */
    if (w->type == OXS_UI_ENVELOPE) {
        if (w->env_attack_id >= 0 && *count < max)  bound_ids[(*count)++] = w->env_attack_id;
        if (w->env_decay_id >= 0 && *count < max)   bound_ids[(*count)++] = w->env_decay_id;
        if (w->env_sustain_id >= 0 && *count < max)  bound_ids[(*count)++] = w->env_sustain_id;
        if (w->env_release_id >= 0 && *count < max)  bound_ids[(*count)++] = w->env_release_id;
    }
    for (int i = 0; i < w->num_children; i++) {
        count_param_bindings(w->children[i], bound_ids, count, max);
    }
    return *count;
}

bool oxs_ui_validate_layout(const oxs_ui_layout_t *lay)
{
    if (!lay || !lay->root) return false;

    /* Collect all param IDs referenced in layout */
    int bound_ids[512];
    int count = 0;
    count_param_bindings(lay->root, bound_ids, &count, 512);

    /* Verify all are valid param IDs (exist in registry) */
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);

    for (int i = 0; i < count; i++) {
        oxs_param_info_t info;
        if (!oxs_param_get_info(&reg, (uint32_t)bound_ids[i], &info)) {
            return false; /* invalid param ID in layout */
        }
    }

    return true;
}
