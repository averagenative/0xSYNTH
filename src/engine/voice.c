/*
 * 0xSYNTH Voice Manager Implementation
 *
 * Polyphonic voice allocation and subtractive synthesis render path.
 * Ported from 0x808 synth.c (synth_trigger, synth_render, synth_release_all).
 */

#include "voice.h"
#include "fm.h"
#include "wavetable.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Voice Pool ─────────────────────────────────────────────────────────── */

void oxs_voice_pool_init(oxs_voice_pool_t *pool)
{
    memset(pool, 0, sizeof(*pool));
}

int oxs_voice_alloc(oxs_voice_pool_t *pool, int max_voices,
                    oxs_steal_mode_t steal_mode)
{
    if (max_voices > OXS_MAX_VOICES) max_voices = OXS_MAX_VOICES;
    if (max_voices < 1) max_voices = 1;

    /* Look for a free voice */
    for (int i = 0; i < max_voices; i++) {
        if (pool->voices[i].state == OXS_VOICE_IDLE) {
            return i;
        }
    }

    /* No free voice — steal based on mode */
    int steal_idx = 0;
    switch (steal_mode) {
    case OXS_STEAL_OLDEST: {
        uint64_t oldest = UINT64_MAX;
        for (int i = 0; i < max_voices; i++) {
            if (pool->voices[i].start_time < oldest) {
                oldest = pool->voices[i].start_time;
                steal_idx = i;
            }
        }
        break;
    }
    case OXS_STEAL_QUIETEST: {
        float quietest = 2.0f;
        for (int i = 0; i < max_voices; i++) {
            if (pool->voices[i].amp_env.level < quietest) {
                quietest = pool->voices[i].amp_env.level;
                steal_idx = i;
            }
        }
        break;
    }
    case OXS_STEAL_LOWEST: {
        float lowest = 200000.0f;
        for (int i = 0; i < max_voices; i++) {
            if (pool->voices[i].frequency < lowest) {
                lowest = pool->voices[i].frequency;
                steal_idx = i;
            }
        }
        break;
    }
    case OXS_STEAL_HIGHEST: {
        float highest = 0.0f;
        for (int i = 0; i < max_voices; i++) {
            if (pool->voices[i].frequency > highest) {
                highest = pool->voices[i].frequency;
                steal_idx = i;
            }
        }
        break;
    }
    }
    return steal_idx;
}

/* ─── Voice Trigger ──────────────────────────────────────────────────────── */

