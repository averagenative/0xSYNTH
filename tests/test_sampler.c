/*
 * 0xSYNTH Sampler Engine Tests (Phase 9)
 */

#include "synth_api.h"
#include "../src/engine/params.h"

#include "dr_wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-55s ", name); } while (0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while (0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

/* Generate a test WAV file (440Hz sine, 0.5 seconds, mono, 44100Hz) */
static const char *TEST_WAV = "/tmp/oxs_test_sample.wav";

static void create_test_wav(void)
{
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    format.sampleRate = 44100;
    format.bitsPerSample = 32;

    drwav wav;
    if (!drwav_init_file_write(&wav, TEST_WAV, &format, NULL)) return;

    uint32_t frames = 22050; /* 0.5 seconds */
    float *data = malloc(frames * sizeof(float));
    for (uint32_t i = 0; i < frames; i++) {
        data[i] = sinf((float)(i * 2.0 * M_PI * 440.0 / 44100.0)) * 0.8f;
    }

    drwav_write_pcm_frames(&wav, frames, data);
    drwav_uninit(&wav);
    free(data);
}

/* ===== Tests ===== */

static void test_load_wav(void)
{
    TEST("load WAV sample");
    create_test_wav();

    oxs_synth_t *s = oxs_synth_create(44100);
    int idx = oxs_synth_load_sample(s, TEST_WAV);
    ASSERT(idx >= 0, "should return valid index");

    oxs_synth_destroy(s);
    PASS();
}

static void test_sample_produces_audio(void)
{
    TEST("triggered sample produces audio");
    create_test_wav();

    oxs_synth_t *s = oxs_synth_create(44100);
    int idx = oxs_synth_load_sample(s, TEST_WAV);
    ASSERT(idx >= 0, "load failed");

    oxs_synth_sample_trigger(s, idx, 1.0f, 0);

    float buf[2048];
    oxs_synth_process(s, buf, 1024);
    ASSERT(!buffer_is_silent(buf, 1024), "should produce audio");

    oxs_synth_destroy(s);
    PASS();
}

static void test_sample_finishes(void)
{
    TEST("sample playback finishes and goes silent");
    create_test_wav();

    oxs_synth_t *s = oxs_synth_create(44100);
    int idx = oxs_synth_load_sample(s, TEST_WAV);
    oxs_synth_sample_trigger(s, idx, 1.0f, 0);

    /* Render past the end of the sample (0.5s = 22050 frames) */
    float buf[2048];
    for (int i = 0; i < 30; i++)
        oxs_synth_process(s, buf, 1024);

    /* Should be silent now */
    oxs_synth_process(s, buf, 1024);
    ASSERT(buffer_is_silent(buf, 1024), "should be silent after sample ends");

    oxs_synth_destroy(s);
    PASS();
}

static void test_pitch_shift(void)
{
    TEST("pitch shifting changes playback rate");
    create_test_wav();

    oxs_synth_t *s = oxs_synth_create(44100);
    int idx = oxs_synth_load_sample(s, TEST_WAV);

    /* Normal pitch */
    oxs_synth_sample_trigger(s, idx, 1.0f, 0);
    float buf_normal[2048];
    oxs_synth_process(s, buf_normal, 1024);

    /* Wait for it to finish */
    float skip[2048];
    for (int i = 0; i < 30; i++) oxs_synth_process(s, skip, 1024);

    /* +12 semitones (octave up = 2x speed) */
    oxs_synth_sample_trigger(s, idx, 1.0f, 12);
    float buf_up[2048];
    oxs_synth_process(s, buf_up, 1024);

    /* Count zero crossings — octave up should have ~2x */
    int zc_normal = 0, zc_up = 0;
    for (uint32_t i = 1; i < 1024; i++) {
        if ((buf_normal[i*2] > 0) != (buf_normal[(i-1)*2] > 0)) zc_normal++;
        if ((buf_up[i*2] > 0) != (buf_up[(i-1)*2] > 0)) zc_up++;
    }

    float ratio = (float)zc_up / (float)(zc_normal > 0 ? zc_normal : 1);
    ASSERT(ratio > 1.5f && ratio < 2.5f, "octave up should ~2x zero crossings");

    oxs_synth_destroy(s);
    PASS();
}

static void test_missing_file(void)
{
    TEST("loading nonexistent file returns -1");
    oxs_synth_t *s = oxs_synth_create(44100);
    int idx = oxs_synth_load_sample(s, "/tmp/nonexistent_sample_12345.wav");
    ASSERT(idx == -1, "should return -1");
    oxs_synth_destroy(s);
    PASS();
}

static void test_sampler_no_nan(void)
{
    TEST("sampler produces no NaN or Inf");
    create_test_wav();

    oxs_synth_t *s = oxs_synth_create(44100);
    int idx = oxs_synth_load_sample(s, TEST_WAV);

    /* Trigger many simultaneous samples at various pitches */
    for (int i = 0; i < 8; i++)
        oxs_synth_sample_trigger(s, idx, 1.0f, i * 3 - 12);

    float buf[2048];
    int clean = 1;
    for (int block = 0; block < 30; block++) {
        oxs_synth_process(s, buf, 1024);
        for (int i = 0; i < 2048; i++) {
            if (isnan(buf[i]) || isinf(buf[i])) { clean = 0; break; }
        }
        if (!clean) break;
    }

    ASSERT(clean, "no NaN or Inf");
    unlink(TEST_WAV);
    oxs_synth_destroy(s);
    PASS();
}

int main(void)
{
    printf("0xSYNTH Sampler Engine Tests\n");
    printf("=============================\n\n");

    test_load_wav();
    test_sample_produces_audio();
    test_sample_finishes();
    test_pitch_shift();
    test_missing_file();
    test_sampler_no_nan();

    printf("\n=============================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
