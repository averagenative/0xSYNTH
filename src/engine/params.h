/*
 * 0xSYNTH Parameter System
 *
 * Flat integer-ID parameter registry with metadata.
 * Maps directly to CLAP parameter model.
 * All parameters are stored as atomic floats for thread-safe access.
 */

#ifndef OXS_PARAMS_H
#define OXS_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "types.h"

/* Parameter ID ranges (with gaps for future expansion) */
typedef enum {
    /* Global / Master (0-9) */
    OXS_PARAM_MASTER_VOLUME = 0,
    OXS_PARAM_SYNTH_MODE,          /* 0=subtractive, 1=FM, 2=wavetable */

    /* Oscillator (10-29) */
    OXS_PARAM_OSC1_WAVE = 10,      /* 0=saw, 1=square, 2=triangle, 3=sine */
    OXS_PARAM_OSC2_WAVE,
    OXS_PARAM_OSC_MIX,             /* 0.0 = osc1 only, 1.0 = osc2 only */
    OXS_PARAM_OSC2_DETUNE,         /* cents, -100 to +100 */
    OXS_PARAM_UNISON_VOICES,       /* 1-7 */
    OXS_PARAM_UNISON_DETUNE,       /* cents, 0-50 */

    /* Filter (30-39) */
    OXS_PARAM_FILTER_TYPE = 30,    /* 0=LP, 1=HP, 2=BP */
    OXS_PARAM_FILTER_CUTOFF,       /* Hz, 20-20000 */
    OXS_PARAM_FILTER_RESONANCE,    /* Q, 0.5-20.0 */
    OXS_PARAM_FILTER_ENV_DEPTH,    /* -1.0 to 1.0 */

    /* Amp Envelope (40-49) */
    OXS_PARAM_AMP_ATTACK = 40,     /* seconds, 0.001-10.0 */
    OXS_PARAM_AMP_DECAY,
    OXS_PARAM_AMP_SUSTAIN,         /* 0.0-1.0 */
    OXS_PARAM_AMP_RELEASE,

    /* Filter Envelope (50-59) */
    OXS_PARAM_FILT_ATTACK = 50,
    OXS_PARAM_FILT_DECAY,
    OXS_PARAM_FILT_SUSTAIN,
    OXS_PARAM_FILT_RELEASE,

    /* LFO (60-69) */
    OXS_PARAM_LFO_WAVE = 60,       /* 0=sine, 1=tri, 2=square, 3=saw */
    OXS_PARAM_LFO_RATE,            /* Hz, 0.01-50.0 */
    OXS_PARAM_LFO_DEPTH,           /* 0.0-1.0 */
    OXS_PARAM_LFO_DEST,            /* 0=none, 1=pitch, 2=filter, 3=amp */
    OXS_PARAM_LFO_BPM_SYNC,        /* 0=off, 1=on */
    OXS_PARAM_LFO_SYNC_DIV,        /* 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, 5=1/32 */

    /* FM Synthesis (70-109) */
    OXS_PARAM_FM_ALGORITHM = 70,    /* 0-7 */

    /* FM Operator 0 (71-77) */
    OXS_PARAM_FM_OP0_RATIO = 71,    /* 0.5-16.0 */
    OXS_PARAM_FM_OP0_LEVEL,         /* 0.0-1.0 */
    OXS_PARAM_FM_OP0_FEEDBACK,      /* 0.0-1.0 */
    OXS_PARAM_FM_OP0_ATTACK,
    OXS_PARAM_FM_OP0_DECAY,
    OXS_PARAM_FM_OP0_SUSTAIN,
    OXS_PARAM_FM_OP0_RELEASE,

    /* FM Operator 1 (78-84) */
    OXS_PARAM_FM_OP1_RATIO = 78,
    OXS_PARAM_FM_OP1_LEVEL,
    OXS_PARAM_FM_OP1_FEEDBACK,
    OXS_PARAM_FM_OP1_ATTACK,
    OXS_PARAM_FM_OP1_DECAY,
    OXS_PARAM_FM_OP1_SUSTAIN,
    OXS_PARAM_FM_OP1_RELEASE,

    /* FM Operator 2 (85-91) */
    OXS_PARAM_FM_OP2_RATIO = 85,
    OXS_PARAM_FM_OP2_LEVEL,
    OXS_PARAM_FM_OP2_FEEDBACK,
    OXS_PARAM_FM_OP2_ATTACK,
    OXS_PARAM_FM_OP2_DECAY,
    OXS_PARAM_FM_OP2_SUSTAIN,
    OXS_PARAM_FM_OP2_RELEASE,

    /* FM Operator 3 (92-98) */
    OXS_PARAM_FM_OP3_RATIO = 92,
    OXS_PARAM_FM_OP3_LEVEL,
    OXS_PARAM_FM_OP3_FEEDBACK,
    OXS_PARAM_FM_OP3_ATTACK,
    OXS_PARAM_FM_OP3_DECAY,
    OXS_PARAM_FM_OP3_SUSTAIN,
    OXS_PARAM_FM_OP3_RELEASE,

    /* Wavetable (110-119) */
    OXS_PARAM_WT_BANK = 110,
    OXS_PARAM_WT_POSITION,          /* 0.0-1.0 */
    OXS_PARAM_WT_ENV_DEPTH,         /* 0.0-1.0 */
    OXS_PARAM_WT_LFO_DEPTH,         /* 0.0-1.0 */

    /* Effect Slot 0 (120-139) */
    OXS_PARAM_EFX0_TYPE = 120,      /* 0=none, 1=filter, ... 14=shimmer */
    OXS_PARAM_EFX0_BYPASS,
    OXS_PARAM_EFX0_MIX,             /* wet/dry, 0.0-1.0 */
    OXS_PARAM_EFX0_P0,              /* type-specific params */
    OXS_PARAM_EFX0_P1,
    OXS_PARAM_EFX0_P2,
    OXS_PARAM_EFX0_P3,
    OXS_PARAM_EFX0_P4,
    OXS_PARAM_EFX0_P5,
    OXS_PARAM_EFX0_P6,
    OXS_PARAM_EFX0_P7,

    /* Effect Slot 1 (140-159) */
    OXS_PARAM_EFX1_TYPE = 140,
    OXS_PARAM_EFX1_BYPASS,
    OXS_PARAM_EFX1_MIX,
    OXS_PARAM_EFX1_P0,
    OXS_PARAM_EFX1_P1,
    OXS_PARAM_EFX1_P2,
    OXS_PARAM_EFX1_P3,
    OXS_PARAM_EFX1_P4,
    OXS_PARAM_EFX1_P5,
    OXS_PARAM_EFX1_P6,
    OXS_PARAM_EFX1_P7,

    /* Effect Slot 2 (160-179) */
    OXS_PARAM_EFX2_TYPE = 160,
    OXS_PARAM_EFX2_BYPASS,
    OXS_PARAM_EFX2_MIX,
    OXS_PARAM_EFX2_P0,
    OXS_PARAM_EFX2_P1,
    OXS_PARAM_EFX2_P2,
    OXS_PARAM_EFX2_P3,
    OXS_PARAM_EFX2_P4,
    OXS_PARAM_EFX2_P5,
    OXS_PARAM_EFX2_P6,
    OXS_PARAM_EFX2_P7,

    /* Sampler (180-189) */
    OXS_PARAM_SAMPLER_ROOT_NOTE = 180,  /* MIDI note 0-127 */
    OXS_PARAM_SAMPLER_TUNE,             /* cents, -100 to +100 */
    OXS_PARAM_SAMPLER_VOLUME,           /* 0.0-1.0 */
    OXS_PARAM_SAMPLER_PAN,              /* -1.0 to 1.0 */
    OXS_PARAM_SAMPLER_SLOT,             /* slot index */

    /* Polyphony (190-199) */
    OXS_PARAM_POLY_VOICES = 190,        /* 1-16 */
    OXS_PARAM_POLY_STEAL_MODE,          /* 0=oldest, 1=quietest, 2=lowest, 3=highest */

    OXS_PARAM_COUNT = 200               /* total param slots (with room for growth) */
} oxs_param_id;

