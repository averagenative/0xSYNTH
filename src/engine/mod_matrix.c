/*
 * 0xSYNTH Modulation Matrix Implementation
 *
 * Pure computation — no allocations, no state, real-time safe.
 */

#include "mod_matrix.h"

void oxs_mod_routing_init(oxs_mod_routing_t *mod,
                           const oxs_param_snapshot_t *snap,
                           const oxs_param_registry_t *reg)
{
    mod->active_count = 0;

    for (int i = 0; i < OXS_MOD_SLOTS; i++) {
        uint32_t base = OXS_PARAM_MOD0_SRC + (uint32_t)i * 3;
        int src = (int)snap->values[base];
        if (src <= OXS_MOD_SRC_NONE || src >= OXS_MOD_SRC_COUNT)
            continue;

        uint32_t dst = (uint32_t)snap->values[base + 1];
        float depth = snap->values[base + 2];
        if (dst >= OXS_PARAM_COUNT || depth == 0.0f)
            continue;

        oxs_mod_slot_t *slot = &mod->slots[mod->active_count];
        slot->src = src;
        slot->dst_id = dst;
        slot->depth = depth;

        /* Look up destination param range for scaling */
        slot->range = 1.0f;
        if (reg && dst < OXS_PARAM_COUNT && reg->info[dst].name[0]) {
            slot->range = reg->info[dst].max - reg->info[dst].min;
        }

        mod->active_count++;
    }
}

float oxs_mod_get_source(const oxs_mod_sources_t *src, int source_id)
{
    switch (source_id) {
    case OXS_MOD_SRC_LFO1:      return src->lfo1;
    case OXS_MOD_SRC_LFO2:      return src->lfo2;
    case OXS_MOD_SRC_AMP_ENV:   return src->amp_env;
    case OXS_MOD_SRC_FILT_ENV:  return src->filt_env;
    case OXS_MOD_SRC_MOD_WHEEL: return src->mod_wheel;
    case OXS_MOD_SRC_VELOCITY:  return src->velocity;
    case OXS_MOD_SRC_AFTERTOUCH:return src->aftertouch;
    case OXS_MOD_SRC_KEY_TRACK: return src->key_track;
    case OXS_MOD_SRC_MACRO1:   return src->macro1;
    case OXS_MOD_SRC_MACRO2:   return src->macro2;
    case OXS_MOD_SRC_MACRO3:   return src->macro3;
    case OXS_MOD_SRC_MACRO4:   return src->macro4;
    case OXS_MOD_SRC_LFO3:         return src->lfo3;
    case OXS_MOD_SRC_MPE_PRESSURE: return src->mpe_pressure;
    case OXS_MOD_SRC_MPE_SLIDE:    return src->mpe_slide;
    default: return 0.0f;
    }
}

float oxs_mod_offset(const oxs_mod_routing_t *mod,
                      const oxs_mod_sources_t *src,
                      uint32_t dst_id)
{
    float offset = 0.0f;
    for (int i = 0; i < mod->active_count; i++) {
        if (mod->slots[i].dst_id == dst_id) {
            float sv = oxs_mod_get_source(src, mod->slots[i].src);
            offset += sv * mod->slots[i].depth * mod->slots[i].range;
        }
    }
    return offset;
}
