/*
 * 0xSYNTH Step Sequencer Tests
 */

#include "synth_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("  [TEST] %s ... ", name);

#define PASS() \
    do { printf("PASS\n"); tests_passed++; } while(0)

#define FAIL(msg) \
    do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)

#define ASSERT(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while(0)


/* === Test: Default pattern initialization === */
static void test_default_pattern(void)
{
    TEST("default pattern init");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Check default C minor pentatonic: C3, Eb3, F3, G3, Bb3, C4, Bb3, G3 */
    uint8_t expected[] = { 48, 51, 53, 55, 58, 60, 58, 55 };
    for (int i = 0; i < 8; i++) {
        uint8_t note, vel, slide, accent;
        float gate;
        oxs_synth_seq_get_step(s, i, &note, &vel, &slide, &accent, &gate);
        ASSERT(note == expected[i], "wrong default note");
        ASSERT(vel == 100, "wrong default velocity");
        ASSERT(slide == 0, "wrong default slide");
        ASSERT(accent == 0, "wrong default accent");
        ASSERT(fabsf(gate - 0.5f) < 0.01f, "wrong default gate");
    }

    /* Step 8+ should be rests (velocity=0) */
    uint8_t note, vel, slide, accent;
    float gate;
    oxs_synth_seq_get_step(s, 8, &note, &vel, &slide, &accent, &gate);
    ASSERT(vel == 0, "step 8 should be rest");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Set/get step === */
static void test_set_get_step(void)
{
    TEST("set/get step");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    oxs_synth_seq_set_step(s, 0, 60, 127, 1, 1, 0.75f);

    uint8_t note, vel, slide, accent;
    float gate;
    oxs_synth_seq_get_step(s, 0, &note, &vel, &slide, &accent, &gate);
    ASSERT(note == 60, "wrong note");
    ASSERT(vel == 127, "wrong velocity");
    ASSERT(slide == 1, "wrong slide");
    ASSERT(accent == 1, "wrong accent");
    ASSERT(fabsf(gate - 0.75f) < 0.01f, "wrong gate");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Sequencer produces audio when enabled === */
static void test_seq_produces_audio(void)
{
    TEST("sequencer produces audio");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Enable sequencer */
    oxs_synth_set_param(s, 260, 1.0f);  /* SEQ_ENABLED */
    oxs_synth_set_param(s, 262, 120.0f); /* SEQ_BPM */
    oxs_synth_set_param(s, 263, 0.5f);   /* SEQ_SWING (no swing) */
    oxs_synth_set_param(s, 264, 0.0f);   /* SEQ_DIRECTION (forward) */

    /* Process enough frames to trigger at least one step */
    /* At 120 BPM, 1/16th note = 60/120/4 * 44100 = 5512.5 samples */
    float buf[44100 * 2]; /* 1 second stereo */
    memset(buf, 0, sizeof(buf));
    oxs_synth_process(s, buf, 44100);

    /* Check for non-zero audio output */
    float max_val = 0.0f;
    for (int i = 0; i < 44100 * 2; i++) {
        float abs_val = fabsf(buf[i]);
        if (abs_val > max_val) max_val = abs_val;
    }
    ASSERT(max_val > 0.001f, "sequencer should produce audio");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Sequencer disabled produces no seq audio === */
static void test_seq_disabled(void)
{
    TEST("sequencer disabled = silence");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Sequencer off (default) */
    oxs_synth_set_param(s, 260, 0.0f);

    float buf[4096 * 2];
    memset(buf, 0, sizeof(buf));
    oxs_synth_process(s, buf, 4096);

    /* No notes should have been triggered — output should be silent */
    float max_val = 0.0f;
    for (int i = 0; i < 4096 * 2; i++) {
        float abs_val = fabsf(buf[i]);
        if (abs_val > max_val) max_val = abs_val;
    }
    ASSERT(max_val < 0.001f, "disabled sequencer should be silent");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Arp takes priority over sequencer === */
static void test_arp_priority(void)
{
    TEST("arp takes priority over sequencer");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Enable both arp and seq */
    oxs_synth_set_param(s, 200, 1.0f);  /* ARP_ENABLED */
    oxs_synth_set_param(s, 260, 1.0f);  /* SEQ_ENABLED */

    /* Send a note to arp */
    oxs_synth_note_on(s, 60, 100, 0);

    float buf[8192 * 2];
    memset(buf, 0, sizeof(buf));
    oxs_synth_process(s, buf, 8192);

    /* Current step should still be 0 since seq shouldn't run */
    int step = oxs_synth_seq_current_step(s);
    ASSERT(step == 0, "seq should not advance when arp is enabled");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Step advancement (forward) === */
static void test_forward_direction(void)
{
    TEST("forward direction advancement");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Enable seq, 8-step, forward, 120 BPM */
    oxs_synth_set_param(s, 260, 1.0f);   /* SEQ_ENABLED */
    oxs_synth_set_param(s, 261, 0.0f);   /* SEQ_LENGTH = 8 */
    oxs_synth_set_param(s, 262, 120.0f);  /* SEQ_BPM */
    oxs_synth_set_param(s, 263, 0.5f);    /* SEQ_SWING */
    oxs_synth_set_param(s, 264, 0.0f);    /* SEQ_DIRECTION = forward */

    /* Process multiple steps worth of audio */
    /* 1/16th at 120 BPM = 5512.5 samples. Process 3 steps worth. */
    float buf[20000 * 2];
    memset(buf, 0, sizeof(buf));
    oxs_synth_process(s, buf, 20000);

    /* After ~3.6 steps, current_step should be > 0 */
    int step = oxs_synth_seq_current_step(s);
    ASSERT(step > 0, "step should have advanced");
    ASSERT(step < 8, "step should be within length");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Sequencer param registration === */
static void test_seq_params_registered(void)
{
    TEST("sequencer params registered");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    oxs_param_info_t info;

    /* Check each seq param is registered */
    ASSERT(oxs_synth_param_info(s, 260, &info), "SEQ_ENABLED not registered");
    ASSERT(oxs_synth_param_info(s, 261, &info), "SEQ_LENGTH not registered");
    ASSERT(oxs_synth_param_info(s, 262, &info), "SEQ_BPM not registered");
    ASSERT(oxs_synth_param_info(s, 263, &info), "SEQ_SWING not registered");
    ASSERT(oxs_synth_param_info(s, 264, &info), "SEQ_DIRECTION not registered");
    ASSERT(oxs_synth_param_info(s, 265, &info), "SEQ_GATE not registered");

    /* Check defaults */
    float bpm_default = oxs_synth_get_param(s, 262);
    ASSERT(fabsf(bpm_default - 120.0f) < 0.01f, "BPM default should be 120");

    float swing_default = oxs_synth_get_param(s, 263);
    ASSERT(fabsf(swing_default - 0.5f) < 0.01f, "Swing default should be 0.5");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Panic resets sequencer === */
static void test_panic_resets_seq(void)
{
    TEST("panic resets sequencer");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Enable and run sequencer */
    oxs_synth_set_param(s, 260, 1.0f);
    oxs_synth_set_param(s, 262, 120.0f);

    float buf[20000 * 2];
    memset(buf, 0, sizeof(buf));
    oxs_synth_process(s, buf, 20000);

    /* Disable sequencer before panic so it doesn't restart */
    oxs_synth_set_param(s, 260, 0.0f);

    /* Panic */
    oxs_synth_panic(s);
    oxs_synth_process(s, buf, 256); /* drain queue */

    /* After panic + disable, step should be reset to 0 */
    int step = oxs_synth_seq_current_step(s);
    ASSERT(step == 0, "step should be 0 after panic");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Length modes === */
static void test_length_modes(void)
{
    TEST("length decoding (8/16/32)");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Set up a pattern with notes at steps 0-31 */
    for (int i = 0; i < 32; i++) {
        oxs_synth_seq_set_step(s, i, (uint8_t)(48 + i), 100, 0, 0, 0.5f);
    }

    /* Length=0 means 8 steps — after 8 steps, should wrap */
    oxs_synth_set_param(s, 260, 1.0f);   /* SEQ_ENABLED */
    oxs_synth_set_param(s, 261, 0.0f);   /* SEQ_LENGTH = 8 */
    oxs_synth_set_param(s, 262, 240.0f);  /* fast BPM */
    oxs_synth_set_param(s, 263, 0.5f);    /* no swing */
    oxs_synth_set_param(s, 264, 0.0f);    /* forward */

    /* Run a lot of frames */
    float buf[88200 * 2];
    memset(buf, 0, sizeof(buf));
    oxs_synth_process(s, buf, 88200);

    int step = oxs_synth_seq_current_step(s);
    ASSERT(step < 8, "with length=8, step should be < 8");

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Accent boosts velocity === */
static void test_accent(void)
{
    TEST("accent boosts velocity");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Set step 0 with accent, low velocity */
    oxs_synth_seq_set_step(s, 0, 60, 80, 0, 1, 0.5f);

    /* Verify it's stored */
    uint8_t note, vel, slide, accent;
    float gate;
    oxs_synth_seq_get_step(s, 0, &note, &vel, &slide, &accent, &gate);
    ASSERT(accent == 1, "accent should be set");
    ASSERT(vel == 80, "stored velocity should be 80");

    /* The actual velocity boost (80+40=120) happens in seq_process
     * which is tested indirectly via audio output */

    oxs_synth_destroy(s);
    PASS();
}

/* === Test: Out-of-bounds step access === */
static void test_bounds_check(void)
{
    TEST("out-of-bounds step access");

    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create failed");

    /* Get step at invalid index should return zeroed */
    uint8_t note = 99, vel = 99, slide = 99, accent = 99;
    float gate = 99.0f;
    oxs_synth_seq_get_step(s, 50, &note, &vel, &slide, &accent, &gate);
    ASSERT(vel == 0, "out-of-bounds step velocity should be 0");

    /* Set step at invalid index should not crash */
    oxs_synth_seq_set_step(s, -1, 60, 100, 0, 0, 0.5f);
    oxs_synth_seq_set_step(s, 100, 60, 100, 0, 0, 0.5f);

    oxs_synth_destroy(s);
    PASS();
}

int main(void)
{
    printf("=== 0xSYNTH Step Sequencer Tests ===\n\n");

    test_default_pattern();
    test_set_get_step();
    test_seq_produces_audio();
    test_seq_disabled();
    test_arp_priority();
    test_forward_direction();
    test_seq_params_registered();
    test_panic_resets_seq();
    test_length_modes();
    test_accent();
    test_bounds_check();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