/* Atomic parameter store */
typedef struct {
    _Atomic float values[OXS_PARAM_COUNT];
} oxs_param_store_t;

/* Parameter snapshot (non-atomic copy for DSP) */
typedef struct {
    float values[OXS_PARAM_COUNT];
} oxs_param_snapshot_t;

/* MIDI CC mapping */
#define OXS_MIDI_CC_COUNT 128
#define OXS_MIDI_CC_UNASSIGNED (-1)

typedef struct {
    int32_t param_id[OXS_MIDI_CC_COUNT]; /* -1 = unassigned */
} oxs_midi_cc_map_t;

/*
 * Registry — static table of param info, populated once at init.
 * The registry itself is not atomic; it's read-only after init.
 */
typedef struct {
    oxs_param_info_t info[OXS_PARAM_COUNT];
    uint32_t         count; /* number of registered params (not all slots used) */
    bool             initialized;
} oxs_param_registry_t;

/* Initialize the parameter registry with all param metadata */
void oxs_param_registry_init(oxs_param_registry_t *reg);

/* Initialize param store with default values from registry */
void oxs_param_store_init(oxs_param_store_t *store, const oxs_param_registry_t *reg);

/* Atomic param access */
void  oxs_param_set(oxs_param_store_t *store, uint32_t id, float value);
float oxs_param_get(const oxs_param_store_t *store, uint32_t id);

/* Snapshot all params (called at start of process()) */
void oxs_param_snapshot(const oxs_param_store_t *store, oxs_param_snapshot_t *snap);

/* Registry query */
uint32_t oxs_param_count(const oxs_param_registry_t *reg);
bool     oxs_param_get_info(const oxs_param_registry_t *reg, uint32_t id, oxs_param_info_t *out);
int32_t  oxs_param_id_by_name(const oxs_param_registry_t *reg, const char *name);
float    oxs_param_get_default(const oxs_param_registry_t *reg, uint32_t id);

/* MIDI CC mapping */
void oxs_midi_cc_map_init(oxs_midi_cc_map_t *map);
void oxs_midi_cc_assign(oxs_midi_cc_map_t *map, uint8_t cc, int32_t param_id);
void oxs_midi_cc_unassign(oxs_midi_cc_map_t *map, uint8_t cc);

#endif /* OXS_PARAMS_H */
