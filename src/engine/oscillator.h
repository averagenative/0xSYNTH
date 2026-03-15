/*
 * 0xSYNTH Oscillator
 *
 * Waveform generation: saw, square, triangle, sine.
 * Uses pre-computed wavetables with linear interpolation.
 */

#ifndef OXS_OSCILLATOR_H
#define OXS_OSCILLATOR_H

#include <stdint.h>
#include <stdbool.h>

#define OXS_WAVETABLE_SIZE 2048

typedef enum {
    OXS_WAVE_SAW = 0,
    OXS_WAVE_SQUARE,
    OXS_WAVE_TRIANGLE,
    OXS_WAVE_SINE,
    OXS_WAVE_COUNT
} oxs_waveform_t;

typedef struct {
    float tables[OXS_WAVE_COUNT][OXS_WAVETABLE_SIZE];
    bool  initialized;
} oxs_wavetables_t;

/* Initialize wavetables (call once at synth create) */
void oxs_wavetables_init(oxs_wavetables_t *wt);

/* Read a sample from a wavetable with linear interpolation.
 * phase is 0.0–1.0. */
static inline float oxs_wavetable_read(const float *table, double phase)
{
    double pos = phase * OXS_WAVETABLE_SIZE;
    int idx = (int)pos;
    float frac = (float)(pos - idx);
    int next = (idx + 1) % OXS_WAVETABLE_SIZE;
    return table[idx] * (1.0f - frac) + table[next] * frac;
}

#endif /* OXS_OSCILLATOR_H */