void oxs_voice_trigger(oxs_voice_pool_t *pool, int vi,
                       uint8_t note, uint8_t velocity, uint8_t channel,
                       const oxs_param_snapshot_t *snap, uint32_t sample_rate)
{
    oxs_voice_t *v = &pool->voices[vi];
    float new_freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);

    /* Mono legato: if voice is already active and poly=1, just glide pitch */
    int max_voices = (int)snap->values[OXS_PARAM_POLY_VOICES];
    if (max_voices == 1 && v->state == OXS_VOICE_ACTIVE) {
        /* Legato — update pitch and note without retriggering */
        v->note = note;
        v->frequency = new_freq;
        v->velocity = (float)velocity / 127.0f;
        return;
    }

    memset(v, 0, sizeof(*v));

    v->state = OXS_VOICE_ACTIVE;
    v->note = note;
    v->channel = channel;
    v->velocity = (float)velocity / 127.0f;
    v->start_time = pool->sample_counter;

    /* MIDI note → frequency */
    v->frequency = new_freq;

    /* Initialize oscillator phases */
    v->osc1_phase = 0.0;
    v->osc2_phase = 0.0;

    /* Spread unison starting phases for width */
    int uv = (int)snap->values[OXS_PARAM_UNISON_VOICES];
    if (uv < 1) uv = 1;
    if (uv > OXS_MAX_UNISON) uv = OXS_MAX_UNISON;
    for (int u = 0; u < uv; u++) {
        v->unison_phases[u] = (double)u / (double)uv;
    }

    /* Smoothed filter cutoff */
    v->smoothed_cutoff = snap->values[OXS_PARAM_FILTER_CUTOFF];

    /* Amp envelope */
    oxs_adsr_params_t amp_adsr = {
        .attack  = snap->values[OXS_PARAM_AMP_ATTACK],
        .decay   = snap->values[OXS_PARAM_AMP_DECAY],
        .sustain = snap->values[OXS_PARAM_AMP_SUSTAIN],
        .release = snap->values[OXS_PARAM_AMP_RELEASE]
    };
    oxs_envelope_trigger(&v->amp_env, &amp_adsr, sample_rate);

    /* Filter envelope */
    oxs_adsr_params_t filt_adsr = {
        .attack  = snap->values[OXS_PARAM_FILT_ATTACK],
        .decay   = snap->values[OXS_PARAM_FILT_DECAY],
        .sustain = snap->values[OXS_PARAM_FILT_SUSTAIN],
        .release = snap->values[OXS_PARAM_FILT_RELEASE]
    };
    oxs_envelope_trigger(&v->filter_env, &filt_adsr, sample_rate);

    /* Filter state */
    v->filter.type = (oxs_filter_type_t)(int)snap->values[OXS_PARAM_FILTER_TYPE];
    memset(v->filter.z1, 0, sizeof(v->filter.z1));
    memset(v->filter.z2, 0, sizeof(v->filter.z2));

    /* LFO */
    v->lfo.waveform = (int)snap->values[OXS_PARAM_LFO_WAVE];
    v->lfo.rate     = snap->values[OXS_PARAM_LFO_RATE];
    v->lfo.depth    = snap->values[OXS_PARAM_LFO_DEPTH];
    v->lfo.dest     = (int)snap->values[OXS_PARAM_LFO_DEST];
    v->lfo.phase    = 0.0;

    /* Mode-specific init */
    int synth_mode = (int)snap->values[OXS_PARAM_SYNTH_MODE];
    if (synth_mode == 1) { /* FM */
        oxs_fm_trigger(v, snap, sample_rate);
    } else if (synth_mode == 2) { /* Wavetable */
        v->wt_phase = 0.0;
        v->wt_smoothed_pos = snap->values[OXS_PARAM_WT_POSITION];
    }
}

/* ─── Voice Release ──────────────────────────────────────────────────────── */

void oxs_voice_release_note(oxs_voice_pool_t *pool, uint8_t note,
                            uint8_t channel,
                            const oxs_param_snapshot_t *snap,
                            uint32_t sample_rate)
{
    oxs_adsr_params_t amp_adsr = {
        .attack  = snap->values[OXS_PARAM_AMP_ATTACK],
        .decay   = snap->values[OXS_PARAM_AMP_DECAY],
        .sustain = snap->values[OXS_PARAM_AMP_SUSTAIN],
        .release = snap->values[OXS_PARAM_AMP_RELEASE]
    };
    oxs_adsr_params_t filt_adsr = {
        .attack  = snap->values[OXS_PARAM_FILT_ATTACK],
        .decay   = snap->values[OXS_PARAM_FILT_DECAY],
        .sustain = snap->values[OXS_PARAM_FILT_SUSTAIN],
        .release = snap->values[OXS_PARAM_FILT_RELEASE]
    };

    for (int i = 0; i < OXS_MAX_VOICES; i++) {
        oxs_voice_t *v = &pool->voices[i];
        if (v->state == OXS_VOICE_ACTIVE &&
            v->note == note && v->channel == channel) {
            v->state = OXS_VOICE_RELEASING;
            oxs_envelope_release(&v->amp_env, &amp_adsr, sample_rate);
            oxs_envelope_release(&v->filter_env, &filt_adsr, sample_rate);
            /* Also release FM operator envelopes */
            int mode = (int)snap->values[OXS_PARAM_SYNTH_MODE];
            if (mode == 1) oxs_fm_release(v, snap, sample_rate);
        }
    }
}

