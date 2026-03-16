/*
 * 0xSYNTH FM Synthesis Implementation
 * Ported from 0x808 synth.c
 */

#include "voice.h"
#include "fm.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Algorithm Routing Tables ───────────────────────────────────────────── */

const oxs_fm_algorithm_t oxs_fm_algorithms[OXS_FM_NUM_ALGORITHMS] = {
    /* 0: Serial chain: 3→2→1→0  (carrier: 0) */
    {{{1, -1, -1, -1}, {2, -1, -1, -1}, {3, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, false, false, false}},

    /* 1: 2→1→0, 3→0  (carrier: 0) */
    {{{1, 3, -1, -1}, {2, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, false, false, false}},

    /* 2: Two pairs: 3→2, 1→0  (carriers: 0, 2) */
    {{{1, -1, -1, -1}, {-1, -1, -1, -1}, {3, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, false, true, false}},

    /* 3: 3→2→1, 0 free  (carriers: 0, 1) */
    {{{-1, -1, -1, -1}, {2, -1, -1, -1}, {3, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, true, false, false}},

    /* 4: 3, 2, 1→0  (carriers: 0, 2, 3) */
    {{{1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, false, true, true}},

    /* 5: 3→2, 1, 0  (carriers: 0, 1, 2) */
    {{{-1, -1, -1, -1}, {-1, -1, -1, -1}, {3, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, true, true, false}},

    /* 6: All carriers (additive): 3, 2, 1, 0 */
    {{{-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, true, true, true}},

    /* 7: 3→(1,2), 0  (carriers: 0, 1, 2) */
    {{{-1, -1, -1, -1}, {3, -1, -1, -1}, {3, -1, -1, -1}, {-1, -1, -1, -1}},
     {true, true, true, false}},
};

/* ─── Helper: get FM operator params from snapshot ───────────────────────── */

/* Base param ID for operator N */
static inline uint32_t fm_op_base(int op)
{
    return OXS_PARAM_FM_OP0_RATIO + (uint32_t)(op * 7);
}

static inline float fm_op_ratio(const oxs_param_snapshot_t *snap, int op)
{
    return snap->values[fm_op_base(op)];
}

static inline float fm_op_level(const oxs_param_snapshot_t *snap, int op)
{
    return snap->values[fm_op_base(op) + 1];
}

static inline float fm_op_feedback(const oxs_param_snapshot_t *snap, int op)
{
    return snap->values[fm_op_base(op) + 2];
}

static inline oxs_adsr_params_t fm_op_adsr(const oxs_param_snapshot_t *snap, int op)
{
    uint32_t base = fm_op_base(op);
    return (oxs_adsr_params_t){
        .attack  = snap->values[base + 3],
        .decay   = snap->values[base + 4],
        .sustain = snap->values[base + 5],
        .release = snap->values[base + 6],
    };
}

/* ─── FM Trigger / Release ───────────────────────────────────────────────── */

void oxs_fm_trigger(oxs_voice_t *v,
                    const oxs_param_snapshot_t *snap,
                    uint32_t sample_rate)
{
    for (int op = 0; op < OXS_FM_NUM_OPERATORS; op++) {
        v->fm_phase[op] = 0.0;
        v->fm_feedback_state[op] = 0.0f;
        oxs_adsr_params_t adsr = fm_op_adsr(snap, op);
        oxs_envelope_trigger(&v->fm_env[op], &adsr, sample_rate);
    }
}

void oxs_fm_release(oxs_voice_t *v,
                    const oxs_param_snapshot_t *snap,
                    uint32_t sample_rate)
{
    for (int op = 0; op < OXS_FM_NUM_OPERATORS; op++) {
        oxs_adsr_params_t adsr = fm_op_adsr(snap, op);
        oxs_envelope_release(&v->fm_env[op], &adsr, sample_rate);
    }
}

/* ─── FM Render ──────────────────────────────────────────────────────────── */

void oxs_fm_render_voice(oxs_voice_t *v,
                         const oxs_param_snapshot_t *snap,
                         float *output, uint32_t num_frames,
                         uint32_t sample_rate)
{
    int alg_idx = (int)snap->values[OXS_PARAM_FM_ALGORITHM];
    if (alg_idx < 0 || alg_idx >= OXS_FM_NUM_ALGORITHMS) alg_idx = 0;
    const oxs_fm_algorithm_t *alg = &oxs_fm_algorithms[alg_idx];

    float sr = (float)sample_rate;
    /* Apply pitch bend */
    float bend = snap->values[OXS_PARAM_PITCH_BEND];
    float bend_range = snap->values[OXS_PARAM_PITCH_BEND_RANGE];
    float base_freq = v->frequency * powf(2.0f, (bend * bend_range) / 12.0f);
    float base_gain = v->velocity;

    /* Amp envelope ADSR */
    oxs_adsr_params_t amp_adsr = {
        snap->values[OXS_PARAM_AMP_ATTACK], snap->values[OXS_PARAM_AMP_DECAY],
        snap->values[OXS_PARAM_AMP_SUSTAIN], snap->values[OXS_PARAM_AMP_RELEASE]
    };

    /* Per-operator ADSR (for envelope_process calls) */
    oxs_adsr_params_t op_adsr[OXS_FM_NUM_OPERATORS];
    for (int op = 0; op < OXS_FM_NUM_OPERATORS; op++) {
        op_adsr[op] = fm_op_adsr(snap, op);
    }

    /* Pre-compute operator params */
    float op_ratio[OXS_FM_NUM_OPERATORS];
    float op_level[OXS_FM_NUM_OPERATORS];
    float op_fb[OXS_FM_NUM_OPERATORS];
    double phase_inc[OXS_FM_NUM_OPERATORS];

    for (int op = 0; op < OXS_FM_NUM_OPERATORS; op++) {
        op_ratio[op] = fm_op_ratio(snap, op);
        op_level[op] = fm_op_level(snap, op);
        op_fb[op] = fm_op_feedback(snap, op);
        phase_inc[op] = (double)(base_freq * op_ratio[op]) / (double)sr;
    }

    /* Pan (centered for now) */
    float pan_l = 0.5f;
    float pan_r = 0.5f;

    for (uint32_t i = 0; i < num_frames; i++) {
        /* Voice amplitude envelope */
        float amp = oxs_envelope_process(&v->amp_env, &amp_adsr, sample_rate);
        if (v->amp_env.stage == OXS_ENV_IDLE) {
            v->state = OXS_VOICE_IDLE;
            break;
        }

        /* Per-operator envelopes */
        float op_env[OXS_FM_NUM_OPERATORS];
        for (int op = 0; op < OXS_FM_NUM_OPERATORS; op++) {
            op_env[op] = oxs_envelope_process(&v->fm_env[op], &op_adsr[op],
                                               sample_rate);
        }

        /* Compute operators in reverse order (3→0) */
        float op_out[OXS_FM_NUM_OPERATORS];
        for (int op = OXS_FM_NUM_OPERATORS - 1; op >= 0; op--) {
            /* Sum modulation from source operators */
            float mod = 0.0f;
            for (int j = 0; j < OXS_FM_NUM_OPERATORS; j++) {
                int src = alg->mod_sources[op][j];
                if (src < 0) break;
                mod += op_out[src] * op_level[src] * op_env[src];
            }

            /* Self-feedback */
            if (op_fb[op] > 0.0f) {
                mod += v->fm_feedback_state[op] * op_fb[op];
            }

            /* sin(2π * (phase + mod)) */
            double phase = v->fm_phase[op] + (double)mod;
            op_out[op] = sinf((float)(phase * 2.0 * M_PI));

            /* Update feedback state */
            v->fm_feedback_state[op] = op_out[op];

            /* Advance phase */
            v->fm_phase[op] += phase_inc[op];
            if (v->fm_phase[op] >= 1.0) v->fm_phase[op] -= 1.0;
        }

        /* Sum carrier outputs */
        float sample = 0.0f;
        int num_carriers = 0;
        for (int op = 0; op < OXS_FM_NUM_OPERATORS; op++) {
            if (alg->is_carrier[op]) {
                sample += op_out[op] * op_level[op] * op_env[op];
                num_carriers++;
            }
        }
        if (num_carriers > 1) sample /= sqrtf((float)num_carriers);

        /* Apply voice envelope and gain */
        float gain = amp * base_gain;
        output[i * 2]     += sample * pan_l * gain;
        output[i * 2 + 1] += sample * pan_r * gain;
    }
}
