/*
 * 0xSYNTH Standalone Audio Backend
 *
 * Uses miniaudio for cross-platform audio output.
 * Audio callback calls oxs_synth_process() to fill buffers.
 */

#ifndef OXS_AUDIO_H
#define OXS_AUDIO_H

#include "../api/synth_api.h"
#include "../engine/recorder.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct oxs_audio oxs_audio_t;

/* Create audio backend. Does not start playback. */
oxs_audio_t *oxs_audio_create(oxs_synth_t *synth, uint32_t sample_rate,
                               uint32_t buffer_size);

/* Start audio playback (begins calling synth_process from audio thread). */
bool oxs_audio_start(oxs_audio_t *audio);

/* Stop audio playback. */
void oxs_audio_stop(oxs_audio_t *audio);

/* Destroy audio backend and free resources. */
void oxs_audio_destroy(oxs_audio_t *audio);

/* List available audio devices to stdout. */
void oxs_audio_list_devices(void);

/* Get number of available audio playback devices. */
int oxs_audio_get_device_count(void);

/* Get name of audio device at index. Returns NULL if invalid. */
const char *oxs_audio_get_device_name(int index);

/* Create audio backend targeting a specific device index.
 * Use device_index = -1 for default device. */
oxs_audio_t *oxs_audio_create_device(oxs_synth_t *synth, uint32_t sample_rate,
                                      uint32_t buffer_size, int device_index);

/* Get the recorder (for UI to start/stop, audio callback writes). */
oxs_recorder_t *oxs_audio_get_recorder(oxs_audio_t *audio);

/* Get the sample rate. */
uint32_t oxs_audio_get_sample_rate(oxs_audio_t *audio);

#endif /* OXS_AUDIO_H */