void oxs_voice_release_all(oxs_voice_pool_t *pool,
                           const oxs_param_snapshot_t *snap,
                           uint32_t sample_rate)
{
    oxs_adsr_params_t amp_adsr = {
        .attack  = snap->values[OXS_PARAM_AMP_ATTACK],
        .decay   = snap->values[OXS_PARAM_AMP_DECAY],
        .sustain = snap->values[OXS_PARAM_AMP_SUSTAIN],
        .release = snap->values[OXS_PARAM_AMP_RELEASE]
    };

    int mode = (int)snap->values[OXS_PARAM_SYNTH_MODE];

    for (int i = 0; i < OXS_MAX_VOICES; i++) {
        oxs_voice_t *v = &pool->voices[i];
        if (v->state != OXS_VOICE_IDLE) {
            v->state = OXS_VOICE_RELEASING;
            oxs_envelope_release(&v->amp_env, &amp_adsr, sample_rate);
            if (mode == 1) oxs_fm_release(v, snap, sample_rate);
        }
    }
}

/* ─── Subtractive Render ─────────────────────────────────────────────────── */

void oxs_voice_render_subtractive(oxs_voice_pool_t *pool,
                                  const oxs_param_snapshot_t *snap,
                                  const oxs_wavetables_t *wt,
                                  float *output, uint32_t num_frames,
                                  uint32_t sample_rate)
{
    if (!wt->initialized) return;

    float sr = (float)sample_rate;
    /* Smoothing coefficient: ~3ms */
    float smooth_coeff = 1.0f - expf(-1.0f / (0.003f * sr));

    /* Read param snapshot values once */
    int osc1_wave = (int)snap->values[OXS_PARAM_OSC1_WAVE];
    int osc2_wave = (int)snap->values[OXS_PARAM_OSC2_WAVE];
    float osc_mix = snap->values[OXS_PARAM_OSC_MIX];
    float osc2_detune_cents = snap->values[OXS_PARAM_OSC2_DETUNE];
    int unison_count = (int)snap->values[OXS_PARAM_UNISON_VOICES];
    float unison_detune = snap->values[OXS_PARAM_UNISON_DETUNE];
    float filter_cutoff = snap->values[OXS_PARAM_FILTER_CUTOFF];
    float filter_res = snap->values[OXS_PARAM_FILTER_RESONANCE];
    float filter_env_depth = snap->values[OXS_PARAM_FILTER_ENV_DEPTH];
    /* ADSR params for per-sample processing */
    oxs_adsr_params_t amp_adsr = {
        snap->values[OXS_PARAM_AMP_ATTACK], snap->values[OXS_PARAM_AMP_DECAY],
        snap->values[OXS_PARAM_AMP_SUSTAIN], snap->values[OXS_PARAM_AMP_RELEASE]
    };
    oxs_adsr_params_t filt_adsr = {
        snap->values[OXS_PARAM_FILT_ATTACK], snap->values[OXS_PARAM_FILT_DECAY],
        snap->values[OXS_PARAM_FILT_SUSTAIN], snap->values[OXS_PARAM_FILT_RELEASE]
    };

    /* Clamp waveform indices */
    if (osc1_wave < 0) osc1_wave = 0;
    if (osc1_wave >= OXS_WAVE_COUNT) osc1_wave = OXS_WAVE_COUNT - 1;
    if (osc2_wave < 0) osc2_wave = 0;
    if (osc2_wave >= OXS_WAVE_COUNT) osc2_wave = OXS_WAVE_COUNT - 1;
    if (unison_count < 1) unison_count = 1;
    if (unison_count > OXS_MAX_UNISON) unison_count = OXS_MAX_UNISON;

    const float *table1 = wt->tables[osc1_wave];
    const float *table2 = wt->tables[osc2_wave];

    float osc1_mix = 1.0f - osc_mix;
    float osc2_mix_f = osc_mix;
    double osc2_detune_ratio = pow(2.0, (double)osc2_detune_cents / 1200.0);
    float unison_gain = 1.0f / sqrtf((float)unison_count);

    for (int vi = 0; vi < OXS_MAX_VOICES; vi++) {
        oxs_voice_t *v = &pool->voices[vi];
        if (v->state == OXS_VOICE_IDLE) continue;

        float base_freq = v->frequency;
        double base_phase_inc = (double)base_freq / (double)sr;
        float base_gain = v->velocity;

        /* Per-voice pan */
        float pan = 0.0f; /* center — could be parameterized later */
        float pan_l = (1.0f - pan) * 0.5f;
        float pan_r = (1.0f + pan) * 0.5f;

        /* Pre-compute unison detune ratios and pan positions */
        double u_ratios[OXS_MAX_UNISON];
        float  u_pan_l[OXS_MAX_UNISON];
        float  u_pan_r[OXS_MAX_UNISON];

        for (int u = 0; u < unison_count; u++) {
            if (unison_count == 1) {
                u_ratios[u] = 1.0;
                u_pan_l[u] = pan_l;
                u_pan_r[u] = pan_r;
            } else {
                float detune_c = unison_detune *
                    (-1.0f + 2.0f * (float)u / (float)(unison_count - 1));
                u_ratios[u] = pow(2.0, (double)detune_c / 1200.0);

                float spread = -1.0f + 2.0f * (float)u / (float)(unison_count - 1);
                float u_pan = pan + spread * 0.8f;
                if (u_pan < -1.0f) u_pan = -1.0f;
                if (u_pan >  1.0f) u_pan =  1.0f;
                u_pan_l[u] = (1.0f - u_pan) * 0.5f;
                u_pan_r[u] = (1.0f + u_pan) * 0.5f;
            }
        }

        /* LFO destinations */
        bool lfo_pitch  = (v->lfo.dest == OXS_LFO_DEST_PITCH  && v->lfo.depth > 0.0f);
        bool lfo_filter = (v->lfo.dest == OXS_LFO_DEST_FILTER && v->lfo.depth > 0.0f);
        bool lfo_amp    = (v->lfo.dest == OXS_LFO_DEST_AMP    && v->lfo.depth > 0.0f);

        /* Filter coefficient tracking */
        oxs_filter_coeffs_t fc;
        oxs_filter_calc_coeffs(&fc, v->smoothed_cutoff, filter_res, sample_rate);
        int filter_update_counter = 0;

        for (uint32_t i = 0; i < num_frames; i++) {
            /* Process envelopes */
            float amp = oxs_envelope_process(&v->amp_env, &amp_adsr, sample_rate);
            float filt_env = oxs_envelope_process(&v->filter_env, &filt_adsr,
                                                   sample_rate);

            /* Voice done? */
            if (v->amp_env.stage == OXS_ENV_IDLE) {
                v->state = OXS_VOICE_IDLE;
                break;
            }

            /* LFO */
            float lfo_val = 0.0f;
            if (lfo_pitch || lfo_filter || lfo_amp) {
                lfo_val = oxs_lfo_process(&v->lfo, sample_rate);
            }

            /* Phase increment with pitch LFO */
            double phase_inc = base_phase_inc;
            if (lfo_pitch) {
                float pitch_mult = 1.0f + lfo_val * (2.0f / 12.0f);
                phase_inc = base_phase_inc * pitch_mult;
            }

            /* Render oscillators with unison */
            float left = 0.0f, right = 0.0f;

            for (int u = 0; u < unison_count; u++) {
                double phase = v->unison_phases[u];

                float osc1 = oxs_wavetable_read(table1, phase);
                double osc2_phase = phase * osc2_detune_ratio;
                osc2_phase -= (int)osc2_phase;
                float osc2 = oxs_wavetable_read(table2, osc2_phase);

                float mix = osc1 * osc1_mix + osc2 * osc2_mix_f;
                left  += mix * u_pan_l[u] * unison_gain;
                right += mix * u_pan_r[u] * unison_gain;

                v->unison_phases[u] += phase_inc * u_ratios[u];
                if (v->unison_phases[u] >= 1.0)
                    v->unison_phases[u] -= 1.0;
            }

            /* Update filter coefficients every 32 samples */
            if (++filter_update_counter >= 32) {
                filter_update_counter = 0;
                float target_cutoff = filter_cutoff
                    + filt_env * filter_env_depth * filter_cutoff;
                if (lfo_filter) target_cutoff += lfo_val * 2000.0f;

                v->smoothed_cutoff = oxs_smooth_param(v->smoothed_cutoff,
                                                       target_cutoff,
                                                       smooth_coeff * 32.0f);
                oxs_filter_calc_coeffs(&fc, v->smoothed_cutoff, filter_res,
                                       sample_rate);
            }

            /* Apply filter */
            oxs_filter_apply(&v->filter, &fc, &left, &right);

            /* Apply amplitude */
            float amp_mod = lfo_amp ? (1.0f + lfo_val * 0.5f) : 1.0f;
            float gain = amp * base_gain * amp_mod;
            output[i * 2]     += left * gain;
            output[i * 2 + 1] += right * gain;
        }
    }

    /* Advance sample counter */
    pool->sample_counter += num_frames;
}

