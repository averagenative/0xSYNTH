/*
 * 0xSYNTH Standalone Audio Backend Implementation
 *
 * miniaudio playback device driving oxs_synth_process().
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

struct oxs_audio {
    ma_device        device;
    ma_device_config config;
    oxs_synth_t     *synth;
    uint32_t         sample_rate;
    uint32_t         buffer_size;
    bool             running;
    oxs_recorder_t   recorder;
};

/* Audio callback — called from miniaudio's audio thread */
static void audio_callback(ma_device *device, void *output, const void *input,
                           ma_uint32 frame_count)
{
    (void)input;
    oxs_audio_t *audio = (oxs_audio_t *)device->pUserData;
    if (!audio || !audio->synth) {
        memset(output, 0, frame_count * 2 * sizeof(float));
        return;
    }

    /* oxs_synth_process outputs interleaved stereo floats — matches miniaudio */
    oxs_synth_process(audio->synth, (float *)output, frame_count);

    /* Stream to WAV if recording */
    if (atomic_load_explicit(&audio->recorder.state, memory_order_acquire) == OXS_REC_ACTIVE)
        oxs_recorder_write(&audio->recorder, (const float *)output, frame_count);
}

oxs_audio_t *oxs_audio_create(oxs_synth_t *synth, uint32_t sample_rate,
                               uint32_t buffer_size)
{
    oxs_audio_t *audio = calloc(1, sizeof(oxs_audio_t));
    if (!audio) return NULL;

    audio->synth = synth;
    audio->sample_rate = sample_rate;
    audio->buffer_size = buffer_size;

    audio->config = ma_device_config_init(ma_device_type_playback);
    audio->config.playback.format   = ma_format_f32;
    audio->config.playback.channels = 2;
    audio->config.sampleRate        = sample_rate;
    audio->config.periodSizeInFrames = buffer_size;
    audio->config.dataCallback      = audio_callback;
    audio->config.pUserData         = audio;

    ma_result result = ma_device_init(NULL, &audio->config, &audio->device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio device: %s\n",
                ma_result_description(result));
        free(audio);
        return NULL;
    }

    printf("Audio: %s @ %uHz, buffer %u frames\n",
           audio->device.playback.name,
           audio->device.sampleRate,
           buffer_size);

    return audio;
}

bool oxs_audio_start(oxs_audio_t *audio)
{
    if (!audio || audio->running) return false;

    ma_result result = ma_device_start(&audio->device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to start audio: %s\n",
                ma_result_description(result));
        return false;
    }

    audio->running = true;
    return true;
}

void oxs_audio_stop(oxs_audio_t *audio)
{
    if (!audio || !audio->running) return;
    ma_device_stop(&audio->device);
    audio->running = false;
}

void oxs_audio_destroy(oxs_audio_t *audio)
{
    if (!audio) return;
    if (audio->recorder.state == OXS_REC_ACTIVE)
        oxs_recorder_stop(&audio->recorder);
    if (audio->running) oxs_audio_stop(audio);
    ma_device_uninit(&audio->device);
    free(audio);
}

oxs_recorder_t *oxs_audio_get_recorder(oxs_audio_t *audio)
{
    return audio ? &audio->recorder : NULL;
}

uint32_t oxs_audio_get_sample_rate(oxs_audio_t *audio)
{
    return audio ? audio->sample_rate : 0;
}

void oxs_audio_list_devices(void)
{
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio context\n");
        return;
    }

    ma_device_info *playback_infos;
    ma_uint32 playback_count;
    ma_context_get_devices(&context, &playback_infos, &playback_count,
                           NULL, NULL);

    printf("Audio devices:\n");
    for (ma_uint32 i = 0; i < playback_count; i++) {
        printf("  [%u] %s%s\n", i, playback_infos[i].name,
               playback_infos[i].isDefault ? " (default)" : "");
    }

    ma_context_uninit(&context);
}
