/*
 * 0xSYNTH Parameter System Implementation
 */

#include "params.h"
#include <string.h>
#include <math.h>

/* Helper to register a single param */
static void reg_param(oxs_param_registry_t *reg, uint32_t id,
                      const char *name, const char *group,
                      float min, float max, float def, float step,
                      const char *units, uint32_t flags)
{
    oxs_param_info_t *p = &reg->info[id];
    p->id = id;
    strncpy(p->name, name, sizeof(p->name) - 1);
    strncpy(p->group, group, sizeof(p->group) - 1);
    strncpy(p->units, units, sizeof(p->units) - 1);
    p->min = min;
    p->max = max;
    p->default_val = def;
    p->step = step;
    p->flags = flags;
    reg->count++;
}

#define AUTO OXS_PARAM_FLAG_AUTOMATABLE
#define AUTO_INT (OXS_PARAM_FLAG_AUTOMATABLE | OXS_PARAM_FLAG_INTEGER)
#define AUTO_BOOL (OXS_PARAM_FLAG_AUTOMATABLE | OXS_PARAM_FLAG_BOOLEAN)
#define AUTO_MOD (OXS_PARAM_FLAG_AUTOMATABLE | OXS_PARAM_FLAG_MODULATABLE)

void oxs_param_registry_init(oxs_param_registry_t *reg)
{
    memset(reg, 0, sizeof(*reg));

    /* Master */
    reg_param(reg, OXS_PARAM_MASTER_VOLUME,  "Master Volume",  "Master",  0.0f, 1.0f, 0.8f, 0, "",   AUTO);
    reg_param(reg, OXS_PARAM_SYNTH_MODE,     "Synth Mode",     "Master",  0, 2, 0, 1, "",             AUTO_INT);

    /* Oscillator */
    reg_param(reg, OXS_PARAM_OSC1_WAVE,      "Osc 1 Wave",     "Oscillator", 0, 3, 0, 1, "",          AUTO_INT);
    reg_param(reg, OXS_PARAM_OSC2_WAVE,      "Osc 2 Wave",     "Oscillator", 0, 3, 0, 1, "",          AUTO_INT);
    reg_param(reg, OXS_PARAM_OSC_MIX,        "Osc Mix",        "Oscillator", 0.0f, 1.0f, 0.0f, 0, "", AUTO_MOD);
    reg_param(reg, OXS_PARAM_OSC2_DETUNE,    "Osc 2 Detune",   "Oscillator", -100, 100, 0, 0, "ct",   AUTO);
    reg_param(reg, OXS_PARAM_UNISON_VOICES,  "Unison Voices",  "Oscillator", 1, 7, 1, 1, "",          AUTO_INT);
    reg_param(reg, OXS_PARAM_UNISON_DETUNE,  "Unison Detune",  "Oscillator", 0, 50, 10, 0, "ct",      AUTO);
    reg_param(reg, OXS_PARAM_NOISE_LEVEL,   "Noise Level",    "Oscillator", 0, 1, 0, 0, "",         AUTO);
    reg_param(reg, OXS_PARAM_NOISE_TYPE,    "Noise Type",     "Oscillator", 0, 1, 0, 1, "",         AUTO_INT);
    reg_param(reg, OXS_PARAM_SUB_LEVEL,     "Sub Level",      "Oscillator", 0, 1, 0, 0, "",         AUTO);
    reg_param(reg, OXS_PARAM_SUB_WAVE,      "Sub Wave",       "Oscillator", 0, 1, 0, 1, "",         AUTO_INT);
    reg_param(reg, OXS_PARAM_SUB_OCTAVE,    "Sub Octave",     "Oscillator", 0, 1, 0, 1, "",         AUTO_INT);

    /* Filter */
    reg_param(reg, OXS_PARAM_FILTER_TYPE,     "Filter Type",    "Filter", 0, 6, 0, 1, "",              AUTO_INT);
    reg_param(reg, OXS_PARAM_FILTER_CUTOFF,   "Filter Cutoff",  "Filter", 20, 20000, 20000, 0, "Hz",   AUTO_MOD);
    reg_param(reg, OXS_PARAM_FILTER_RESONANCE,"Filter Resonance","Filter", 0.5f, 20.0f, 0.707f, 0, "", AUTO_MOD);
    reg_param(reg, OXS_PARAM_FILTER_ENV_DEPTH,"Filter Env Depth","Filter", -1.0f, 1.0f, 0.0f, 0, "",   AUTO);

    /* Filter 2 */
    reg_param(reg, OXS_PARAM_FILTER2_TYPE,     "Filter 2 Type",     "Filter 2", 0, 7, 0, 1, "",  AUTO_INT);
    reg_param(reg, OXS_PARAM_FILTER2_CUTOFF,   "Filter 2 Cutoff",   "Filter 2", 20, 20000, 20000, 0, "Hz", AUTO);
    reg_param(reg, OXS_PARAM_FILTER2_RESONANCE,"Filter 2 Resonance","Filter 2", 0.5f, 20, 0.707f, 0, "",  AUTO);
    reg_param(reg, OXS_PARAM_FILTER2_ENV_DEPTH,"Filter 2 Env Depth","Filter 2", -1.0f, 1.0f, 0.0f, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_FILTER_ROUTING,   "Filter Routing",    "Filter 2", 0, 1, 0, 1, "",  AUTO_INT);

    /* Amp Envelope */
    reg_param(reg, OXS_PARAM_AMP_ATTACK,  "Amp Attack",  "Amp Envelope", 0.001f, 10.0f, 0.01f, 0, "s", AUTO);
    reg_param(reg, OXS_PARAM_AMP_DECAY,   "Amp Decay",   "Amp Envelope", 0.001f, 10.0f, 0.1f, 0, "s",  AUTO);
    reg_param(reg, OXS_PARAM_AMP_SUSTAIN, "Amp Sustain", "Amp Envelope", 0.0f, 1.0f, 0.7f, 0, "",      AUTO);
    reg_param(reg, OXS_PARAM_AMP_RELEASE, "Amp Release", "Amp Envelope", 0.001f, 10.0f, 0.3f, 0, "s",  AUTO);

    /* Filter Envelope */
    reg_param(reg, OXS_PARAM_FILT_ATTACK,  "Filter Env Attack",  "Filter Envelope", 0.001f, 10.0f, 0.01f, 0, "s", AUTO);
    reg_param(reg, OXS_PARAM_FILT_DECAY,   "Filter Env Decay",   "Filter Envelope", 0.001f, 10.0f, 0.5f, 0, "s",  AUTO);
    reg_param(reg, OXS_PARAM_FILT_SUSTAIN, "Filter Env Sustain", "Filter Envelope", 0.0f, 1.0f, 0.0f, 0, "",      AUTO);
    reg_param(reg, OXS_PARAM_FILT_RELEASE, "Filter Env Release", "Filter Envelope", 0.001f, 10.0f, 0.3f, 0, "s",  AUTO);

    /* LFO */
    reg_param(reg, OXS_PARAM_LFO_WAVE,     "LFO Wave",      "LFO", 0, 3, 0, 1, "",      AUTO_INT);
    reg_param(reg, OXS_PARAM_LFO_RATE,     "LFO Rate",      "LFO", 0.01f, 50.0f, 1.0f, 0, "Hz", AUTO);
    reg_param(reg, OXS_PARAM_LFO_DEPTH,    "LFO Depth",     "LFO", 0.0f, 1.0f, 0.0f, 0, "",     AUTO_MOD);
    reg_param(reg, OXS_PARAM_LFO_DEST,     "LFO Destination","LFO", 0, 3, 0, 1, "",               AUTO_INT);
    reg_param(reg, OXS_PARAM_LFO_BPM_SYNC, "LFO BPM Sync",  "LFO", 0, 1, 0, 1, "",               AUTO_BOOL);
    reg_param(reg, OXS_PARAM_LFO_SYNC_DIV, "LFO Sync Div",  "LFO", 0, 5, 2, 1, "",               AUTO_INT);

    /* FM - Algorithm */
    reg_param(reg, OXS_PARAM_FM_ALGORITHM, "FM Algorithm", "FM", 0, 7, 0, 1, "", AUTO_INT);

    /* FM Operators (macro for 4 operators) */
    #define REG_FM_OP(N, BASE) \
        reg_param(reg, BASE,     "FM Op " #N " Ratio",    "FM Op " #N, 0.5f, 16.0f, 1.0f, 0, "", AUTO); \
        reg_param(reg, BASE + 1, "FM Op " #N " Level",    "FM Op " #N, 0.0f, 1.0f, 1.0f, 0, "",  AUTO_MOD); \
        reg_param(reg, BASE + 2, "FM Op " #N " Feedback", "FM Op " #N, 0.0f, 1.0f, 0.0f, 0, "",  AUTO); \
        reg_param(reg, BASE + 3, "FM Op " #N " Attack",   "FM Op " #N, 0.001f, 10.0f, 0.01f, 0, "s", AUTO); \
        reg_param(reg, BASE + 4, "FM Op " #N " Decay",    "FM Op " #N, 0.001f, 10.0f, 0.5f, 0, "s",  AUTO); \
        reg_param(reg, BASE + 5, "FM Op " #N " Sustain",  "FM Op " #N, 0.0f, 1.0f, 0.0f, 0, "",      AUTO); \
        reg_param(reg, BASE + 6, "FM Op " #N " Release",  "FM Op " #N, 0.001f, 10.0f, 0.3f, 0, "s",  AUTO);

    REG_FM_OP(0, OXS_PARAM_FM_OP0_RATIO)
    REG_FM_OP(1, OXS_PARAM_FM_OP1_RATIO)
    REG_FM_OP(2, OXS_PARAM_FM_OP2_RATIO)
    REG_FM_OP(3, OXS_PARAM_FM_OP3_RATIO)
    #undef REG_FM_OP

    /* Wavetable */
    reg_param(reg, OXS_PARAM_WT_BANK,      "WT Bank",      "Wavetable", 0, 7, 0, 1, "",          AUTO_INT);
    reg_param(reg, OXS_PARAM_WT_POSITION,   "WT Position",  "Wavetable", 0.0f, 1.0f, 0.0f, 0, "", AUTO_MOD);
    reg_param(reg, OXS_PARAM_WT_ENV_DEPTH,  "WT Env Depth", "Wavetable", 0.0f, 1.0f, 0.0f, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_WT_LFO_DEPTH,  "WT LFO Depth", "Wavetable", 0.0f, 1.0f, 0.0f, 0, "", AUTO);

    /* Effects - 3 slots, each with type, bypass, mix, and 8 generic params */
    #define REG_EFX_SLOT(N, BASE) \
        reg_param(reg, BASE,     "Effect " #N " Type",   "Effect " #N, 0, 14, 0, 1, "", AUTO_INT); \
        reg_param(reg, BASE + 1, "Effect " #N " Bypass", "Effect " #N, 0, 1, 0, 1, "",  AUTO_BOOL); \
        reg_param(reg, BASE + 2, "Effect " #N " Mix",    "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 3, "Effect " #N " P0",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 4, "Effect " #N " P1",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 5, "Effect " #N " P2",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 6, "Effect " #N " P3",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 7, "Effect " #N " P4",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 8, "Effect " #N " P5",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 9, "Effect " #N " P6",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO); \
        reg_param(reg, BASE + 10,"Effect " #N " P7",     "Effect " #N, 0.0f, 1.0f, 0.5f, 0, "", AUTO);

    REG_EFX_SLOT(1, OXS_PARAM_EFX0_TYPE)
    REG_EFX_SLOT(2, OXS_PARAM_EFX1_TYPE)
    REG_EFX_SLOT(3, OXS_PARAM_EFX2_TYPE)
    #undef REG_EFX_SLOT

    /* Sampler */
    reg_param(reg, OXS_PARAM_SAMPLER_ROOT_NOTE, "Sampler Root Note", "Sampler", 0, 127, 60, 1, "",    AUTO_INT);
    reg_param(reg, OXS_PARAM_SAMPLER_TUNE,      "Sampler Tune",      "Sampler", -100, 100, 0, 0, "ct", AUTO);
    reg_param(reg, OXS_PARAM_SAMPLER_VOLUME,    "Sampler Volume",    "Sampler", 0.0f, 1.0f, 1.0f, 0, "",  AUTO);
    reg_param(reg, OXS_PARAM_SAMPLER_PAN,       "Sampler Pan",       "Sampler", -1.0f, 1.0f, 0.0f, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_SAMPLER_SLOT,      "Sampler Slot",      "Sampler", 0, 15, 0, 1, "",          AUTO_INT);

    /* Polyphony */
    reg_param(reg, OXS_PARAM_POLY_VOICES,     "Polyphony",     "Polyphony", 1, 16, 16, 1, "", AUTO_INT);
    reg_param(reg, OXS_PARAM_POLY_STEAL_MODE, "Voice Steal Mode","Polyphony", 0, 3, 0, 1, "",  AUTO_INT);

    /* Pitch Bend */
    reg_param(reg, OXS_PARAM_PITCH_BEND,       "Pitch Bend",      "Pitch Bend", -1.0f, 1.0f, 0.0f, 0, "",   AUTO_MOD);
    reg_param(reg, OXS_PARAM_PITCH_BEND_RANGE,  "Bend Range",      "Pitch Bend", 1, 24, 2, 1, "st",          AUTO_INT);
    reg_param(reg, OXS_PARAM_PITCH_BEND_SNAP,   "Bend Snap",       "Pitch Bend", 0, 1, 0, 1, "",             AUTO_BOOL);

    /* Arpeggiator */
    reg_param(reg, OXS_PARAM_ARP_ENABLED, "Arp Enabled",  "Arpeggiator", 0, 1, 0, 1, "",     AUTO_BOOL);
    reg_param(reg, OXS_PARAM_ARP_MODE,    "Arp Mode",     "Arpeggiator", 0, 4, 0, 1, "",     AUTO_INT);
    reg_param(reg, OXS_PARAM_ARP_RATE,    "Arp Rate",     "Arpeggiator", 0, 5, 3, 1, "",     AUTO_INT);
    reg_param(reg, OXS_PARAM_ARP_GATE,    "Arp Gate",     "Arpeggiator", 0.1f, 1.0f, 0.5f, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_ARP_OCTAVES, "Arp Octaves",  "Arpeggiator", 1, 4, 1, 1, "oct",  AUTO_INT);
    reg_param(reg, OXS_PARAM_ARP_BPM,     "Arp BPM",      "Arpeggiator", 20, 300, 120, 0, "bpm", AUTO);

    /* Mod Matrix — 8 slots x (src, dst, depth) */
#define REG_MOD(N, BASE) \
    reg_param(reg, BASE,     "Mod " #N " Source", "Mod Matrix", 0, 15, 0, 1, "", AUTO_INT); \
    reg_param(reg, BASE + 1, "Mod " #N " Dest",   "Mod Matrix", 0, 249, 0, 1, "", AUTO_INT); \
    reg_param(reg, BASE + 2, "Mod " #N " Depth",  "Mod Matrix", -1.0f, 1.0f, 0.0f, 0, "", AUTO);

    REG_MOD(1, OXS_PARAM_MOD0_SRC)
    REG_MOD(2, OXS_PARAM_MOD1_SRC)
    REG_MOD(3, OXS_PARAM_MOD2_SRC)
    REG_MOD(4, OXS_PARAM_MOD3_SRC)
    REG_MOD(5, OXS_PARAM_MOD4_SRC)
    REG_MOD(6, OXS_PARAM_MOD5_SRC)
    REG_MOD(7, OXS_PARAM_MOD6_SRC)
    REG_MOD(8, OXS_PARAM_MOD7_SRC)
#undef REG_MOD

    /* Performance inputs */
    reg_param(reg, OXS_PARAM_MOD_WHEEL,  "Mod Wheel",    "Performance", 0, 1, 0, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_AFTERTOUCH, "Aftertouch",   "Performance", 0, 1, 0, 0, "", AUTO);

    /* LFO 2 */
    reg_param(reg, OXS_PARAM_LFO2_WAVE,     "LFO 2 Wave",     "LFO 2", 0, 3, 0, 1, "",   AUTO_INT);
    reg_param(reg, OXS_PARAM_LFO2_RATE,     "LFO 2 Rate",     "LFO 2", 0.01f, 50, 1, 0, "Hz", AUTO);
    reg_param(reg, OXS_PARAM_LFO2_DEPTH,    "LFO 2 Depth",    "LFO 2", 0, 1, 0, 0, "",   AUTO);
    reg_param(reg, OXS_PARAM_LFO2_DEST,     "LFO 2 Dest",     "LFO 2", 0, 3, 0, 1, "",   AUTO_INT);
    reg_param(reg, OXS_PARAM_LFO2_BPM_SYNC, "LFO 2 BPM Sync", "LFO 2", 0, 1, 0, 1, "",  AUTO_BOOL);
    reg_param(reg, OXS_PARAM_LFO2_SYNC_DIV, "LFO 2 Sync Div", "LFO 2", 0, 5, 2, 1, "",  AUTO_INT);

    /* LFO 3 */
    reg_param(reg, OXS_PARAM_LFO3_WAVE,     "LFO 3 Wave",     "LFO 3", 0, 3, 0, 1, "",   AUTO_INT);
    reg_param(reg, OXS_PARAM_LFO3_RATE,     "LFO 3 Rate",     "LFO 3", 0.01f, 50, 1, 0, "Hz", AUTO);
    reg_param(reg, OXS_PARAM_LFO3_DEPTH,    "LFO 3 Depth",    "LFO 3", 0, 1, 0, 0, "",   AUTO);
    reg_param(reg, OXS_PARAM_LFO3_DEST,     "LFO 3 Dest",     "LFO 3", 0, 3, 0, 1, "",   AUTO_INT);
    reg_param(reg, OXS_PARAM_LFO3_BPM_SYNC, "LFO 3 BPM Sync", "LFO 3", 0, 1, 0, 1, "",  AUTO_BOOL);
    reg_param(reg, OXS_PARAM_LFO3_SYNC_DIV, "LFO 3 Sync Div", "LFO 3", 0, 5, 2, 1, "",  AUTO_INT);

    /* Macro controls */
    reg_param(reg, OXS_PARAM_MACRO1, "Macro 1", "Macros", 0, 1, 0, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_MACRO2, "Macro 2", "Macros", 0, 1, 0, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_MACRO3, "Macro 3", "Macros", 0, 1, 0, 0, "", AUTO);
    reg_param(reg, OXS_PARAM_MACRO4, "Macro 4", "Macros", 0, 1, 0, 0, "", AUTO);

    /* Oversampling */
    reg_param(reg, OXS_PARAM_OVERSAMPLING, "Oversample", "Polyphony", 0, 2, 0, 1, "", AUTO_INT);

    /* MPE */
    reg_param(reg, OXS_PARAM_MPE_ENABLED,     "MPE Enabled",    "Polyphony", 0, 1, 0, 1, "", AUTO_BOOL);
    reg_param(reg, OXS_PARAM_MPE_PITCH_RANGE, "MPE Pitch Range","Polyphony", 1, 96, 48, 1, "st", AUTO_INT);

    reg->initialized = true;
}

#undef AUTO
#undef AUTO_INT
#undef AUTO_BOOL
#undef AUTO_MOD

void oxs_param_store_init(oxs_param_store_t *store, const oxs_param_registry_t *reg)
{
    for (uint32_t i = 0; i < OXS_PARAM_COUNT; i++) {
        atomic_store_explicit(&store->values[i], reg->info[i].default_val,
                              memory_order_relaxed);
    }
}

void oxs_param_set(oxs_param_store_t *store, uint32_t id, float value)
{
    if (id < OXS_PARAM_COUNT) {
        atomic_store_explicit(&store->values[id], value, memory_order_relaxed);
    }
}

float oxs_param_get(const oxs_param_store_t *store, uint32_t id)
{
    if (id < OXS_PARAM_COUNT) {
        return atomic_load_explicit(&store->values[id], memory_order_relaxed);
    }
    return 0.0f;
}

void oxs_param_snapshot(const oxs_param_store_t *store, oxs_param_snapshot_t *snap)
{
    for (uint32_t i = 0; i < OXS_PARAM_COUNT; i++) {
        snap->values[i] = atomic_load_explicit(&store->values[i], memory_order_relaxed);
    }
}

uint32_t oxs_param_count(const oxs_param_registry_t *reg)
{
    return reg->count;
}

bool oxs_param_get_info(const oxs_param_registry_t *reg, uint32_t id, oxs_param_info_t *out)
{
    if (id < OXS_PARAM_COUNT && reg->info[id].name[0] != '\0') {
        *out = reg->info[id];
        return true;
    }
    return false;
}

int32_t oxs_param_id_by_name(const oxs_param_registry_t *reg, const char *name)
{
    for (uint32_t i = 0; i < OXS_PARAM_COUNT; i++) {
        if (reg->info[i].name[0] != '\0' && strcmp(reg->info[i].name, name) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

float oxs_param_get_default(const oxs_param_registry_t *reg, uint32_t id)
{
    if (id < OXS_PARAM_COUNT) {
        return reg->info[id].default_val;
    }
    return 0.0f;
}

void oxs_midi_cc_map_init(oxs_midi_cc_map_t *map)
{
    for (int i = 0; i < OXS_MIDI_CC_COUNT; i++) {
        map->param_id[i] = OXS_MIDI_CC_UNASSIGNED;
    }

    /* Standard MIDI CC defaults — work with most controllers out of the box.
     * CC1 (mod wheel) is handled separately as OXS_PARAM_MOD_WHEEL.
     * Users can override any of these via MIDI learn.
     *
     * RockJam RJMK25 factory knobs: CC70-77
     * GM2 standard CCs: CC71=resonance, CC72=release, CC73=attack, CC74=cutoff
     */
    map->param_id[7]  = OXS_PARAM_MASTER_VOLUME;    /* CC7:  Volume (universal) */
    map->param_id[70] = OXS_PARAM_OSC_MIX;           /* CC70: Sound Variation → Osc Mix (RockJam knob 1) */
    map->param_id[71] = OXS_PARAM_FILTER_RESONANCE;  /* CC71: Resonance (GM2, RockJam knob 2) */
    map->param_id[72] = OXS_PARAM_AMP_RELEASE;       /* CC72: Release Time (GM2, RockJam knob 3) */
    map->param_id[73] = OXS_PARAM_AMP_ATTACK;        /* CC73: Attack Time (GM2, RockJam knob 4) */
    map->param_id[74] = OXS_PARAM_FILTER_CUTOFF;     /* CC74: Brightness/Cutoff (GM2, RockJam knob 5) */
    map->param_id[75] = OXS_PARAM_LFO_RATE;          /* CC75: → LFO Rate (RockJam knob 6) */
    map->param_id[76] = OXS_PARAM_LFO_DEPTH;         /* CC76: → LFO Depth (RockJam knob 7) */
    map->param_id[77] = OXS_PARAM_FILTER_ENV_DEPTH;  /* CC77: → Filter Env Depth (RockJam knob 8) */
    map->param_id[16] = OXS_PARAM_MACRO1;            /* CC16: General Purpose 1 → Macro 1 */
    map->param_id[17] = OXS_PARAM_MACRO2;            /* CC17: General Purpose 2 → Macro 2 */
    map->param_id[18] = OXS_PARAM_MACRO3;            /* CC18: General Purpose 3 → Macro 3 */
    map->param_id[19] = OXS_PARAM_MACRO4;            /* CC19: General Purpose 4 → Macro 4 */
}

void oxs_midi_cc_assign(oxs_midi_cc_map_t *map, uint8_t cc, int32_t param_id)
{
    if (cc < OXS_MIDI_CC_COUNT) {
        map->param_id[cc] = param_id;
    }
}

void oxs_midi_cc_unassign(oxs_midi_cc_map_t *map, uint8_t cc)
{
    if (cc < OXS_MIDI_CC_COUNT) {
        map->param_id[cc] = OXS_MIDI_CC_UNASSIGNED;
    }
}