/* ─── FM Render ──────────────────────────────────────────────────────────── */

void oxs_voice_render_fm(oxs_voice_pool_t *pool,
                         const oxs_param_snapshot_t *snap,
                         float *output, uint32_t num_frames,
                         uint32_t sample_rate)
{
    for (int vi = 0; vi < OXS_MAX_VOICES; vi++) {
        oxs_voice_t *v = &pool->voices[vi];
        if (v->state == OXS_VOICE_IDLE) continue;
        oxs_fm_render_voice(v, snap, output, num_frames, sample_rate);
    }
    pool->sample_counter += num_frames;
}

/* ─── Wavetable Render ────────────────────────────────────────────────────── */

void oxs_voice_render_wavetable(oxs_voice_pool_t *pool,
                                const oxs_param_snapshot_t *snap,
                                const void *wt_banks_ptr,
                                float *output, uint32_t num_frames,
                                uint32_t sample_rate)
{
    const oxs_wt_banks_t *banks = (const oxs_wt_banks_t *)wt_banks_ptr;
    for (int vi = 0; vi < OXS_MAX_VOICES; vi++) {
        oxs_voice_t *v = &pool->voices[vi];
        if (v->state == OXS_VOICE_IDLE) continue;
        oxs_wt_render_voice(v, snap, banks, output, num_frames, sample_rate);
    }
    pool->sample_counter += num_frames;
}

