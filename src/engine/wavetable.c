/*
 * 0xSYNTH Wavetable Synthesis Implementation
 * Ported from 0x808 synth.c
 */

#include "wavetable.h"
#include "voice.h"
#include "envelope.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Bank Reading ───────────────────────────────────────────────────────── */

float oxs_wt_bank_read(const oxs_wt_bank_t *bank, double phase, float position)
{
    if (bank->num_frames < 1) return 0.0f;

    /* Position maps to frame index */
    float pos = position * (float)(bank->num_frames - 1);
    int f0 = (int)pos;
    if (f0 >= bank->num_frames - 1) f0 = bank->num_frames - 2;
    if (f0 < 0) f0 = 0;
    int f1 = f0 + 1;
    float frac = pos - (float)f0;

    /* Read from both frames with linear interpolation */
    float s0 = oxs_wavetable_read(bank->frames[f0], phase);
    float s1 = oxs_wavetable_read(bank->frames[f1], phase);
    return s0 * (1.0f - frac) + s1 * frac;
}

/* ─── Built-in Banks ─────────────────────────────────────────────────────── */

void oxs_wt_banks_init(oxs_wt_banks_t *wt)
{
    memset(wt, 0, sizeof(*wt));

    /* Bank 0: "Analog" — morph saw → square → triangle → sine */
    {
        oxs_wt_bank_t *b = &wt->banks[0];
        snprintf(b->name, sizeof(b->name), "Analog");
        b->num_frames = 16;
        for (int f = 0; f < 16; f++) {
            float t = (float)f / 15.0f;
            for (int i = 0; i < OXS_WAVETABLE_SIZE; i++) {
                double phase = (double)i / (double)OXS_WAVETABLE_SIZE;
                float saw = (float)(2.0 * phase - 1.0);
                float square = (phase < 0.5) ? 1.0f : -1.0f;
                float tri;
                if (phase < 0.25) tri = (float)(4.0 * phase);
                else if (phase < 0.75) tri = (float)(2.0 - 4.0 * phase);
                else tri = (float)(4.0 * phase - 4.0);
                float sine = sinf((float)(phase * 2.0 * M_PI));

                float val;
                if (t < 0.333f) {
                    float m = t / 0.333f;
                    val = saw * (1.0f - m) + square * m;
                } else if (t < 0.667f) {
                    float m = (t - 0.333f) / 0.334f;
                    val = square * (1.0f - m) + tri * m;
                } else {
                    float m = (t - 0.667f) / 0.333f;
                    val = tri * (1.0f - m) + sine * m;
                }
                b->frames[f][i] = val;
            }
        }
    }

    /* Bank 1: "Harmonics" — additive with increasing harmonic count */
    {
        oxs_wt_bank_t *b = &wt->banks[1];
        snprintf(b->name, sizeof(b->name), "Harmonics");
        b->num_frames = 16;
        for (int f = 0; f < 16; f++) {
            int num_harmonics = f + 1;
            for (int i = 0; i < OXS_WAVETABLE_SIZE; i++) {
                double phase = (double)i / (double)OXS_WAVETABLE_SIZE;
                float val = 0.0f;
                for (int h = 1; h <= num_harmonics; h++) {
                    val += sinf((float)(phase * 2.0 * M_PI * h)) / (float)h;
                }
                b->frames[f][i] = val * 0.5f;
            }
        }
    }

    /* Bank 2: "PWM" — pulse width from 50% down to 5% */
    {
        oxs_wt_bank_t *b = &wt->banks[2];
        snprintf(b->name, sizeof(b->name), "PWM");
        b->num_frames = 16;
        for (int f = 0; f < 16; f++) {
            float pw = 0.5f - (float)f * 0.03f;
            if (pw < 0.05f) pw = 0.05f;
            for (int i = 0; i < OXS_WAVETABLE_SIZE; i++) {
                double phase = (double)i / (double)OXS_WAVETABLE_SIZE;
                b->frames[f][i] = (phase < (double)pw) ? 1.0f : -1.0f;
            }
        }
    }

    /* Bank 3: "Formant" — vowel-like resonances */
    {
        oxs_wt_bank_t *b = &wt->banks[3];
        snprintf(b->name, sizeof(b->name), "Formant");
        b->num_frames = 16;
        static const float formants[][2] = {
            {800, 1200}, {400, 2200}, {250, 2600}, {450, 800}, {300, 700},
        };
        for (int f = 0; f < 16; f++) {
            float idx = (float)f / 15.0f * 4.0f;
            int v0 = (int)idx;
            if (v0 > 3) v0 = 3;
            int v1 = v0 + 1;
            float frac = idx - (float)v0;
            float f1 = formants[v0][0] * (1.0f - frac) + formants[v1][0] * frac;
            float f2 = formants[v0][1] * (1.0f - frac) + formants[v1][1] * frac;

            for (int i = 0; i < OXS_WAVETABLE_SIZE; i++) {
                double phase = (double)i / (double)OXS_WAVETABLE_SIZE;
                float val = sinf((float)(phase * 2.0 * M_PI));
                float r1 = f1 / 256.0f;
                float r2 = f2 / 256.0f;
                val += 0.5f * sinf((float)(phase * 2.0 * M_PI * r1));
                val += 0.3f * sinf((float)(phase * 2.0 * M_PI * r2));
                b->frames[f][i] = val * 0.4f;
            }
        }
    }

    wt->num_banks = 4;
}

