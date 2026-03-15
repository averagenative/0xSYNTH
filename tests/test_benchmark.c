/*
 * 0xSYNTH Performance Benchmark (Phase 12)
 *
 * Renders 60s of 16-voice polyphonic audio with effects
 * and measures real-time ratio.
 */

#include "synth_api.h"
#include "../src/engine/params.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

int main(void)
{
    printf("0xSYNTH Performance Benchmark\n");
    printf("==============================\n\n");

    uint32_t sample_rate = 44100;
    uint32_t buffer_size = 256;
    double audio_seconds = 60.0;
    uint32_t total_frames = (uint32_t)(audio_seconds * sample_rate);
    uint32_t num_blocks = total_frames / buffer_size;

    oxs_synth_t *s = oxs_synth_create(sample_rate);

    /* Configure: SuperSaw with effects */
    oxs_synth_set_param(s, OXS_PARAM_OSC1_WAVE, 0.0f);  /* saw */
    oxs_synth_set_param(s, OXS_PARAM_OSC2_WAVE, 0.0f);
    oxs_synth_set_param(s, OXS_PARAM_OSC_MIX, 0.5f);
    oxs_synth_set_param(s, OXS_PARAM_UNISON_VOICES, 5.0f);
    oxs_synth_set_param(s, OXS_PARAM_UNISON_DETUNE, 20.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.01f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 0.8f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_RELEASE, 0.5f);
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 5000.0f);
    oxs_synth_set_param(s, OXS_PARAM_LFO_RATE, 0.3f);
    oxs_synth_set_param(s, OXS_PARAM_LFO_DEPTH, 0.1f);
    oxs_synth_set_param(s, OXS_PARAM_LFO_DEST, 2.0f); /* filter */

    /* Enable effects: reverb + delay + overdrive */
    oxs_synth_set_param(s, OXS_PARAM_EFX0_TYPE, 3.0f);  /* reverb */
    oxs_synth_set_param(s, OXS_PARAM_EFX1_TYPE, 2.0f);  /* delay */
    oxs_synth_set_param(s, OXS_PARAM_EFX2_TYPE, 4.0f);  /* overdrive */

    /* Trigger 16 voices (full polyphony) */
    for (int i = 0; i < 16; i++) {
        oxs_synth_note_on(s, (uint8_t)(48 + (i % 12)), 100, 0);
    }

    float *buf = malloc(buffer_size * 2 * sizeof(float));
    int has_nan = 0;

    /* Warm up */
    for (int i = 0; i < 10; i++)
        oxs_synth_process(s, buf, buffer_size);

    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (uint32_t block = 0; block < num_blocks; block++) {
        oxs_synth_process(s, buf, buffer_size);

        /* Spot-check for NaN */
        if (block % 1000 == 0) {
            for (uint32_t i = 0; i < buffer_size * 2; i++) {
                if (isnan(buf[i]) || isinf(buf[i])) { has_nan = 1; break; }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double wall_time = (double)(end.tv_sec - start.tv_sec) +
                       (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    double rt_ratio = audio_seconds / wall_time;

    printf("Config:\n");
    printf("  Sample rate:  %u Hz\n", sample_rate);
    printf("  Buffer size:  %u frames\n", buffer_size);
    printf("  Voices:       16 (5-voice unison each)\n");
    printf("  Effects:      reverb + delay + overdrive\n");
    printf("  Duration:     %.0f seconds of audio\n\n", audio_seconds);

    printf("Results:\n");
    printf("  Wall time:    %.3f seconds\n", wall_time);
    printf("  RT ratio:     %.1fx real-time\n", rt_ratio);
    printf("  NaN/Inf:      %s\n", has_nan ? "DETECTED" : "none");
    printf("\n");

    if (rt_ratio >= 10.0 && !has_nan) {
        printf("PASS: %.1fx real-time (target: >10x)\n", rt_ratio);
    } else if (has_nan) {
        printf("FAIL: NaN/Inf detected in output\n");
    } else {
        printf("WARN: %.1fx real-time (below 10x target)\n", rt_ratio);
    }

    free(buf);
    oxs_synth_destroy(s);

    return (rt_ratio >= 10.0 && !has_nan) ? 0 : 1;
}
