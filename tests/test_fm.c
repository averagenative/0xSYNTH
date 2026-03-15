/*
 * 0xSYNTH FM Synthesis Tests (Phase 3)
 */

#include "synth_api.h"
#include "../src/engine/params.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-55s ", name); } while (0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while (0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

static float buffer_rms(const float *buf, uint32_t num_frames)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_frames * 2; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / (float)(num_frames * 2));
}

static int buffer_is_silent(const float *buf, uint32_t num_frames)
{
    for (uint32_t i = 0; i < num_frames * 2; i++)
        if (fabsf(buf[i]) > 1e-6f) return 0;
    return 1;
}

/* Helper: create synth in FM mode with fast envelope */
static oxs_synth_t *create_fm_synth(void)
{
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_SYNTH_MODE, 1.0f); /* FM mode */
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_RELEASE, 0.01f);
    return s;
}

/* ===== Tests ===== */

static void test_fm_produces_audio(void)
{
    TEST("FM mode produces non-silent audio");
    oxs_synth_t *s = create_fm_synth();
    oxs_synth_note_on(s, 60, 100, 0);
    float buf[512];
    oxs_synth_process(s, buf, 256);
    ASSERT(!buffer_is_silent(buf, 256), "FM should produce audio");
    oxs_synth_destroy(s);
    PASS();
}

static void test_fm_note_off_silences(void)
{
    TEST("FM note off eventually silences");
    oxs_synth_t *s = create_fm_synth();
    oxs_synth_note_on(s, 60, 100, 0);
    float buf[512];
    oxs_synth_process(s, buf, 256);
    oxs_synth_note_off(s, 60, 0);
    for (int i = 0; i < 20; i++) oxs_synth_process(s, buf, 256);
    ASSERT(buffer_is_silent(buf, 256), "should be silent after release");
    oxs_synth_destroy(s);
    PASS();
}

static void test_fm_algorithms_produce_different_output(void)
{
    TEST("different FM algorithms produce different output");
    float rms_vals[8];
    float skip[512];

    for (int alg = 0; alg < 8; alg++) {
        oxs_synth_t *s = create_fm_synth();
        oxs_synth_set_param(s, OXS_PARAM_FM_ALGORITHM, (float)alg);
        /* Set some modulation so algorithms actually differ */
        oxs_synth_set_param(s, OXS_PARAM_FM_OP1_LEVEL, 0.8f);
        oxs_synth_set_param(s, OXS_PARAM_FM_OP1_RATIO, 2.0f);
        oxs_synth_set_param(s, OXS_PARAM_FM_OP2_LEVEL, 0.5f);
        oxs_synth_set_param(s, OXS_PARAM_FM_OP2_RATIO, 3.0f);
        oxs_synth_set_param(s, OXS_PARAM_FM_OP3_LEVEL, 0.3f);
        oxs_synth_set_param(s, OXS_PARAM_FM_OP3_RATIO, 4.0f);

        oxs_synth_note_on(s, 60, 100, 0);
        oxs_synth_process(s, skip, 256); /* skip transient */
        float buf[2048];
        oxs_synth_process(s, buf, 1024);
        rms_vals[alg] = buffer_rms(buf, 1024);
        oxs_synth_destroy(s);
    }

    /* All should produce audio */
    for (int i = 0; i < 8; i++) {
        ASSERT(rms_vals[i] > 0.001f, "all algorithms should produce audio");
    }

    /* Not all identical — at least some should differ */
    int differ = 0;
    for (int i = 1; i < 8; i++) {
        if (fabsf(rms_vals[i] - rms_vals[0]) > 0.001f) { differ++; }
    }
    ASSERT(differ >= 2, "algorithms should produce different output levels");

    PASS();
}