/* ─── Unified Render Dispatch ────────────────────────────────────────────── */

void oxs_voice_render(oxs_voice_pool_t *pool,
                      const oxs_param_snapshot_t *snap,
                      const oxs_wavetables_t *wt,
                      float *output, uint32_t num_frames,
                      uint32_t sample_rate)
{
    (void)wt; /* wavetables used for subtractive osc lookup */
    int mode = (int)snap->values[OXS_PARAM_SYNTH_MODE];
    switch (mode) {
    case 0: /* Subtractive */
        oxs_voice_render_subtractive(pool, snap, wt, output, num_frames,
                                     sample_rate);
        break;
    case 1: /* FM */
        oxs_voice_render_fm(pool, snap, output, num_frames, sample_rate);
        break;
    case 2: /* Wavetable */
        /* wt_banks is stored in synth handle, passed via API layer */
        break;
    default:
        oxs_voice_render_subtractive(pool, snap, wt, output, num_frames,
                                     sample_rate);
        break;
    }
}

/* ─── Status Queries ─────────────────────────────────────────────────────── */

uint16_t oxs_voice_activity_mask(const oxs_voice_pool_t *pool)
{
    uint16_t mask = 0;
    for (int i = 0; i < OXS_MAX_VOICES; i++) {
        if (pool->voices[i].state != OXS_VOICE_IDLE) {
            mask |= (1u << i);
        }
    }
    return mask;
}

void oxs_voice_env_stages(const oxs_voice_pool_t *pool,
                          uint8_t stages[OXS_MAX_VOICES])
{
    for (int i = 0; i < OXS_MAX_VOICES; i++) {
        stages[i] = (uint8_t)pool->voices[i].amp_env.stage;
    }
}
