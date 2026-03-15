/*
 * 0xSYNTH Wavetable Synthesis Tests (Phase 3)
 */

#include "synth_api.h"
#include "../src/engine/params.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

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

static int buffer_is_silent(const float *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n * 2; i++)
        if (fabsf(buf[i]) > 1e-6f) return 0;
    return 1;
}

static oxs_synth_t *create_wt_synth(void)
{
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_SYNTH_MODE, 2.0f); /* wavetable */
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_RELEASE, 0.01f);
    return s;
}

static void test_wt_produces_audio(void)
{
    TEST("wavetable mode produces non-silent audio");
    oxs_synth_t *s = create_wt_synth();
    oxs_synth_note_on(s, 60, 100, 0);
    float buf[512];
    oxs_synth_process(s, buf, 256);
    ASSERT(!buffer_is_silent(buf, 256), "WT should produce audio");
    oxs_synth_destroy(s);
    PASS();
}

static void test_wt_note_off_silences(void)
{
    TEST("wavetable note off eventually silences");
    oxs_synth_t *s = create_wt_synth();
    oxs_synth_note_on(s, 60, 100, 0);
    float buf[512];
    oxs_synth_process(s, buf, 256);
    oxs_synth_note_off(s, 60, 0);
    for (int i = 0; i < 20; i++) oxs_synth_process(s, buf, 256);
    ASSERT(buffer_is_silent(buf, 256), "should be silent after release");
    oxs_synth_destroy(s);
    PASS();
}

static void test_wt_position_changes_timbre(void)
{
    TEST("wavetable position sweep changes timbre");
    float skip[512];

    /* Position = 0.0 */
    oxs_synth_t *s = create_wt_synth();
    oxs_synth_set_param(s, OXS_PARAM_WT_POSITION, 0.0f);
    oxs_synth_note_on(s, 60, 100, 0);
    oxs_synth_process(s, skip, 256);
    float buf_start[2048];
    oxs_synth_process(s, buf_start, 1024);
    oxs_synth_destroy(s);

    /* Position = 1.0 */
    s = create_wt_synth();
    oxs_synth_set_param(s, OXS_PARAM_WT_POSITION, 1.0f);
    oxs_synth_note_on(s, 60, 100, 0);
    oxs_synth_process(s, skip, 256);
    float buf_end[2048];
    oxs_synth_process(s, buf_end, 1024);
    oxs_synth_destroy(s);

    /* Compare zero crossing rates — different position = different timbre */
    int zc_start = 0, zc_end = 0;
    for (uint32_t i = 1; i < 1024; i++) {
        if ((buf_start[i*2] > 0) != (buf_start[(i-1)*2] > 0)) zc_start++;
        if ((buf_end[i*2] > 0) != (buf_end[(i-1)*2] > 0)) zc_end++;
    }

    /* They should differ (different waveform shapes) */
    ASSERT(abs(zc_start - zc_end) > 5 ||
           fabsf(buffer_rms(buf_start, 1024) - buffer_rms(buf_end, 1024)) > 0.01f,
           "different positions should produce different timbres");
    PASS();
}

static void test_wt_different_banks(void)
{
    TEST("different wavetable banks produce different output");
    float rms_vals[4];
    float skip[512];

    for (int bank = 0; bank < 4; bank++) {
        oxs_synth_t *s = create_wt_synth();
        oxs_synth_set_param(s, OXS_PARAM_WT_BANK, (float)bank);
        oxs_synth_set_param(s, OXS_PARAM_WT_POSITION, 0.5f);
        oxs_synth_note_on(s, 60, 100, 0);
        oxs_synth_process(s, skip, 256);
        float buf[2048];
        oxs_synth_process(s, buf, 1024);
        rms_vals[bank] = buffer_rms(buf, 1024);
        oxs_synth_destroy(s);
    }

    for (int i = 0; i < 4; i++)
        ASSERT(rms_vals[i] > 0.001f, "all banks should produce audio");

    int differ = 0;
    for (int i = 1; i < 4; i++)
        if (fabsf(rms_vals[i] - rms_vals[0]) > 0.001f) differ++;
    ASSERT(differ >= 1, "banks should differ");
    PASS();
}

static void test_wt_mode_switch(void)
{
    TEST("mode switch sub → FM → wavetable all produce audio");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_RELEASE, 0.005f);
    float skip[512];

    float rms[3];
    for (int mode = 0; mode < 3; mode++) {
        oxs_synth_set_param(s, OXS_PARAM_SYNTH_MODE, (float)mode);
        oxs_synth_note_on(s, 60, 100, 0);
        oxs_synth_process(s, skip, 256);
        float buf[2048];
        oxs_synth_process(s, buf, 1024);
        rms[mode] = buffer_rms(buf, 1024);
        oxs_synth_panic(s);
        for (int i = 0; i < 20; i++) oxs_synth_process(s, skip, 256);
    }

    ASSERT(rms[0] > 0.01f, "subtractive should produce audio");
    ASSERT(rms[1] > 0.01f, "FM should produce audio");
    ASSERT(rms[2] > 0.01f, "wavetable should produce audio");

    oxs_synth_destroy(s);
    PASS();
}

static void test_wt_no_nan_or_inf(void)
{
    TEST("wavetable no NaN or Inf under stress");
    oxs_synth_t *s = create_wt_synth();
    oxs_synth_set_param(s, OXS_PARAM_WT_ENV_DEPTH, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_WT_LFO_DEPTH, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_LFO_DEPTH, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_LFO_RATE, 20.0f);

    for (int n = 0; n < 16; n++)
        oxs_synth_note_on(s, (uint8_t)(40 + n * 3), 127, 0);

    float buf[2048];
    int clean = 1;
    for (int block = 0; block < 50; block++) {
        oxs_synth_process(s, buf, 1024);
        for (int i = 0; i < 2048; i++) {
            if (isnan(buf[i]) || isinf(buf[i])) { clean = 0; break; }
        }
        if (!clean) break;
    }
    ASSERT(clean, "no NaN or Inf");
    oxs_synth_destroy(s);
    PASS();
}

int main(void)
{
    printf("0xSYNTH Wavetable Synthesis Tests\n");
    printf("==================================\n\n");

    test_wt_produces_audio();
    test_wt_note_off_silences();
    test_wt_position_changes_timbre();
    test_wt_different_banks();
    test_wt_mode_switch();
    test_wt_no_nan_or_inf();

    printf("\n==================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