/* ─── Wavetable Voice Render ─────────────────────────────────────────────── */

void oxs_wt_render_voice(oxs_voice_t *v,
                         const oxs_param_snapshot_t *snap,
                         const oxs_wt_banks_t *banks,
                         float *output, uint32_t num_frames,
                         uint32_t sample_rate)
{
    int bank_idx = (int)snap->values[OXS_PARAM_WT_BANK];
    if (bank_idx < 0 || (uint32_t)bank_idx >= banks->num_banks) bank_idx = 0;
    if (banks->num_banks == 0) {
        v->state = OXS_VOICE_IDLE;
        return;
    }
    const oxs_wt_bank_t *bank = &banks->banks[bank_idx];

    float sr = (float)sample_rate;
    /* Apply pitch bend */
    float bend = snap->values[OXS_PARAM_PITCH_BEND];
    float bend_range = snap->values[OXS_PARAM_PITCH_BEND_RANGE];
    float bent_freq = v->frequency * powf(2.0f, (bend * bend_range) / 12.0f);
    double phase_inc = (double)bent_freq / (double)sr;
    float base_gain = v->velocity;
    float smooth_coeff = 1.0f - expf(-1.0f / (0.005f * sr));

    float wt_position = snap->values[OXS_PARAM_WT_POSITION];
    float wt_env_depth = snap->values[OXS_PARAM_WT_ENV_DEPTH];
    float wt_lfo_depth = snap->values[OXS_PARAM_WT_LFO_DEPTH];

    oxs_adsr_params_t amp_adsr = {
        snap->values[OXS_PARAM_AMP_ATTACK], snap->values[OXS_PARAM_AMP_DECAY],
        snap->values[OXS_PARAM_AMP_SUSTAIN], snap->values[OXS_PARAM_AMP_RELEASE]
    };
    oxs_adsr_params_t filt_adsr = {
        snap->values[OXS_PARAM_FILT_ATTACK], snap->values[OXS_PARAM_FILT_DECAY],
        snap->values[OXS_PARAM_FILT_SUSTAIN], snap->values[OXS_PARAM_FILT_RELEASE]
    };

    /* Pan (centered) */
    float pan_l = 0.5f, pan_r = 0.5f;

    for (uint32_t i = 0; i < num_frames; i++) {
        float amp = oxs_envelope_process(&v->amp_env, &amp_adsr, sample_rate);
        float filt_env = oxs_envelope_process(&v->filter_env, &filt_adsr,
                                               sample_rate);

        if (v->amp_env.stage == OXS_ENV_IDLE) {
            v->state = OXS_VOICE_IDLE;
            break;
        }

        /* LFO */
        float lfo_val = 0.0f;
        if (v->lfo.dest != OXS_LFO_DEST_NONE && v->lfo.depth > 0.0f) {
            lfo_val = oxs_lfo_process(&v->lfo, sample_rate);
        }

        /* Compute modulated wavetable position */
        float target_pos = wt_position
                         + filt_env * wt_env_depth
                         + lfo_val * wt_lfo_depth;
        if (target_pos < 0.0f) target_pos = 0.0f;
        if (target_pos > 1.0f) target_pos = 1.0f;

        /* Smooth position to prevent clicks */
        v->wt_smoothed_pos += smooth_coeff * (target_pos - v->wt_smoothed_pos);

        /* Read from wavetable bank */
        float sample = oxs_wt_bank_read(bank, v->wt_phase, v->wt_smoothed_pos);

        /* Advance phase */
        v->wt_phase += phase_inc;
        if (v->wt_phase >= 1.0) v->wt_phase -= 1.0;

        /* Apply gain */
        float gain = amp * base_gain;
        output[i * 2]     += sample * pan_l * gain;
        output[i * 2 + 1] += sample * pan_r * gain;
    }
}

