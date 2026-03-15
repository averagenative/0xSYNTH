/*
 * 0xSYNTH Subtractive Synthesis Tests (Phase 2)
 *
 * Tests: note on → audio output, envelope shape, filter, LFO,
 * polyphony, voice stealing.
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

/* Helper: compute RMS of a stereo buffer */
static float buffer_rms(const float *buf, uint32_t num_frames)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_frames * 2; i++) {
        sum += buf[i] * buf[i];
    }
    return sqrtf(sum / (float)(num_frames * 2));
}

/* Helper: check if buffer has any non-zero samples */
static int buffer_is_silent(const float *buf, uint32_t num_frames)
{
    for (uint32_t i = 0; i < num_frames * 2; i++) {
        if (fabsf(buf[i]) > 1e-6f) return 0;
    }
    return 1;
}

/* Helper: find peak absolute value */
static float buffer_peak(const float *buf, uint32_t num_frames)
{
    float peak = 0.0f;
    for (uint32_t i = 0; i < num_frames * 2; i++) {
        float a = fabsf(buf[i]);
        if (a > peak) peak = a;
    }
    return peak;
}

/* ===== Tests ===== */

static void test_note_on_produces_audio(void)
{
    TEST("note on produces non-silent audio");
    oxs_synth_t *s = oxs_synth_create(44100);

    oxs_synth_note_on(s, 60, 100, 0); /* middle C */

    float buf[512]; /* 256 frames */
    oxs_synth_process(s, buf, 256);

    ASSERT(!buffer_is_silent(buf, 256), "output should not be silent after note on");

    float rms = buffer_rms(buf, 256);
    ASSERT(rms > 0.001f, "RMS too low");

    oxs_synth_destroy(s);
    PASS();
}

static void test_no_notes_is_silent(void)
{
    TEST("no notes produces silence");
    oxs_synth_t *s = oxs_synth_create(44100);

    float buf[512];
    oxs_synth_process(s, buf, 256);

    ASSERT(buffer_is_silent(buf, 256), "should be silent with no notes");

    oxs_synth_destroy(s);
    PASS();
}

static void test_note_off_enters_release(void)
{
    TEST("note off enters release and eventually silences");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Short release time */
    oxs_synth_set_param(s, OXS_PARAM_AMP_RELEASE, 0.01f);

    oxs_synth_note_on(s, 60, 100, 0);
    float buf[512];
    oxs_synth_process(s, buf, 256); /* let it play */

    ASSERT(!buffer_is_silent(buf, 256), "should be playing");

    oxs_synth_note_off(s, 60, 0);

    /* Process enough buffers for the release to complete (~0.01s = 441 samples) */
    for (int i = 0; i < 10; i++) {
        oxs_synth_process(s, buf, 256);
    }

    ASSERT(buffer_is_silent(buf, 256), "should be silent after release");

    oxs_synth_destroy(s);
    PASS();
}

static void test_envelope_attack_ramp(void)
{
    TEST("envelope attack ramps up over time");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Long attack so we can see the ramp */
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.1f); /* 100ms */
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    oxs_synth_note_on(s, 60, 127, 0);

    /* Render first 1ms */
    float buf1[512];
    oxs_synth_process(s, buf1, 44); /* ~1ms */
    float rms1 = buffer_rms(buf1, 44);

    /* Render at ~50ms */
    float buf2[512];
    for (int i = 0; i < 49; i++) {
        oxs_synth_process(s, buf2, 44);
    }
    float rms2 = buffer_rms(buf2, 44);

    ASSERT(rms2 > rms1, "amplitude should increase during attack");

    oxs_synth_destroy(s);
    PASS();
}

static void test_different_notes_different_pitch(void)
{
    TEST("different MIDI notes produce different pitches");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Use sine wave for clean pitch detection */
    oxs_synth_set_param(s, OXS_PARAM_OSC1_WAVE, 3.0f); /* sine */
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    /* Play C4 (note 60) */
    oxs_synth_note_on(s, 60, 100, 0);
    float buf_c4[2048];
    oxs_synth_process(s, buf_c4, 1024);
    oxs_synth_panic(s);
    float silent[512];
    for (int i = 0; i < 5; i++) oxs_synth_process(s, silent, 256);

    /* Play C5 (note 72) — one octave higher */
    oxs_synth_note_on(s, 72, 100, 0);
    float buf_c5[2048];
    oxs_synth_process(s, buf_c5, 1024);

    /* Count zero crossings as a rough pitch proxy.
     * C5 should have ~2x the zero crossings of C4. */
    int zc_c4 = 0, zc_c5 = 0;
    for (uint32_t i = 1; i < 1024; i++) {
        if ((buf_c4[i*2] > 0) != (buf_c4[(i-1)*2] > 0)) zc_c4++;
        if ((buf_c5[i*2] > 0) != (buf_c5[(i-1)*2] > 0)) zc_c5++;
    }

    /* C5 should have roughly 2x zero crossings of C4 (±30% tolerance) */
    float ratio = (float)zc_c5 / (float)(zc_c4 > 0 ? zc_c4 : 1);
    ASSERT(ratio > 1.5f && ratio < 2.5f, "octave should have ~2x zero crossings");

    oxs_synth_destroy(s);
    PASS();
}

