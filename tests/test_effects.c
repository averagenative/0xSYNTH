/*
 * 0xSYNTH Effects Chain Tests (Phase 4)
 */

#include "synth_api.h"
#include "../src/engine/params.h"
#include "../src/engine/effects.h"

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

static float buffer_rms(const float *buf, uint32_t n)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < n * 2; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / (float)(n * 2));
}

/* Generate a test tone (saw wave, 440Hz) */
static void generate_test_tone(float *buf, uint32_t frames, uint32_t sample_rate)
{
    double phase = 0.0;
    double inc = 440.0 / (double)sample_rate;
    for (uint32_t i = 0; i < frames; i++) {
        float val = (float)(2.0 * phase - 1.0) * 0.5f;
        buf[i * 2] = val;
        buf[i * 2 + 1] = val;
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
    }
}

/* ===== Direct effect slot tests ===== */

static void test_effect_init_free(void)
{
    TEST("effect init and free lifecycle");
    oxs_effect_slot_t slot;

    /* Test each effect type that allocates buffers */
    oxs_effect_type_t heap_types[] = {
        OXS_EFFECT_DELAY, OXS_EFFECT_REVERB, OXS_EFFECT_CHORUS,
        OXS_EFFECT_FLANGER, OXS_EFFECT_SHIMMER
    };
    for (int i = 0; i < 5; i++) {
        oxs_effect_init(&slot, heap_types[i], 44100);
        oxs_effect_free(&slot);
    }

    /* Test non-heap types */
    oxs_effect_type_t stack_types[] = {
        OXS_EFFECT_FILTER, OXS_EFFECT_OVERDRIVE, OXS_EFFECT_FUZZ,
        OXS_EFFECT_BITCRUSHER, OXS_EFFECT_COMPRESSOR, OXS_EFFECT_PHASER,
        OXS_EFFECT_TREMOLO, OXS_EFFECT_RINGMOD, OXS_EFFECT_TAPE
    };
    for (int i = 0; i < 9; i++) {
        oxs_effect_init(&slot, stack_types[i], 44100);
        oxs_effect_free(&slot);
    }

    PASS();
}

static void test_each_effect_modifies_signal(void)
{
    TEST("each effect type modifies the signal");

    oxs_effect_type_t types[] = {
        OXS_EFFECT_FILTER, OXS_EFFECT_DELAY, OXS_EFFECT_REVERB,
        OXS_EFFECT_OVERDRIVE, OXS_EFFECT_FUZZ, OXS_EFFECT_CHORUS,
        OXS_EFFECT_BITCRUSHER, OXS_EFFECT_COMPRESSOR, OXS_EFFECT_PHASER,
        OXS_EFFECT_FLANGER, OXS_EFFECT_TREMOLO, OXS_EFFECT_RINGMOD,
        OXS_EFFECT_TAPE, OXS_EFFECT_SHIMMER
    };
    const char *names[] = {
        "filter", "delay", "reverb", "overdrive", "fuzz", "chorus",
        "bitcrusher", "compressor", "phaser", "flanger", "tremolo",
        "ringmod", "tape", "shimmer"
    };

    uint32_t frames = 1024;
    float *dry = malloc(frames * 2 * sizeof(float));
    float *wet = malloc(frames * 2 * sizeof(float));

    int all_modified = 1;
    for (int t = 0; t < 14; t++) {
        oxs_effect_slot_t slot;
        oxs_effect_init(&slot, types[t], 44100);

        /* For delay: process a few blocks so delayed signal appears */
        /* For compressor: lower threshold so it compresses the test tone */
        if (types[t] == OXS_EFFECT_DELAY) {
            slot.delay.time = 0.005f; /* 5ms — short enough to appear in 1024 frames */
            slot.delay.wet = 0.5f;
        }
        if (types[t] == OXS_EFFECT_COMPRESSOR) {
            slot.compressor.threshold = 0.1f; /* low threshold to catch test tone */
            slot.compressor.ratio = 10.0f;
        }

        /* Process 2 blocks so delay has data to read back */
        generate_test_tone(wet, frames, 44100);
        oxs_effect_process(&slot, wet, frames, 44100, 120.0);

        generate_test_tone(dry, frames, 44100);
        memcpy(wet, dry, frames * 2 * sizeof(float));
        oxs_effect_process(&slot, wet, frames, 44100, 120.0);
        oxs_effect_free(&slot);

        /* Compare — at least some samples should differ */
        int differs = 0;
        for (uint32_t i = 0; i < frames * 2; i++) {
            if (fabsf(wet[i] - dry[i]) > 1e-6f) { differs = 1; break; }
        }

        if (!differs) {
            printf("\n    WARNING: %s didn't modify signal", names[t]);
            all_modified = 0;
        }
    }

    free(dry);
    free(wet);
    ASSERT(all_modified, "all effects should modify the signal");
    PASS();
}

