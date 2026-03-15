/*
 * 0xSYNTH Preset Management Tests (Phase 5)
 */

#include "synth_api.h"
#include "../src/engine/params.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-55s ", name); } while (0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while (0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

static float buffer_rms(const float *buf, uint32_t n)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < n * 2; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / (float)(n * 2));
}

/* ===== Tests ===== */

static void test_save_load_roundtrip(void)
{
    TEST("save and load preset round-trip");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Set some non-default values */
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 1234.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.05f);
    oxs_synth_set_param(s, OXS_PARAM_SYNTH_MODE, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_FM_ALGORITHM, 3.0f);

    /* Assign a CC */
    oxs_synth_cc_assign(s, 74, OXS_PARAM_FILTER_CUTOFF);

    /* Save */
    const char *path = "/tmp/oxs_test_preset.json";
    ASSERT(oxs_synth_preset_save(s, path, "Test", "Tester", "Test"), "save failed");

    /* Create fresh synth and load */
    oxs_synth_t *s2 = oxs_synth_create(44100);
    ASSERT(oxs_synth_preset_load(s2, path), "load failed");

    /* Verify values match */
    float cutoff = oxs_synth_get_param(s2, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(fabsf(cutoff - 1234.0f) < 1.0f, "cutoff didn't round-trip");

    float attack = oxs_synth_get_param(s2, OXS_PARAM_AMP_ATTACK);
    ASSERT(fabsf(attack - 0.05f) < 0.001f, "attack didn't round-trip");

    float mode = oxs_synth_get_param(s2, OXS_PARAM_SYNTH_MODE);
    ASSERT(fabsf(mode - 1.0f) < 0.1f, "synth mode didn't round-trip");

    float alg = oxs_synth_get_param(s2, OXS_PARAM_FM_ALGORITHM);
    ASSERT(fabsf(alg - 3.0f) < 0.1f, "FM algorithm didn't round-trip");

    /* Clean up */
    unlink(path);
    oxs_synth_destroy(s);
    oxs_synth_destroy(s2);
    PASS();
}

static void test_render_roundtrip(void)
{
    TEST("preset load produces same audio as original params");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Configure a specific sound */
    oxs_synth_set_param(s, OXS_PARAM_OSC1_WAVE, 0.0f);
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 2000.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    /* Render */
    oxs_synth_note_on(s, 60, 100, 0);
    float buf1[2048];
    float skip[512];
    oxs_synth_process(s, skip, 256);
    oxs_synth_process(s, buf1, 1024);
    float rms1 = buffer_rms(buf1, 1024);
    oxs_synth_destroy(s);

    /* Save the same params */
    s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_OSC1_WAVE, 0.0f);
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 2000.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    const char *path = "/tmp/oxs_test_render.json";
    oxs_synth_preset_save(s, path, "RenderTest", NULL, NULL);
    oxs_synth_destroy(s);

    /* Load into fresh synth and render */
    s = oxs_synth_create(44100);
    ASSERT(oxs_synth_preset_load(s, path), "load failed");

    oxs_synth_note_on(s, 60, 100, 0);
    float buf2[2048];
    oxs_synth_process(s, skip, 256);
    oxs_synth_process(s, buf2, 1024);
    float rms2 = buffer_rms(buf2, 1024);

    /* RMS should be very close (same params → same output) */
    ASSERT(fabsf(rms1 - rms2) < 0.01f, "rendered output should match after preset load");

    unlink(path);
    oxs_synth_destroy(s);
    PASS();
}

static void test_factory_presets_load(void)
{
    TEST("factory presets load and produce audio");
    char *names[64];
    int count = oxs_synth_preset_list("../presets/factory", names, 64);
    ASSERT(count >= 3, "should have at least 3 factory presets");

    int all_play = 1;
    for (int i = 0; i < count; i++) {
        char path[256];
        snprintf(path, sizeof(path), "../presets/factory/%s.json", names[i]);

        oxs_synth_t *s = oxs_synth_create(44100);
        ASSERT(oxs_synth_preset_load(s, path), "factory preset load failed");

        oxs_synth_note_on(s, 60, 100, 0);
        float buf[2048];
        oxs_synth_process(s, buf, 256); /* skip attack */
        oxs_synth_process(s, buf, 1024);
        float rms = buffer_rms(buf, 1024);

        if (rms < 0.001f) {
            printf("\n    WARNING: %s is silent", names[i]);
            all_play = 0;
        }

        oxs_synth_destroy(s);
        free(names[i]);
    }

    ASSERT(all_play, "all factory presets should produce audio");
    PASS();
}

static void test_preset_list(void)
{
    TEST("preset list finds JSON files");
    char *names[64];
    int count = oxs_synth_preset_list("../presets/factory", names, 64);
    ASSERT(count >= 3, "should find factory presets");

    /* Verify sorted */
    int sorted = 1;
    for (int i = 1; i < count; i++) {
        if (strcmp(names[i - 1], names[i]) > 0) { sorted = 0; break; }
    }
    ASSERT(sorted, "preset list should be sorted");

    for (int i = 0; i < count; i++) free(names[i]);
    PASS();
}

static void test_malformed_json_rejected(void)
{
    TEST("malformed JSON is rejected gracefully");
    /* Write garbage JSON */
    const char *path = "/tmp/oxs_test_bad.json";
    FILE *f = fopen(path, "w");
    fputs("{not valid json!!!", f);
    fclose(f);

    oxs_synth_t *s = oxs_synth_create(44100);
    bool loaded = oxs_synth_preset_load(s, path);
    ASSERT(!loaded, "should reject malformed JSON");

    unlink(path);
    oxs_synth_destroy(s);
    PASS();
}

static void test_missing_file_rejected(void)
{
    TEST("missing file returns false");
    oxs_synth_t *s = oxs_synth_create(44100);
    bool loaded = oxs_synth_preset_load(s, "/tmp/nonexistent_preset_12345.json");
    ASSERT(!loaded, "should return false for missing file");
    oxs_synth_destroy(s);
    PASS();
}

static void test_param_range_clamping(void)
{
    TEST("preset load clamps out-of-range values");
    /* Write preset with out-of-range values */
    const char *path = "/tmp/oxs_test_range.json";
    FILE *f = fopen(path, "w");
    fputs("{ \"name\": \"test\", \"params\": { \"Filter Cutoff\": 99999, \"Amp Sustain\": -5.0 } }", f);
    fclose(f);

    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_preset_load(s, path);

    float cutoff = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(cutoff <= 20000.0f, "cutoff should be clamped to max");

    float sustain = oxs_synth_get_param(s, OXS_PARAM_AMP_SUSTAIN);
    ASSERT(sustain >= 0.0f, "sustain should be clamped to min");

    unlink(path);
    oxs_synth_destroy(s);
    PASS();
}

static void test_user_dir_exists(void)
{
    TEST("user preset directory is accessible");
    const char *dir = oxs_synth_preset_user_dir();
    ASSERT(dir != NULL, "user dir should not be NULL");
    ASSERT(strlen(dir) > 0, "user dir should not be empty");
    /* Directory should have been created */
    struct stat st;
    ASSERT(stat(dir, &st) == 0, "user dir should exist");
    PASS();
}

int main(void)
{
    printf("0xSYNTH Preset Management Tests\n");
    printf("================================\n\n");

    test_save_load_roundtrip();
    test_render_roundtrip();
    test_factory_presets_load();
    test_preset_list();
    test_malformed_json_rejected();
    test_missing_file_rejected();
    test_param_range_clamping();
    test_user_dir_exists();

    printf("\n================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
