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
    OXS_FILTER_BANDPASS
} oxs_filter_type_t;

typedef struct {
    float a1, a2, a3, k;
} oxs_filter_coeffs_t;

typedef struct {
    oxs_filter_type_t type;
    float z1[2]; /* state per channel (L, R) */
    float z2[2];
} oxs_filter_state_t;

/* Calculate filter coefficients from cutoff (Hz) and resonance (Q) */
void oxs_filter_calc_coeffs(oxs_filter_coeffs_t *c, float cutoff_hz,
                            float resonance, uint32_t sample_rate);

/* Apply filter to one stereo sample pair (in-place) */
static inline void oxs_filter_apply(oxs_filter_state_t *f,
                                    const oxs_filter_coeffs_t *c,
                                    float *left, float *right)
{
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