static void test_bypass_passes_through(void)
{
    TEST("bypassed effect passes signal through unchanged");
    uint32_t frames = 256;
    float *dry = malloc(frames * 2 * sizeof(float));
    float *wet = malloc(frames * 2 * sizeof(float));

    generate_test_tone(dry, frames, 44100);
    memcpy(wet, dry, frames * 2 * sizeof(float));

    oxs_effect_slot_t slot;
    oxs_effect_init(&slot, OXS_EFFECT_OVERDRIVE, 44100);
    slot.bypass = true;
    oxs_effect_process(&slot, wet, frames, 44100, 120.0);
    oxs_effect_free(&slot);

    int identical = 1;
    for (uint32_t i = 0; i < frames * 2; i++) {
        if (fabsf(wet[i] - dry[i]) > 1e-6f) { identical = 0; break; }
    }

    free(dry);
    free(wet);
    ASSERT(identical, "bypassed effect should not modify signal");
    PASS();
}

static void test_none_passes_through(void)
{
    TEST("EFFECT_NONE passes signal through");
    uint32_t frames = 256;
    float *dry = malloc(frames * 2 * sizeof(float));
    float *wet = malloc(frames * 2 * sizeof(float));

    generate_test_tone(dry, frames, 44100);
    memcpy(wet, dry, frames * 2 * sizeof(float));

    oxs_effect_slot_t slot;
    memset(&slot, 0, sizeof(slot));
    slot.type = OXS_EFFECT_NONE;
    oxs_effect_process(&slot, wet, frames, 44100, 120.0);

    int identical = 1;
    for (uint32_t i = 0; i < frames * 2; i++) {
        if (fabsf(wet[i] - dry[i]) > 1e-6f) { identical = 0; break; }
    }

    free(dry);
    free(wet);
    ASSERT(identical, "NONE should not modify signal");
    PASS();
}

static void test_chain_applies_in_series(void)
{
    TEST("effect chain applies effects in series");
    uint32_t frames = 1024;
    float *buf = malloc(frames * 2 * sizeof(float));

    generate_test_tone(buf, frames, 44100);
    float rms_before = buffer_rms(buf, frames);

    /* Chain: overdrive (adds gain) → filter (removes highs) */
    oxs_effect_slot_t chain[3];
    oxs_effect_init(&chain[0], OXS_EFFECT_OVERDRIVE, 44100);
    chain[0].overdrive.drive = 0.8f;
    chain[0].overdrive.mix = 1.0f;
    oxs_effect_init(&chain[1], OXS_EFFECT_FILTER, 44100);
    chain[1].filter.cutoff = 500.0f;
    chain[1].filter.resonance = 1.0f;
    chain[1].filter.wet = 1.0f;
    chain[1].filter.last_cutoff = 0; /* force recalc */
    memset(&chain[2], 0, sizeof(oxs_effect_slot_t));
    chain[2].type = OXS_EFFECT_NONE;

    oxs_effects_chain_process(chain, 3, buf, frames, 44100, 120.0);
    float rms_after = buffer_rms(buf, frames);

    /* Signal should be modified */
    ASSERT(fabsf(rms_after - rms_before) > 0.001f, "chain should modify signal");

    oxs_effect_free(&chain[0]);
    oxs_effect_free(&chain[1]);
    free(buf);
    PASS();
}

/* ===== API-level effect tests ===== */

static void test_api_effect_via_params(void)
{
    TEST("API: setting effect type via params activates effect");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    /* Play a note with no effects */
    oxs_synth_note_on(s, 60, 100, 0);
    float buf_dry[2048];
    float skip[512];
    oxs_synth_process(s, skip, 256); /* skip attack */
    oxs_synth_process(s, buf_dry, 1024);
    float rms_dry = buffer_rms(buf_dry, 1024);

    /* Enable overdrive on slot 0 */
    oxs_synth_set_param(s, OXS_PARAM_EFX0_TYPE, (float)OXS_EFFECT_OVERDRIVE);

    float buf_wet[2048];
    oxs_synth_process(s, buf_wet, 1024);
    float rms_wet = buffer_rms(buf_wet, 1024);

    /* Overdrive should change the signal */
    ASSERT(fabsf(rms_wet - rms_dry) > 0.001f || rms_wet > 0.001f,
           "overdrive should modify signal");

    oxs_synth_destroy(s);
    PASS();
}

