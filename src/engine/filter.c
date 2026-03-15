/*
 * 0xSYNTH Filter Implementation
 */

#include "filter.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void oxs_filter_calc_coeffs(oxs_filter_coeffs_t *c, float cutoff_hz,
                            float resonance, uint32_t sample_rate)
{
    if (cutoff_hz < 20.0f) cutoff_hz = 20.0f;
    float max_co = (float)sample_rate * 0.45f;
    if (cutoff_hz > max_co) cutoff_hz = max_co;
    if (resonance < 0.5f) resonance = 0.5f;

    float g = tanf((float)M_PI * cutoff_hz / (float)sample_rate);
    c->k = 1.0f / resonance;
    c->a1 = 1.0f / (1.0f + g * (g + c->k));
    c->a2 = g * c->a1;
    c->a3 = g * c->a2;
}