static void test_filter_reduces_brightness(void)
{
    TEST("lowpass filter reduces high-frequency content");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Saw wave — lots of harmonics */
    oxs_synth_set_param(s, OXS_PARAM_OSC1_WAVE, 0.0f); /* saw */
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    /* Full cutoff — bright */
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 20000.0f);
    oxs_synth_note_on(s, 60, 100, 0);
    float buf_bright[2048];
    /* Skip attack transient */
    float skip[512];
    oxs_synth_process(s, skip, 256);
    oxs_synth_process(s, buf_bright, 1024);
    oxs_synth_panic(s);
    for (int i = 0; i < 5; i++) oxs_synth_process(s, skip, 256);

    /* Low cutoff — dark */
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 200.0f);
    oxs_synth_note_on(s, 60, 100, 0);
    float buf_dark[2048];
    oxs_synth_process(s, skip, 256);
    oxs_synth_process(s, buf_dark, 1024);

    /* Compare "brightness" via zero crossing rate.
     * More zero crossings = more high-frequency content. */
    int zc_bright = 0, zc_dark = 0;
    for (uint32_t i = 1; i < 1024; i++) {
        if ((buf_bright[i*2] > 0) != (buf_bright[(i-1)*2] > 0)) zc_bright++;
        if ((buf_dark[i*2] > 0) != (buf_dark[(i-1)*2] > 0)) zc_dark++;
    }

    ASSERT(zc_dark < zc_bright, "filtered signal should have fewer zero crossings");

    oxs_synth_destroy(s);
    PASS();
}

static void test_polyphony_multiple_notes(void)
{
    TEST("polyphony: multiple simultaneous notes");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    /* Play a chord */
    oxs_synth_note_on(s, 60, 100, 0); /* C4 */
    oxs_synth_note_on(s, 64, 100, 0); /* E4 */
    oxs_synth_note_on(s, 67, 100, 0); /* G4 */

    float buf[512];
    oxs_synth_process(s, buf, 256);

    float rms = buffer_rms(buf, 256);
    ASSERT(rms > 0.01f, "chord should produce audio");

    /* Check voice activity */
    oxs_output_event_t ev;
    ASSERT(oxs_synth_pop_output_event(s, &ev), "should have output event");
    /* At least 3 bits set in voice_active */
    int active_count = 0;
    for (int i = 0; i < 16; i++) {
        if (ev.voice_active & (1 << i)) active_count++;
    }
    ASSERT(active_count >= 3, "should have at least 3 active voices");

    oxs_synth_destroy(s);
    PASS();
}

static void test_voice_stealing(void)
{
    TEST("voice stealing when pool is full");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    /* Fill all 16 voices */
    for (int i = 0; i < 16; i++) {
        oxs_synth_note_on(s, (uint8_t)(48 + i), 100, 0);
    }
    float buf[512];
    oxs_synth_process(s, buf, 256);

    /* Play one more — should steal oldest */
    oxs_synth_note_on(s, 80, 100, 0);
    oxs_synth_process(s, buf, 256);

    /* Should still have audio (not crash) */
    ASSERT(!buffer_is_silent(buf, 256), "should still produce audio after stealing");

    oxs_synth_destroy(s);
    PASS();
}