/* ─── WAV Import as Wavetable Bank ──────────────────────────────────────── */

#include "dr_wav.h"

int oxs_wt_load_wav(oxs_wt_banks_t *wt, const char *path, int frame_size)
{
    if (!wt || !path) return -1;
    if (wt->num_banks >= OXS_WT_MAX_BANKS) return -1;
    if (frame_size <= 0) frame_size = OXS_WAVETABLE_SIZE;

    /* Load WAV as mono float */
    drwav wav;
    if (!drwav_init_file(&wav, path, NULL)) return -1;

    uint64_t total = wav.totalPCMFrameCount;
    if (total == 0) { drwav_uninit(&wav); return -1; }

    float *data = (float *)malloc(total * sizeof(float));
    if (!data) { drwav_uninit(&wav); return -1; }

    /* Read as mono (average channels if stereo) */
    if (wav.channels == 1) {
        drwav_read_pcm_frames_f32(&wav, (drwav_uint64)total, data);
    } else {
        float *interleaved = (float *)malloc(total * wav.channels * sizeof(float));
        if (!interleaved) { free(data); drwav_uninit(&wav); return -1; }
        drwav_read_pcm_frames_f32(&wav, (drwav_uint64)total, interleaved);
        for (uint64_t i = 0; i < total; i++) {
            float sum = 0;
            for (uint32_t ch = 0; ch < wav.channels; ch++)
                sum += interleaved[i * wav.channels + ch];
            data[i] = sum / (float)wav.channels;
        }
        free(interleaved);
    }
    drwav_uninit(&wav);

    /* Split into wavetable frames */
    int num_frames = (int)(total / frame_size);
    if (num_frames < 1) { free(data); return -1; }
    if (num_frames > OXS_WT_MAX_FRAMES) num_frames = OXS_WT_MAX_FRAMES;

    uint32_t idx = wt->num_banks;
    oxs_wt_bank_t *bank = &wt->banks[idx];
    memset(bank, 0, sizeof(*bank));

    /* Extract filename for bank name */
    const char *name = path;
    const char *sep = strrchr(path, '/');
    if (!sep) sep = strrchr(path, '\\');
    if (sep) name = sep + 1;
    snprintf(bank->name, sizeof(bank->name), "%s", name);
    /* Strip extension */
    char *dot = strrchr(bank->name, '.');
    if (dot) *dot = '\0';

    bank->num_frames = num_frames;

    /* Resample each frame to OXS_WAVETABLE_SIZE */
    for (int f = 0; f < num_frames; f++) {
        const float *src = data + f * frame_size;
        for (int s = 0; s < OXS_WAVETABLE_SIZE; s++) {
            double pos = (double)s / OXS_WAVETABLE_SIZE * frame_size;
            int i0 = (int)pos;
            float frac = (float)(pos - i0);
            int i1 = i0 + 1;
            if (i1 >= frame_size) i1 = 0; /* wrap */
            bank->frames[f][s] = src[i0] * (1.0f - frac) + src[i1] * frac;
        }
    }

    wt->num_banks++;
    free(data);
    return (int)idx;
}
