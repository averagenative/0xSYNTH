/*
 * 0xSYNTH Sampler Engine
 *
 * Sample playback with Hermite interpolation, pitch shifting,
 * velocity sensitivity, and stereo panning.
 * Ported from 0x808 sampler.c.
 */

#ifndef OXS_SAMPLER_H
#define OXS_SAMPLER_H

#include <stdint.h>
#include <stdbool.h>

#define OXS_MAX_SAMPLES     16
#define OXS_MAX_SAMPLE_VOICES 8
#define OXS_SAMPLE_NAME_LEN 64

/* Loaded audio sample */
typedef struct {
    float   *data;          /* interleaved float samples (L,R,L,R,...) */
    uint32_t num_frames;    /* total frames */
    uint32_t num_channels;  /* 1=mono, 2=stereo */
    uint32_t sample_rate;
    char     name[OXS_SAMPLE_NAME_LEN];
} oxs_sample_t;

/* Sampler playback voice */
typedef struct {
    bool     active;
    int      sample_index;
    double   position;      /* fractional playback position */
    double   rate;          /* 1.0=normal, 2.0=+12 semitones */
    float    velocity;      /* 0-1 */
    float    volume;        /* 0-1 */
    float    pan;           /* -1 to 1 */
    uint64_t start_time;
} oxs_sampler_voice_t;

/* Sampler state */
typedef struct {
    oxs_sample_t        samples[OXS_MAX_SAMPLES];
    uint32_t            num_samples;
    oxs_sampler_voice_t voices[OXS_MAX_SAMPLE_VOICES];
    uint64_t            sample_counter;
} oxs_sampler_t;

/* Initialize sampler */
void oxs_sampler_init(oxs_sampler_t *s);

/* Free all loaded samples */
void oxs_sampler_free(oxs_sampler_t *s);

/* Load a sample from file (WAV/FLAC/MP3 via dr_libs).
 * Returns slot index on success, -1 on failure. */
int oxs_sampler_load(oxs_sampler_t *s, const char *path);

/* Trigger sample playback */
void oxs_sampler_trigger(oxs_sampler_t *s, int sample_index,
                         float velocity, int pitch_offset,
                         float volume, float pan);

/* Render all active sampler voices into output (additive) */
void oxs_sampler_render(oxs_sampler_t *s, float *output, uint32_t num_frames);

#endif /* OXS_SAMPLER_H */