static void test_waveform_differences(void)
{
    TEST("different waveforms produce different output");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 20000.0f);

    float rms_vals[4];
    float skip[512];

    for (int wave = 0; wave < 4; wave++) {
        oxs_synth_set_param(s, OXS_PARAM_OSC1_WAVE, (float)wave);
        oxs_synth_note_on(s, 60, 100, 0);
        oxs_synth_process(s, skip, 256); /* skip attack */
        float buf[2048];
        oxs_synth_process(s, buf, 1024);
        rms_vals[wave] = buffer_rms(buf, 1024);
        oxs_synth_panic(s);
        for (int i = 0; i < 5; i++) oxs_synth_process(s, skip, 256);
    }

    /* All should produce audio */
    for (int i = 0; i < 4; i++) {
        ASSERT(rms_vals[i] > 0.001f, "all waveforms should produce audio");
    }

    /* Sine should have lowest RMS (no harmonics), saw/square highest */
    /* Just verify they're not all identical */
    int all_same = 1;
    for (int i = 1; i < 4; i++) {
        if (fabsf(rms_vals[i] - rms_vals[0]) > 0.001f) {
            all_same = 0;
            break;
        }
    }
    ASSERT(!all_same, "different waveforms should have different RMS");

    oxs_synth_destroy(s);
    PASS();
}

static void test_velocity_affects_amplitude(void)
{
    TEST("velocity affects output amplitude");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_SUSTAIN, 1.0f);

    /* Set short release so panic clears voices quickly */
    oxs_synth_set_param(s, OXS_PARAM_AMP_RELEASE, 0.005f);

    /* Loud note */
    oxs_synth_note_on(s, 60, 127, 0);
    float buf_loud[2048];
    float skip[512];
    oxs_synth_process(s, skip, 256);
    oxs_synth_process(s, buf_loud, 1024);
    float rms_loud = buffer_rms(buf_loud, 1024);
    oxs_synth_panic(s);
    /* Wait for release to complete */
    for (int i = 0; i < 20; i++) oxs_synth_process(s, skip, 256);

    /* Quiet note */
    oxs_synth_note_on(s, 60, 30, 0);
    float buf_quiet[2048];
    oxs_synth_process(s, skip, 256);
    oxs_synth_process(s, buf_quiet, 1024);
    float rms_quiet = buffer_rms(buf_quiet, 1024);

    ASSERT(rms_loud > rms_quiet * 1.5f, "loud note should be louder than quiet");

    oxs_synth_destroy(s);
    PASS();
}

static void test_output_event_peaks(void)
{
    TEST("output events report non-zero peaks when playing");
    oxs_synth_t *s = oxs_synth_create(44100);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);

    oxs_synth_note_on(s, 60, 100, 0);
    float buf[512];
    oxs_synth_process(s, buf, 256);

    oxs_output_event_t ev;
    ASSERT(oxs_synth_pop_output_event(s, &ev), "should have event");
    ASSERT(ev.peak_l > 0.0f || ev.peak_r > 0.0f, "peaks should be non-zero");
    ASSERT(ev.voice_active != 0, "should have active voices");

    oxs_synth_destroy(s);
    PASS();
}

static void test_no_nan_or_inf(void)
{
    TEST("no NaN or Inf in output under stress");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Extreme parameter values */
    oxs_synth_set_param(s, OXS_PARAM_FILTER_RESONANCE, 20.0f);
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 20.0f);
    oxs_synth_set_param(s, OXS_PARAM_AMP_ATTACK, 0.001f);
    oxs_synth_set_param(s, OXS_PARAM_UNISON_VOICES, 7.0f);
    oxs_synth_set_param(s, OXS_PARAM_UNISON_DETUNE, 50.0f);

    /* Play many notes rapidly */
    for (int i = 0; i < 16; i++) {
        oxs_synth_note_on(s, (uint8_t)(40 + i * 3), 127, 0);
    }

    float buf[2048];
    int clean = 1;
    for (int block = 0; block < 100; block++) {
        oxs_synth_process(s, buf, 1024);
        for (int i = 0; i < 2048; i++) {
            if (isnan(buf[i]) || isinf(buf[i])) {
                clean = 0;
                break;
            }
        }
        if (!clean) break;
    }

    ASSERT(clean, "output should have no NaN or Inf");

    oxs_synth_destroy(s);
    PASS();
}

/* ===== Main ===== */

int main(void)
{
    printf("0xSYNTH Subtractive Synthesis Tests\n");
    printf("====================================\n\n");

    test_note_on_produces_audio();
    test_no_notes_is_silent();
    test_note_off_enters_release();
    test_envelope_attack_ramp();
    test_different_notes_different_pitch();
    test_filter_reduces_brightness();
    test_polyphony_multiple_notes();
    test_voice_stealing();
    test_waveform_differences();
    test_velocity_affects_amplitude();
    test_output_event_peaks();
    test_no_nan_or_inf();

    printf("\n====================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
