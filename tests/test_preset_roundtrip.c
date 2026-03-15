/*
 * 0xSYNTH Preset Round-Trip Test (Phase 12)
 *
 * For every factory preset: load → render → save → reload → render → compare.
 */

#include "synth_api.h"
#include "../src/engine/params.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

int main(void)
{
    printf("0xSYNTH Preset Round-Trip Test\n");
    printf("===============================\n\n");

    char *names[128];
    int count = oxs_synth_preset_list("../presets/factory", names, 128);
    printf("Testing %d factory presets...\n\n", count);

    int passed = 0;
    int failed = 0;

    for (int i = 0; i < count; i++) {
        char path[256];
        snprintf(path, sizeof(path), "../presets/factory/%s.json", names[i]);

        /* Load and render */
        oxs_synth_t *s1 = oxs_synth_create(44100);
        if (!oxs_synth_preset_load(s1, path)) {
            printf("  FAIL: %s — load failed\n", names[i]);
            failed++;
            oxs_synth_destroy(s1);
            free(names[i]);
            continue;
        }

        oxs_synth_note_on(s1, 60, 100, 0);
        float *buf1 = malloc(1024 * 2 * sizeof(float));
        /* Render enough for slow-attack presets */
        for (int b = 0; b < 10; b++)
            oxs_synth_process(s1, buf1, 1024);

        /* Save to temp */
        const char *tmp = "/tmp/oxs_roundtrip.json";
        oxs_synth_preset_save(s1, tmp, names[i], "test", "test");
        oxs_synth_destroy(s1);

        /* Reload and render */
        oxs_synth_t *s2 = oxs_synth_create(44100);
        if (!oxs_synth_preset_load(s2, tmp)) {
            printf("  FAIL: %s — reload failed\n", names[i]);
            failed++;
            oxs_synth_destroy(s2);
            free(buf1);
            free(names[i]);
            continue;
        }

        oxs_synth_note_on(s2, 60, 100, 0);
        float *buf2 = malloc(1024 * 2 * sizeof(float));
        for (int b = 0; b < 10; b++)
            oxs_synth_process(s2, buf2, 1024);

        /* Compare RMS — should be very close */
        float rms1 = 0, rms2 = 0;
        for (int j = 0; j < 2048; j++) {
            rms1 += buf1[j] * buf1[j];
            rms2 += buf2[j] * buf2[j];
        }
        rms1 = sqrtf(rms1 / 2048.0f);
        rms2 = sqrtf(rms2 / 2048.0f);

        float diff = fabsf(rms1 - rms2);
        if (diff < 0.01f) {
            passed++;
        } else {
            printf("  FAIL: %s — RMS diff %.4f (%.4f vs %.4f)\n",
                   names[i], diff, rms1, rms2);
            failed++;
        }

        free(buf1);
        free(buf2);
        oxs_synth_destroy(s2);
        unlink(tmp);
        free(names[i]);
    }

    printf("\n===============================\n");
    printf("Results: %d/%d passed", passed, passed + failed);
    if (failed > 0) printf(" (%d failed)", failed);
    printf("\n");

    return (failed == 0) ? 0 : 1;
}
