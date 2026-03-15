/*
 * 0xSYNTH ADSR Envelope and LFO Implementation
 * Ported from 0x808 envelope.c
 */

#include "envelope.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── ADSR Envelope ──────────────────────────────────────────────────────── */

void oxs_envelope_init(oxs_envelope_t *env)
{
    env->stage = OXS_ENV_IDLE;
    env->level = 0.0f;
    env->rate  = 0.0f;
}

void oxs_envelope_trigger(oxs_envelope_t *env, const oxs_adsr_params_t *params,
                           uint32_t sample_rate)
{
    env->stage = OXS_ENV_ATTACK;
    env->level = 0.0f;
    float attack_time = params->attack > 0.001f ? params->attack : 0.001f;
    env->rate = 1.0f / (attack_time * (float)sample_rate);
}

void oxs_envelope_release(oxs_envelope_t *env, const oxs_adsr_params_t *params,
                           uint32_t sample_rate)
{
    if (env->stage == OXS_ENV_IDLE) return;
    env->stage = OXS_ENV_RELEASE;
    float release_time = params->release > 0.001f ? params->release : 0.001f;
    env->rate = env->level / (release_time * (float)sample_rate);
}

float oxs_envelope_process(oxs_envelope_t *env, const oxs_adsr_params_t *params,
                            uint32_t sample_rate)
{
    switch (env->stage) {
    case OXS_ENV_IDLE:
        return 0.0f;

    case OXS_ENV_ATTACK:
        env->level += env->rate;
        if (env->level >= 1.0f) {
            env->level = 1.0f;
            env->stage = OXS_ENV_DECAY;
            float decay_time = params->decay > 0.001f ? params->decay : 0.001f;
            env->rate = (1.0f - params->sustain) / (decay_time * (float)sample_rate);
        }
        return env->level;

    case OXS_ENV_DECAY:
        env->level -= env->rate;
        if (env->level <= params->sustain) {
            env->level = params->sustain;
            env->stage = OXS_ENV_SUSTAIN;
        }
        return env->level;

    case OXS_ENV_SUSTAIN:
        return env->level;

    case OXS_ENV_RELEASE:
        env->level -= env->rate;
        if (env->level <= 0.0f) {
            env->level = 0.0f;
            env->stage = OXS_ENV_IDLE;
        }
        return env->level;
    }
    return 0.0f;
}

/* ─── LFO ────────────────────────────────────────────────────────────────── */

void oxs_lfo_init(oxs_lfo_t *lfo)
{
    lfo->phase = 0.0;
}

float oxs_lfo_process(oxs_lfo_t *lfo, uint32_t sample_rate)
{
    if (lfo->rate <= 0.0f || lfo->depth <= 0.0f)
        return 0.0f;

    double phase_inc = (double)lfo->rate / (double)sample_rate;
    lfo->phase += phase_inc;
    if (lfo->phase >= 1.0) lfo->phase -= 1.0;

    float val = 0.0f;
    double p = lfo->phase;

    switch (lfo->waveform) {
    case 0: /* sine */
        val = sinf((float)(p * 2.0 * M_PI));
        break;
    case 1: /* triangle */
        if (p < 0.5)
            val = (float)(4.0 * p - 1.0);
        else
            val = (float)(3.0 - 4.0 * p);
        break;
    case 2: /* square */
        val = (p < 0.5) ? 1.0f : -1.0f;
        break;
    case 3: /* saw */
        val = (float)(2.0 * p - 1.0);
        break;
    }

    return val * lfo->depth;
}

float oxs_lfo_bpm_sync_rate(double bpm, int division)
{
    double bps = bpm / 60.0;
    static const double div_mult[] = {
        0.25,  /* 1/1 whole note */
        0.5,   /* 1/2 half note */
        1.0,   /* 1/4 quarter note */
        2.0,   /* 1/8 */
        4.0,   /* 1/16 */
        8.0    /* 1/32 */
    };
    if (division < 0) division = 0;
    if (division > 5) division = 5;
    return (float)(bps * div_mult[division]);
}
