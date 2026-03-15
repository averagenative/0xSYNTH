/*
 * 0xSYNTH Sampler Engine Implementation
 * Ported from 0x808 sampler.c
 */

#include "sampler.h"
#include "dr_wav.h"
#include "dr_flac.h"
#include "dr_mp3.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ─── Hermite Interpolation ──────────────────────────────────────────────── */

static inline float hermite_interp(float y0, float y1, float y2, float y3,
                                   float frac)
{
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

/* ─── Init / Free ────────────────────────────────────────────────────────── */

void oxs_sampler_init(oxs_sampler_t *s)
{
    memset(s, 0, sizeof(*s));
}

void oxs_sampler_free(oxs_sampler_t *s)
{
    for (uint32_t i = 0; i < OXS_MAX_SAMPLES; i++) {
        if (s->samples[i].data) {
            free(s->samples[i].data);
            s->samples[i].data = NULL;
        }
    }
    s->num_samples = 0;
}

/* ─── Load ───────────────────────────────────────────────────────────────── */

int oxs_sampler_load(oxs_sampler_t *s, const char *path)
{
    if (s->num_samples >= OXS_MAX_SAMPLES) return -1;

    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 total_frames = 0;
    float *data = NULL;

    /* Try WAV first */
    size_t path_len = strlen(path);
    if (path_len > 4) {
        const char *ext = path + path_len - 4;
        if (strcasecmp(ext, ".wav") == 0) {
            data = drwav_open_file_and_read_pcm_frames_f32(
                path, &channels, &sample_rate, &total_frames, NULL);
        } else if (strcasecmp(ext, ".mp3") == 0) {
            drmp3_config cfg;
            drmp3_uint64 mp3_frames;
            data = drmp3_open_file_and_read_pcm_frames_f32(
                path, &cfg, &mp3_frames, NULL);
            if (data) {
                channels = cfg.channels;
                sample_rate = cfg.sampleRate;
                total_frames = mp3_frames;
            }
        }
        if (!data && path_len > 5) {
            const char *ext5 = path + path_len - 5;
            if (strcasecmp(ext5, ".flac") == 0) {
                data = drflac_open_file_and_read_pcm_frames_f32(
                    path, &channels, &sample_rate, &total_frames, NULL);
            }
        }
    }

    /* Fallback: try WAV anyway */
    if (!data) {
        data = drwav_open_file_and_read_pcm_frames_f32(
            path, &channels, &sample_rate, &total_frames, NULL);
    }

    if (!data || total_frames == 0) return -1;

    int idx = (int)s->num_samples;
    oxs_sample_t *smp = &s->samples[idx];
    smp->data = data;
    smp->num_frames = (uint32_t)total_frames;
    smp->num_channels = channels;
    smp->sample_rate = sample_rate;

    /* Extract filename for display name */
    const char *name = strrchr(path, '/');
    if (!name) name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    strncpy(smp->name, name, OXS_SAMPLE_NAME_LEN - 1);

    s->num_samples++;
    printf("Sample: loaded %s (%u frames, %uch, %uHz)\n",
           smp->name, smp->num_frames, smp->num_channels, smp->sample_rate);
    return idx;
}

/* ─── Trigger ────────────────────────────────────────────────────────────── */

void oxs_sampler_trigger(oxs_sampler_t *s, int sample_index,
                         float velocity, int pitch_offset,
                         float volume, float pan)
{
    if (sample_index < 0 || (uint32_t)sample_index >= s->num_samples)
        return;
    if (!s->samples[sample_index].data)
        return;

    /* Find free voice or steal oldest */
    int vi = -1;
    uint64_t oldest = UINT64_MAX;
    int oldest_idx = 0;

    for (int i = 0; i < OXS_MAX_SAMPLE_VOICES; i++) {
        if (!s->voices[i].active) { vi = i; break; }
        if (s->voices[i].start_time < oldest) {
            oldest = s->voices[i].start_time;
            oldest_idx = i;
        }
    }
    if (vi < 0) vi = oldest_idx;

    oxs_sampler_voice_t *v = &s->voices[vi];
    v->active = true;
    v->sample_index = sample_index;
    v->position = 0.0;
    v->velocity = velocity;
    v->volume = volume;
    v->pan = pan;
    v->start_time = s->sample_counter;
    v->rate = pow(2.0, (double)pitch_offset / 12.0);
}

/* ─── Render ─────────────────────────────────────────────────────────────── */

void oxs_sampler_render(oxs_sampler_t *s, float *output, uint32_t num_frames)
{
    for (int vi = 0; vi < OXS_MAX_SAMPLE_VOICES; vi++) {
        oxs_sampler_voice_t *v = &s->voices[vi];
        if (!v->active) continue;

        oxs_sample_t *smp = &s->samples[v->sample_index];
        if (!smp->data) { v->active = false; continue; }

        uint32_t total = smp->num_frames;
        uint32_t ch = smp->num_channels;
        float gain = v->velocity * v->volume;
        float left_gain  = gain * (1.0f - v->pan) * 0.5f;
        float right_gain = gain * (1.0f + v->pan) * 0.5f;

        for (uint32_t f = 0; f < num_frames; f++) {
            int pos = (int)v->position;
            if (pos >= (int)total - 1) {
                v->active = false;
                break;
            }

            float frac = (float)(v->position - (double)pos);

            int i0 = (pos > 0) ? pos - 1 : 0;
            int i1 = pos;
            int i2 = (pos + 1 < (int)total) ? pos + 1 : (int)total - 1;
            int i3 = (pos + 2 < (int)total) ? pos + 2 : (int)total - 1;

            float sample_l, sample_r;

            if (ch == 1) {
                float val = hermite_interp(
                    smp->data[i0], smp->data[i1],
                    smp->data[i2], smp->data[i3], frac);
                sample_l = val;
                sample_r = val;
            } else {
                sample_l = hermite_interp(
                    smp->data[i0*2], smp->data[i1*2],
                    smp->data[i2*2], smp->data[i3*2], frac);
                sample_r = hermite_interp(
                    smp->data[i0*2+1], smp->data[i1*2+1],
                    smp->data[i2*2+1], smp->data[i3*2+1], frac);
            }

            output[f * 2]     += sample_l * left_gain;
            output[f * 2 + 1] += sample_r * right_gain;

            v->position += v->rate;
        }
    }

    s->sample_counter += num_frames;
}