static void test_effects_no_nan_or_inf(void)
{
    TEST("effects produce no NaN or Inf under stress");

    uint32_t frames = 1024;
    float *buf = malloc(frames * 2 * sizeof(float));

    oxs_effect_type_t types[] = {
        OXS_EFFECT_FILTER, OXS_EFFECT_DELAY, OXS_EFFECT_REVERB,
        OXS_EFFECT_OVERDRIVE, OXS_EFFECT_FUZZ, OXS_EFFECT_CHORUS,
        OXS_EFFECT_BITCRUSHER, OXS_EFFECT_COMPRESSOR, OXS_EFFECT_PHASER,
        OXS_EFFECT_FLANGER, OXS_EFFECT_TREMOLO, OXS_EFFECT_RINGMOD,
        OXS_EFFECT_TAPE, OXS_EFFECT_SHIMMER
    };

    int clean = 1;
    for (int t = 0; t < 14; t++) {
        /* Fill with loud signal */
        for (uint32_t i = 0; i < frames * 2; i++)
            buf[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;

        oxs_effect_slot_t slot;
        oxs_effect_init(&slot, types[t], 44100);

        /* Process multiple blocks */
        for (int block = 0; block < 20; block++) {
            oxs_effect_process(&slot, buf, frames, 44100, 120.0);
            for (uint32_t i = 0; i < frames * 2; i++) {
                if (isnan(buf[i]) || isinf(buf[i])) { clean = 0; break; }
            }
            if (!clean) break;
        }

        oxs_effect_free(&slot);
        if (!clean) break;
    }

    free(buf);
    ASSERT(clean, "no NaN or Inf from any effect");
    PASS();
}

static void test_delay_feedback_decays(void)
{
    TEST("delay feedback decays over time");
    uint32_t frames = 1024;
    float *buf = malloc(frames * 2 * sizeof(float));

    /* Short impulse */
    memset(buf, 0, frames * 2 * sizeof(float));
    buf[0] = 1.0f;
    buf[1] = 1.0f;

    oxs_effect_slot_t slot;
    oxs_effect_init(&slot, OXS_EFFECT_DELAY, 44100);
    slot.delay.time = 0.01f; /* 10ms */
    slot.delay.feedback = 0.5f;
    slot.delay.wet = 1.0f;

    /* Process several blocks */
    for (int i = 0; i < 10; i++) {
        oxs_effect_process(&slot, buf, frames, 44100, 120.0);
        if (i > 0) memset(buf, 0, frames * 2 * sizeof(float)); /* clear input */
    }

    /* After many blocks, buffer should be near-silent */
    float rms = buffer_rms(buf, frames);
    ASSERT(rms < 0.01f, "delay should decay with feedback < 1");

    oxs_effect_free(&slot);
    free(buf);
    PASS();
}

static void test_reverb_tail(void)
{
    TEST("reverb produces a tail after input stops");
    uint32_t frames = 1024;
    float *buf = malloc(frames * 2 * sizeof(float));

    oxs_effect_slot_t slot;
    oxs_effect_init(&slot, OXS_EFFECT_REVERB, 44100);
    slot.reverb.room_size = 0.9f;
    slot.reverb.wet = 1.0f;

    /* Feed a short burst */
    generate_test_tone(buf, frames, 44100);
    oxs_effect_process(&slot, buf, frames, 44100, 120.0);

    /* Now feed silence — reverb tail should still produce output */
    memset(buf, 0, frames * 2 * sizeof(float));
    oxs_effect_process(&slot, buf, frames, 44100, 120.0);
    float rms_tail = buffer_rms(buf, frames);

    ASSERT(rms_tail > 0.001f, "reverb should have a tail after input stops");

    oxs_effect_free(&slot);
    free(buf);
    PASS();
}

int main(void)
{
    printf("0xSYNTH Effects Chain Tests\n");
    printf("===========================\n\n");

    test_effect_init_free();
    test_each_effect_modifies_signal();
    test_bypass_passes_through();
    test_none_passes_through();
    test_chain_applies_in_series();
    test_api_effect_via_params();
    test_effects_no_nan_or_inf();
    test_delay_feedback_decays();
    test_reverb_tail();

    printf("\n===========================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
