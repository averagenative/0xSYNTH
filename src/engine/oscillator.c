/*
 * 0xSYNTH Oscillator Implementation
 * Ported from 0x808 synth.c wavetable initialization
 */

#include "oscillator.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void oxs_wavetables_init(oxs_wavetables_t *wt)
{
    for (int i = 0; i < OXS_WAVETABLE_SIZE; i++) {
        double phase = (double)i / (double)OXS_WAVETABLE_SIZE;

        /* Sine */
        wt->tables[OXS_WAVE_SINE][i] = sinf((float)(phase * 2.0 * M_PI));

        /* Saw: -1 to +1 */
        wt->tables[OXS_WAVE_SAW][i] = (float)(2.0 * phase - 1.0);

        /* Square: +1 / -1 */
        wt->tables[OXS_WAVE_SQUARE][i] = (phase < 0.5) ? 1.0f : -1.0f;

        /* Triangle: piecewise linear */
        if (phase < 0.25)
            wt->tables[OXS_WAVE_TRIANGLE][i] = (float)(4.0 * phase);
        else if (phase < 0.75)
            wt->tables[OXS_WAVE_TRIANGLE][i] = (float)(2.0 - 4.0 * phase);
        else
            wt->tables[OXS_WAVE_TRIANGLE][i] = (float)(4.0 * phase - 4.0);
    }
    wt->initialized = true;
}
