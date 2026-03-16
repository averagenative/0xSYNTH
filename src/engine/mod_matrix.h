/*
 * 0xSYNTH Modulation Matrix
 *
 * 8 routing slots, each mapping a modulation source to any parameter
 * with configurable bipolar depth. Evaluated per-voice per-sample
 * in the render path. Pure computation, no allocations.
 */

#ifndef OXS_MOD_MATRIX_H
#define OXS_MOD_MATRIX_H

#include "params.h"

#define OXS_MOD_SLOTS 8

/* Modulation sources */
typedef enum {
    OXS_MOD_SRC_NONE = 0,
    OXS_MOD_SRC_LFO1,
    OXS_MOD_SRC_LFO2,       /* future — returns 0.0 */
    OXS_MOD_SRC_AMP_ENV,
    OXS_MOD_SRC_FILT_ENV,
    OXS_MOD_SRC_MOD_WHEEL,
    OXS_MOD_SRC_VELOCITY,
    OXS_MOD_SRC_AFTERTOUCH,
    OXS_MOD_SRC_KEY_TRACK,
    OXS_MOD_SRC_MACRO1,
    OXS_MOD_SRC_MACRO2,
    OXS_MOD_SRC_MACRO3,
    OXS_MOD_SRC_MACRO4,
    OXS_MOD_SRC_LFO3,
    OXS_MOD_SRC_MPE_PRESSURE,
    OXS_MOD_SRC_MPE_SLIDE,
    OXS_MOD_SRC_COUNT
} oxs_mod_source_t;

/* Single mod routing slot */
typedef struct {
    int      src;       /* oxs_mod_source_t */
    uint32_t dst_id;    /* target param ID */
    float    depth;     /* -1.0 to +1.0 */
    float    range;     /* max - min of destination param */
} oxs_mod_slot_t;

/* Full routing table (populated once per process() call) */
typedef struct {
    oxs_mod_slot_t slots[OXS_MOD_SLOTS];
    int            active_count;
} oxs_mod_routing_t;

/* Per-voice mod source values (computed once per voice per sample) */
typedef struct {
    float lfo1;
    float lfo2;
    float lfo3;
    float amp_env;
    float filt_env;
    float mod_wheel;
    float velocity;
    float aftertouch;
    float key_track;
    float macro1;
    float macro2;
    float macro3;
    float macro4;
    float mpe_pressure;
    float mpe_slide;
} oxs_mod_sources_t;

/* Initialize routing table from param snapshot.
 * Reads MOD0-7 SRC/DST/DEPTH from snap, looks up param ranges from registry. */
void oxs_mod_routing_init(oxs_mod_routing_t *mod,
                           const oxs_param_snapshot_t *snap,
                           const oxs_param_registry_t *reg);

/* Get the source value for a given mod source enum. */
float oxs_mod_get_source(const oxs_mod_sources_t *src, int source_id);

/* Compute the modulation offset for a specific destination param.
 * Returns the total additive offset from all active mod slots targeting dst_id. */
float oxs_mod_offset(const oxs_mod_routing_t *mod,
                      const oxs_mod_sources_t *src,
                      uint32_t dst_id);

#endif /* OXS_MOD_MATRIX_H */
