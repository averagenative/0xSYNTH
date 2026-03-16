/*
 * 0xSYNTH Wavetable Synthesis
 *
 * Bank-based wavetable scanning with position morphing.
 * Ported from 0x808 synth.c.
 */

#ifndef OXS_WAVETABLE_H
#define OXS_WAVETABLE_H

#include "oscillator.h"
#include "params.h"
#include <stdint.h>

#define OXS_WT_MAX_FRAMES 64
#define OXS_WT_MAX_BANKS   8

typedef struct {
    char  name[32];
    float frames[OXS_WT_MAX_FRAMES][OXS_WAVETABLE_SIZE];
    int   num_frames;
} oxs_wt_bank_t;

typedef struct {
    oxs_wt_bank_t banks[OXS_WT_MAX_BANKS];
    uint32_t      num_banks;
} oxs_wt_banks_t;

/* Initialize built-in wavetable banks (Analog, Harmonics, PWM, Formant) */
void oxs_wt_banks_init(oxs_wt_banks_t *wt);

/* Read a sample from a wavetable bank with position morphing.
 * phase: 0.0–1.0 within the waveform cycle.
 * position: 0.0–1.0 across the bank frames. */
float oxs_wt_bank_read(const oxs_wt_bank_t *bank, double phase, float position);

/* Load a .wav file as a wavetable bank. The file is split into frames
 * of frame_size samples (default 2048 if 0). Returns bank index or -1. */
int oxs_wt_load_wav(oxs_wt_banks_t *wt, const char *path, int frame_size);

/* Forward declare voice type */
typedef struct oxs_voice_s oxs_voice_t;

/* Render wavetable synthesis for a single voice (additive into output) */
void oxs_wt_render_voice(oxs_voice_t *v,
                         const oxs_param_snapshot_t *snap,
                         const oxs_wt_banks_t *banks,
                         float *output, uint32_t num_frames,
                         uint32_t sample_rate);

#endif /* OXS_WAVETABLE_H */
