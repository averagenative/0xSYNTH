/*
 * 0xSYNTH FM Synthesis
 *
 * 4-operator FM with 8 algorithms.
 * Ported from 0x808 synth.c.
 */

#ifndef OXS_FM_H
#define OXS_FM_H

#include "params.h"
#include "envelope.h"
#include <stdint.h>
#include <stdbool.h>

#define OXS_FM_NUM_OPERATORS  4
#define OXS_FM_NUM_ALGORITHMS 8

/* Forward declare — full definition in voice.h */
typedef struct oxs_voice_s oxs_voice_t;

typedef struct {
    int  mod_sources[OXS_FM_NUM_OPERATORS][OXS_FM_NUM_OPERATORS]; /* -1 terminated */
    bool is_carrier[OXS_FM_NUM_OPERATORS];
} oxs_fm_algorithm_t;

/* Global algorithm table — initialized statically */
extern const oxs_fm_algorithm_t oxs_fm_algorithms[OXS_FM_NUM_ALGORITHMS];


/* Render FM synthesis for a single voice into output buffer (additive) */
void oxs_fm_render_voice(oxs_voice_t *v,
                         const oxs_param_snapshot_t *snap,
                         float *output, uint32_t num_frames,
                         uint32_t sample_rate);

/* Trigger FM operator envelopes for a voice */
void oxs_fm_trigger(oxs_voice_t *v,
                    const oxs_param_snapshot_t *snap,
                    uint32_t sample_rate);

/* Release FM operator envelopes for a voice */
void oxs_fm_release(oxs_voice_t *v,
                    const oxs_param_snapshot_t *snap,
                    uint32_t sample_rate);

#endif /* OXS_FM_H */
