/*
 * 0xSYNTH Biquad Filter
 *
 * Transposed direct form II state-variable filter.
 * LP/HP/BP modes with cutoff smoothing and state clamping.
 * Ported from 0x808 synth.c filter implementation.
 */

#ifndef OXS_FILTER_H
#define OXS_FILTER_H

#include <stdint.h>

typedef enum {
    OXS_FILTER_LOWPASS = 0,
    OXS_FILTER_HIGHPASS,
    OXS_FILTER_BANDPASS,
    OXS_FILTER_NOTCH,
    OXS_FILTER_LADDER,     /* 24dB/oct lowpass (2x cascaded SVF) */
    OXS_FILTER_COMB,
    OXS_FILTER_FORMANT,
    OXS_FILTER_COUNT
} oxs_filter_type_t;

typedef struct {
    float a1, a2, a3, k;
} oxs_filter_coeffs_t;

/* Comb filter delay buffer size (max ~10ms at 48kHz) */
#define OXS_COMB_BUF_SIZE 512

typedef struct {
    oxs_filter_type_t type;
    float z1[2]; /* state per channel (L, R) */
    float z2[2];
    /* Ladder filter extra state (second SVF stage) */
    float z3[2];
    float z4[2];
    /* Comb filter delay line */
    float comb_buf[2][OXS_COMB_BUF_SIZE];
    int   comb_pos;
} oxs_filter_state_t;

/* Calculate filter coefficients from cutoff (Hz) and resonance (Q) */
void oxs_filter_calc_coeffs(oxs_filter_coeffs_t *c, float cutoff_hz,
                            float resonance, uint32_t sample_rate);

/* Apply filter to one stereo sample pair (in-place) */
static inline void oxs_filter_apply(oxs_filter_state_t *f,
                                    const oxs_filter_coeffs_t *c,
                                    float *left, float *right)
{
    /* Comb filter uses delay line — separate path */
    if (f->type == OXS_FILTER_COMB) {
        for (int ch = 0; ch < 2; ch++) {
            float input = (ch == 0) ? *left : *right;
            /* Delay length derived from cutoff frequency (a2 encodes ~period) */
            int delay = (int)(c->a1 * 200.0f); /* rough mapping */
            if (delay < 1) delay = 1;
            if (delay >= OXS_COMB_BUF_SIZE) delay = OXS_COMB_BUF_SIZE - 1;
            int read_pos = (f->comb_pos - delay + OXS_COMB_BUF_SIZE) % OXS_COMB_BUF_SIZE;
            float delayed = f->comb_buf[ch][read_pos];
            float out = input + delayed * c->k; /* k = feedback amount */
            f->comb_buf[ch][f->comb_pos] = out;
            if (out > 4.0f) out = 4.0f;
            if (out < -4.0f) out = -4.0f;
            if (ch == 0) *left = out; else *right = out;
        }
        f->comb_pos = (f->comb_pos + 1) % OXS_COMB_BUF_SIZE;
        return;
    }

    /* Formant filter — parallel bandpass at vowel frequencies */
    if (f->type == OXS_FILTER_FORMANT) {
        for (int ch = 0; ch < 2; ch++) {
            float input = (ch == 0) ? *left : *right;
            /* Simple 2-pole resonator approximation */
            float v3 = input - f->z2[ch];
            float v1 = c->a1 * f->z1[ch] + c->a2 * v3;
            float v2 = f->z2[ch] + c->a2 * f->z1[ch] + c->a3 * v3;
            f->z1[ch] = 2.0f * v1 - f->z1[ch];
            f->z2[ch] = 2.0f * v2 - f->z2[ch];
            if (f->z1[ch] > 4.0f) f->z1[ch] = 4.0f;
            else if (f->z1[ch] < -4.0f) f->z1[ch] = -4.0f;
            if (f->z2[ch] > 4.0f) f->z2[ch] = 4.0f;
            else if (f->z2[ch] < -4.0f) f->z2[ch] = -4.0f;
            /* Mix BP output with some LP for formant-like character */
            float out = v1 * 0.7f + v2 * 0.3f;
            if (ch == 0) *left = out; else *right = out;
        }
        return;
    }

    for (int ch = 0; ch < 2; ch++) {
        float input = (ch == 0) ? *left : *right;
        float v3 = input - f->z2[ch];
        float v1 = c->a1 * f->z1[ch] + c->a2 * v3;
        float v2 = f->z2[ch] + c->a2 * f->z1[ch] + c->a3 * v3;
        f->z1[ch] = 2.0f * v1 - f->z1[ch];
        f->z2[ch] = 2.0f * v2 - f->z2[ch];

        /* Clamp state to prevent numerical blowup at high resonance */
        if (f->z1[ch] > 4.0f) f->z1[ch] = 4.0f;
        else if (f->z1[ch] < -4.0f) f->z1[ch] = -4.0f;
        if (f->z2[ch] > 4.0f) f->z2[ch] = 4.0f;
        else if (f->z2[ch] < -4.0f) f->z2[ch] = -4.0f;

        float out;
        switch (f->type) {
        case OXS_FILTER_LOWPASS:  out = v2; break;
        case OXS_FILTER_HIGHPASS: out = input - c->k * v1 - v2; break;
        case OXS_FILTER_BANDPASS: out = v1; break;
        case OXS_FILTER_NOTCH:    out = input - c->k * v1; break;
        case OXS_FILTER_LADDER: {
            /* Second SVF stage for 24dB/oct rolloff */
            float v3b = v2 - f->z4[ch];
            float v1b = c->a1 * f->z3[ch] + c->a2 * v3b;
            float v2b = f->z4[ch] + c->a2 * f->z3[ch] + c->a3 * v3b;
            f->z3[ch] = 2.0f * v1b - f->z3[ch];
            f->z4[ch] = 2.0f * v2b - f->z4[ch];
            if (f->z3[ch] > 4.0f) f->z3[ch] = 4.0f;
            else if (f->z3[ch] < -4.0f) f->z3[ch] = -4.0f;
            if (f->z4[ch] > 4.0f) f->z4[ch] = 4.0f;
            else if (f->z4[ch] < -4.0f) f->z4[ch] = -4.0f;
            out = v2b;
            break;
        }
        default:                  out = v2; break;
        }
        if (ch == 0) *left = out; else *right = out;
    }
}

/* One-pole lowpass smoother for parameter changes */
static inline float oxs_smooth_param(float current, float target, float coeff)
{
    return current + coeff * (target - current);
}

#endif /* OXS_FILTER_H */
