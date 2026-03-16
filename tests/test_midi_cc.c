/*
 * 0xSYNTH MIDI CC Learn Tests (Phase 10)
 */

#include "synth_api.h"
#include "../src/engine/params.h"

#include <stdio.h>
#include <math.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-55s ", name); } while (0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while (0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

static void test_cc_assign_and_process(void)
{
    TEST("CC assignment scales and sets param");
    oxs_synth_t *s = oxs_synth_create(44100);

    oxs_synth_cc_assign(s, 74, OXS_PARAM_FILTER_CUTOFF);
    oxs_synth_midi_cc(s, 74, 64); /* midpoint */

    float buf[512];
    oxs_synth_process(s, buf, 256); /* drain queue */

    float cutoff = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    /* 64/127 * (20000-20) + 20 ≈ 10089 */
    ASSERT(cutoff > 9000.0f && cutoff < 11000.0f, "CC should scale to param range");

    oxs_synth_destroy(s);
    PASS();
}

static void test_cc_zero_and_max(void)
{
    TEST("CC 0 maps to min, CC 127 maps to max");
    oxs_synth_t *s = oxs_synth_create(44100);
    float buf[512];

    oxs_synth_cc_assign(s, 1, OXS_PARAM_FILTER_CUTOFF);

    oxs_synth_midi_cc(s, 1, 0);
    oxs_synth_process(s, buf, 256);
    float min_val = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(min_val < 25.0f, "CC 0 should map near param min");

    oxs_synth_midi_cc(s, 1, 127);
    oxs_synth_process(s, buf, 256);
    float max_val = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(max_val > 19900.0f, "CC 127 should map near param max");

    oxs_synth_destroy(s);
    PASS();
}

static void test_unassigned_cc_ignored(void)
{
    TEST("unassigned CC is ignored");
    oxs_synth_t *s = oxs_synth_create(44100);

    float before = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    oxs_synth_midi_cc(s, 99, 64); /* CC 99 not assigned */
    float buf[512];
    oxs_synth_process(s, buf, 256);
    float after = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);

    ASSERT(fabsf(before - after) < 0.01f, "unassigned CC should not change param");

    oxs_synth_destroy(s);
    PASS();
}

static void test_midi_learn_mode(void)
{
    TEST("MIDI learn auto-assigns next CC to param");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Start learning filter cutoff */
    oxs_synth_midi_learn_start(s, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(oxs_synth_midi_learn_active(s) == OXS_PARAM_FILTER_CUTOFF,
           "learn should be active");

    /* Send CC 22 — should auto-assign to filter cutoff */
    oxs_synth_midi_cc(s, 22, 100);
    float buf[512];
    oxs_synth_process(s, buf, 256);

    /* Learn mode should have exited */
    ASSERT(oxs_synth_midi_learn_active(s) == -1, "learn should exit after CC");

    /* CC 22 should now control filter cutoff */
    oxs_synth_midi_cc(s, 22, 64);
    oxs_synth_process(s, buf, 256);
    float cutoff = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(cutoff > 9000.0f && cutoff < 11000.0f, "learned CC should control param");

    oxs_synth_destroy(s);
    PASS();
}

static void test_midi_learn_cancel(void)
{
    TEST("MIDI learn can be cancelled");
    oxs_synth_t *s = oxs_synth_create(44100);

    oxs_synth_midi_learn_start(s, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(oxs_synth_midi_learn_active(s) >= 0, "should be learning");

    oxs_synth_midi_learn_cancel(s);
    ASSERT(oxs_synth_midi_learn_active(s) == -1, "should be cancelled");

    oxs_synth_destroy(s);
    PASS();
}

static void test_cc_unassign(void)
{
    TEST("CC unassign stops controlling param");
    oxs_synth_t *s = oxs_synth_create(44100);
    float buf[512];

    oxs_synth_cc_assign(s, 74, OXS_PARAM_FILTER_CUTOFF);
    oxs_synth_midi_cc(s, 74, 64);
    oxs_synth_process(s, buf, 256);
    float val1 = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);

    oxs_synth_cc_unassign(s, 74);

    /* Reset cutoff to default */
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 20000.0f);

    /* Send CC again — should be ignored now */
    oxs_synth_midi_cc(s, 74, 32);
    oxs_synth_process(s, buf, 256);
    float val2 = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);

    ASSERT(fabsf(val2 - 20000.0f) < 1.0f, "unassigned CC should not change param");
    (void)val1;

    oxs_synth_destroy(s);
    PASS();
}

static void test_cc_persists_in_preset(void)
{
    TEST("CC mappings persist in preset save/load");
    oxs_synth_t *s = oxs_synth_create(44100);

    oxs_synth_cc_assign(s, 74, OXS_PARAM_FILTER_CUTOFF);
    oxs_synth_cc_assign(s, 1, OXS_PARAM_LFO_DEPTH);

    const char *path = "/tmp/oxs_test_cc_preset.json";
    oxs_synth_preset_save(s, path, "CC Test", NULL, NULL);

    /* Load into fresh synth */
    oxs_synth_t *s2 = oxs_synth_create(44100);
    oxs_synth_preset_load(s2, path);

    /* Send CC 74 — should control cutoff */
    oxs_synth_midi_cc(s2, 74, 64);
    float buf[512];
    oxs_synth_process(s2, buf, 256);
    float cutoff = oxs_synth_get_param(s2, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(cutoff > 9000.0f && cutoff < 11000.0f, "CC mapping should persist");

    unlink(path);
    oxs_synth_destroy(s);
    oxs_synth_destroy(s2);
    PASS();
}

int main(void)
{
    printf("0xSYNTH MIDI CC Learn Tests\n");
    printf("============================\n\n");

    test_cc_assign_and_process();
    test_cc_zero_and_max();
    test_unassigned_cc_ignored();
    test_midi_learn_mode();
    test_midi_learn_cancel();
    test_cc_unassign();
    test_cc_persists_in_preset();

    printf("\n============================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