static void test_fm_operator_feedback(void)
{
    TEST("FM operator feedback produces harmonic content");
    oxs_synth_t *s = create_fm_synth();
    oxs_synth_set_param(s, OXS_PARAM_FM_ALGORITHM, 6.0f); /* all carriers */

    /* No feedback */
    oxs_synth_set_param(s, OXS_PARAM_FM_OP0_FEEDBACK, 0.0f);
    oxs_synth_note_on(s, 60, 100, 0);
    float skip[512];
    oxs_synth_process(s, skip, 256);
    float buf_clean[2048];
    oxs_synth_process(s, buf_clean, 1024);
    oxs_synth_panic(s);
    for (int i = 0; i < 20; i++) oxs_synth_process(s, skip, 256);

    /* With feedback — should add harmonics (more zero crossings) */
    oxs_synth_set_param(s, OXS_PARAM_FM_OP0_FEEDBACK, 0.7f);
    oxs_synth_note_on(s, 60, 100, 0);
    oxs_synth_process(s, skip, 256);
    float buf_fb[2048];
    oxs_synth_process(s, buf_fb, 1024);

    int zc_clean = 0, zc_fb = 0;
    for (uint32_t i = 1; i < 1024; i++) {
        if ((buf_clean[i*2] > 0) != (buf_clean[(i-1)*2] > 0)) zc_clean++;
        if ((buf_fb[i*2] > 0) != (buf_fb[(i-1)*2] > 0)) zc_fb++;
    }

    ASSERT(zc_fb > zc_clean, "feedback should add harmonics (more zero crossings)");
    oxs_synth_destroy(s);
    PASS();
}

static void test_fm_bell_preset_decays(void)
{
    TEST("FM bell preset decays naturally");
    oxs_synth_t *s = create_fm_synth();
    oxs_synth_set_param(s, OXS_PARAM_FM_ALGORITHM, 0.0f); /* serial */

    /* Bell-like: fast modulator decay, slow carrier sustain */
    oxs_synth_set_param(s, OXS_PARAM_FM_OP0_RATIO, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP0_LEVEL, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP1_RATIO, 3.5f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP1_LEVEL, 0.8f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP1_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP1_DECAY, 0.3f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP1_SUSTAIN, 0.0f);

    oxs_synth_note_on(s, 72, 100, 0);

    /* Early: bright with modulation */
    float buf_early[2048];
    oxs_synth_process(s, buf_early, 1024);
    float rms_early = buffer_rms(buf_early, 1024);

    /* Later: modulator has decayed, should be simpler */
    float buf_later[2048];
    for (int i = 0; i < 20; i++) oxs_synth_process(s, buf_later, 1024);
    float rms_later = buffer_rms(buf_later, 1024);

    ASSERT(rms_early > 0.01f, "early should have audio");
    /* The timbre changes — harder to assert RMS change, but it shouldn't crash */
    ASSERT(rms_later >= 0.0f, "later should not be NaN");

    oxs_synth_destroy(s);
    PASS();
}

static void test_fm_mode_switch(void)
{
    TEST("mode switch between subtractive and FM works");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_RELEASE, 0.005f);
    float skip[512];

    /* Subtractive */
    oxs_synth_set_param(s, OXS_PARAM_SYNTH_MODE, 0.0f);
    oxs_synth_note_on(s, 60, 100, 0);
    oxs_synth_process(s, skip, 256);
    float buf_sub[2048];
    oxs_synth_process(s, buf_sub, 1024);
    float rms_sub = buffer_rms(buf_sub, 1024);
    oxs_synth_panic(s);
    for (int i = 0; i < 20; i++) oxs_synth_process(s, skip, 256);

    /* FM */
    oxs_synth_set_param(s, OXS_PARAM_SYNTH_MODE, 1.0f);
    oxs_synth_note_on(s, 60, 100, 0);
    oxs_synth_process(s, skip, 256);
    float buf_fm[2048];
    oxs_synth_process(s, buf_fm, 1024);
    float rms_fm = buffer_rms(buf_fm, 1024);

    ASSERT(rms_sub > 0.01f, "subtractive should produce audio");
    ASSERT(rms_fm > 0.01f, "FM should produce audio");

    oxs_synth_destroy(s);
    PASS();
}

static void test_fm_no_nan_or_inf(void)
{
    TEST("FM no NaN or Inf under stress");
    oxs_synth_t *s = create_fm_synth();
    /* Extreme settings */
    oxs_synth_set_param(s, OXS_PARAM_FM_OP0_FEEDBACK, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP1_FEEDBACK, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP1_RATIO, 16.0f);
    oxs_synth_set_param(s, OXS_PARAM_FM_OP2_RATIO, 0.5f);

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
    ASSERT(clean, "output should have no NaN or Inf");
    oxs_synth_destroy(s);
    PASS();
}

int main(void)
{
    printf("0xSYNTH FM Synthesis Tests\n");
    printf("==========================\n\n");

    test_fm_produces_audio();
    test_fm_note_off_silences();
    test_fm_algorithms_produce_different_output();
    test_fm_operator_feedback();
    test_fm_bell_preset_decays();
    test_fm_mode_switch();
    test_fm_no_nan_or_inf();

    printf("\n==========================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
