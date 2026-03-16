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

    /* Filter 2 state (type 0 = off) */
    int f2_type = (int)snap->values[OXS_PARAM_FILTER2_TYPE];
    v->filter2.type = (oxs_filter_type_t)(f2_type > 0 ? f2_type - 1 : 0);
    memset(v->filter2.z1, 0, sizeof(v->filter2.z1));
    memset(v->filter2.z2, 0, sizeof(v->filter2.z2));
    v->smoothed_cutoff2 = snap->values[OXS_PARAM_FILTER2_CUTOFF];

    /* LFO 1 */
    v->lfo.waveform = (int)snap->values[OXS_PARAM_LFO_WAVE];
    v->lfo.rate     = snap->values[OXS_PARAM_LFO_RATE];
    v->lfo.depth    = snap->values[OXS_PARAM_LFO_DEPTH];
    v->lfo.dest     = (int)snap->values[OXS_PARAM_LFO_DEST];
    v->lfo.phase    = 0.0;

    /* LFO 2 */
    v->lfo2.waveform = (int)snap->values[OXS_PARAM_LFO2_WAVE];
    v->lfo2.rate     = snap->values[OXS_PARAM_LFO2_RATE];
    v->lfo2.depth    = snap->values[OXS_PARAM_LFO2_DEPTH];
    v->lfo2.dest     = (int)snap->values[OXS_PARAM_LFO2_DEST];
    v->lfo2.phase    = 0.0;

    /* LFO 3 */
    v->lfo3.waveform = (int)snap->values[OXS_PARAM_LFO3_WAVE];
    v->lfo3.rate     = snap->values[OXS_PARAM_LFO3_RATE];
    v->lfo3.depth    = snap->values[OXS_PARAM_LFO3_DEPTH];
    v->lfo3.dest     = (int)snap->values[OXS_PARAM_LFO3_DEST];
    v->lfo3.phase    = 0.0;

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
                                  const oxs_mod_routing_t *mod,
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

    /* Filter 2 params */
    int filter2_type_raw = (int)snap->values[OXS_PARAM_FILTER2_TYPE];
    bool filter2_enabled = (filter2_type_raw > 0);
    float filter2_cutoff = snap->values[OXS_PARAM_FILTER2_CUTOFF];
    float filter2_res = snap->values[OXS_PARAM_FILTER2_RESONANCE];
    float filter2_env_depth = snap->values[OXS_PARAM_FILTER2_ENV_DEPTH];
    int filter_routing = (int)snap->values[OXS_PARAM_FILTER_ROUTING]; /* 0=serial, 1=parallel */

    /* Noise oscillator */
    float noise_level = snap->values[OXS_PARAM_NOISE_LEVEL];
    int noise_type = (int)snap->values[OXS_PARAM_NOISE_TYPE];

    /* Sub-oscillator */
    float sub_level = snap->values[OXS_PARAM_SUB_LEVEL];
    int sub_wave = (int)snap->values[OXS_PARAM_SUB_WAVE];
    int sub_octave_sel = (int)snap->values[OXS_PARAM_SUB_OCTAVE];
    double sub_ratio = (sub_octave_sel == 1) ? 0.25 : 0.5; /* -1 or -2 octaves */

    /* Clamp waveform indices */
    if (osc1_wave < 0) osc1_wave = 0;
    if (osc1_wave >= OXS_WAVE_COUNT) osc1_wave = OXS_WAVE_COUNT - 1;
    if (osc2_wave < 0) osc2_wave = 0;
    if (osc2_wave >= OXS_WAVE_COUNT) osc2_wave = OXS_WAVE_COUNT - 1;
    if (unison_count < 1) unison_count = 1;
    if (unison_count > OXS_MAX_UNISON) unison_count = OXS_MAX_UNISON;

    const float *table1 = wt->tables[osc1_wave];
    const float *table2 = wt->tables[osc2_wave];
    /* Sub-osc uses square or sine wavetable */
    const float *sub_table = wt->tables[sub_wave == 1 ? OXS_WAVE_SINE : OXS_WAVE_SQUARE];

    float osc1_mix = 1.0f - osc_mix;
    float osc2_mix_f = osc_mix;
    double osc2_detune_ratio = pow(2.0, (double)osc2_detune_cents / 1200.0);
    float unison_gain = 1.0f / sqrtf((float)unison_count);

    /* Noise seed — use sample counter for variation across calls */
    uint32_t noise_seed = (uint32_t)pool->sample_counter ^ 0xDEADBEEF;

    for (int vi = 0; vi < OXS_MAX_VOICES; vi++) {
        oxs_voice_t *v = &pool->voices[vi];
        if (v->state == OXS_VOICE_IDLE) continue;

        /* Per-voice pink noise filter state */
        float pink_b0 = 0, pink_b1 = 0, pink_b2 = 0;
        float pink_b3 = 0, pink_b4 = 0, pink_b5 = 0, pink_b6 = 0;
        /* Per-voice noise seed (unique per voice) */
        uint32_t v_noise_seed = noise_seed ^ ((uint32_t)vi * 2654435761u);

        /* Apply pitch bend to base frequency */
        float bend = snap->values[OXS_PARAM_PITCH_BEND];
        float bend_range = snap->values[OXS_PARAM_PITCH_BEND_RANGE];
        float bend_semitones = bend * bend_range;

        /* MPE per-voice pitch bend */
        bool mpe_on = snap->values[OXS_PARAM_MPE_ENABLED] > 0.5f;
        if (mpe_on && v->mpe_pitch_bend != 0.0f) {
            float mpe_range = snap->values[OXS_PARAM_MPE_PITCH_RANGE];
            bend_semitones += v->mpe_pitch_bend * mpe_range;
        }

        float base_freq = v->frequency * powf(2.0f, bend_semitones / 12.0f);
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

        /* LFO destinations (all 3 LFOs) */
        bool lfo_pitch  = (v->lfo.dest == OXS_LFO_DEST_PITCH  && v->lfo.depth > 0.0f);
        bool lfo_filter = (v->lfo.dest == OXS_LFO_DEST_FILTER && v->lfo.depth > 0.0f);
        bool lfo_amp    = (v->lfo.dest == OXS_LFO_DEST_AMP    && v->lfo.depth > 0.0f);
        bool lfo2_pitch  = (v->lfo2.dest == OXS_LFO_DEST_PITCH  && v->lfo2.depth > 0.0f);
        bool lfo2_filter = (v->lfo2.dest == OXS_LFO_DEST_FILTER && v->lfo2.depth > 0.0f);
        bool lfo2_amp    = (v->lfo2.dest == OXS_LFO_DEST_AMP    && v->lfo2.depth > 0.0f);
        bool lfo3_pitch  = (v->lfo3.dest == OXS_LFO_DEST_PITCH  && v->lfo3.depth > 0.0f);
        bool lfo3_filter = (v->lfo3.dest == OXS_LFO_DEST_FILTER && v->lfo3.depth > 0.0f);
        bool lfo3_amp    = (v->lfo3.dest == OXS_LFO_DEST_AMP    && v->lfo3.depth > 0.0f);
        bool any_lfo = lfo_pitch || lfo_filter || lfo_amp ||
                        lfo2_pitch || lfo2_filter || lfo2_amp ||
                        lfo3_pitch || lfo3_filter || lfo3_amp;

        /* Filter coefficient tracking */
        oxs_filter_coeffs_t fc;
        oxs_filter_calc_coeffs(&fc, v->smoothed_cutoff, filter_res, sample_rate);
        oxs_filter_coeffs_t fc2;
        if (filter2_enabled) {
            oxs_filter_calc_coeffs(&fc2, v->smoothed_cutoff2, filter2_res, sample_rate);
        }
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

            /* LFOs */
            float lfo_val = 0.0f, lfo2_val = 0.0f, lfo3_val = 0.0f;
            if (any_lfo || (mod && mod->active_count > 0)) {
                lfo_val = oxs_lfo_process(&v->lfo, sample_rate);
                if (lfo2_pitch || lfo2_filter || lfo2_amp || (mod && mod->active_count > 0))
                    lfo2_val = oxs_lfo_process(&v->lfo2, sample_rate);
                if (lfo3_pitch || lfo3_filter || lfo3_amp || (mod && mod->active_count > 0))
                    lfo3_val = oxs_lfo_process(&v->lfo3, sample_rate);
            }

            /* Mod matrix source values */
            oxs_mod_sources_t msrc = {0};
            if (mod && mod->active_count > 0) {
                msrc.lfo1 = lfo_val;
                msrc.lfo2 = lfo2_val;
                msrc.lfo3 = lfo3_val;
                msrc.amp_env = amp;
                msrc.filt_env = filt_env;
                msrc.velocity = v->velocity;
                msrc.mod_wheel = snap->values[OXS_PARAM_MOD_WHEEL];
                msrc.aftertouch = snap->values[OXS_PARAM_AFTERTOUCH];
                msrc.key_track = ((float)v->note - 60.0f) / 60.0f;
                msrc.macro1 = snap->values[OXS_PARAM_MACRO1];
                msrc.macro2 = snap->values[OXS_PARAM_MACRO2];
                msrc.macro3 = snap->values[OXS_PARAM_MACRO3];
                msrc.macro4 = snap->values[OXS_PARAM_MACRO4];
                msrc.mpe_pressure = v->mpe_pressure;
                msrc.mpe_slide = v->mpe_slide;
            }

            /* Phase increment with pitch LFOs + mod matrix */
            double phase_inc = base_phase_inc;
            float pitch_mod = 0.0f;
            if (lfo_pitch)  pitch_mod += lfo_val * (2.0f / 12.0f);
            if (lfo2_pitch) pitch_mod += lfo2_val * (2.0f / 12.0f);
            if (lfo3_pitch) pitch_mod += lfo3_val * (2.0f / 12.0f);
            if (mod && mod->active_count > 0) {
                /* Mod matrix pitch offset (in semitones mapped to ratio) */
                float pitch_offset = oxs_mod_offset(mod, &msrc, OXS_PARAM_PITCH_BEND);
                pitch_mod += pitch_offset / 12.0f;
            }
            if (pitch_mod != 0.0f) {
                phase_inc = base_phase_inc * (1.0 + (double)pitch_mod);
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

            /* Sub-oscillator */
            if (sub_level > 0.001f) {
                double sub_phase = v->osc1_phase * sub_ratio;
                sub_phase -= (int64_t)sub_phase;
                if (sub_phase < 0) sub_phase += 1.0;
                float sub_sample = oxs_wavetable_read(sub_table, sub_phase);
                left  += sub_sample * sub_level;
                right += sub_sample * sub_level;
            }

            /* Noise oscillator */
            if (noise_level > 0.001f) {
                /* Fast xorshift noise */
                v_noise_seed ^= v_noise_seed << 13;
                v_noise_seed ^= v_noise_seed >> 17;
                v_noise_seed ^= v_noise_seed << 5;
                float white = (float)(int32_t)v_noise_seed / 2147483648.0f;

                float noise_sample;
                if (noise_type == 1) {
                    /* Pink noise (Paul Kellet's approximation) */
                    pink_b0 = 0.99886f * pink_b0 + white * 0.0555179f;
                    pink_b1 = 0.99332f * pink_b1 + white * 0.0750759f;
                    pink_b2 = 0.96900f * pink_b2 + white * 0.1538520f;
                    pink_b3 = 0.86650f * pink_b3 + white * 0.3104856f;
                    pink_b4 = 0.55000f * pink_b4 + white * 0.5329522f;
                    pink_b5 = -0.7616f * pink_b5 - white * 0.0168980f;
                    noise_sample = (pink_b0 + pink_b1 + pink_b2 + pink_b3 +
                                    pink_b4 + pink_b5 + pink_b6 + white * 0.5362f) * 0.11f;
                    pink_b6 = white * 0.115926f;
                } else {
                    noise_sample = white;
                }
                left  += noise_sample * noise_level;
                right += noise_sample * noise_level;
            }

            /* Update filter coefficients every 32 samples */
            if (++filter_update_counter >= 32) {
                filter_update_counter = 0;
                float target_cutoff = filter_cutoff
                    + filt_env * filter_env_depth * filter_cutoff;
                if (lfo_filter)  target_cutoff += lfo_val * 2000.0f;
                if (lfo2_filter) target_cutoff += lfo2_val * 2000.0f;
                if (lfo3_filter) target_cutoff += lfo3_val * 2000.0f;
                /* Mod matrix → filter cutoff */
                if (mod && mod->active_count > 0) {
                    target_cutoff += oxs_mod_offset(mod, &msrc, OXS_PARAM_FILTER_CUTOFF);
                }
                if (target_cutoff < 20.0f) target_cutoff = 20.0f;
                if (target_cutoff > 20000.0f) target_cutoff = 20000.0f;

                /* Mod matrix → filter resonance */
                float mod_res = filter_res;
                if (mod && mod->active_count > 0) {
                    mod_res += oxs_mod_offset(mod, &msrc, OXS_PARAM_FILTER_RESONANCE);
                    if (mod_res < 0.0f) mod_res = 0.0f;
                    if (mod_res > 20.0f) mod_res = 20.0f;
                }

                v->smoothed_cutoff = oxs_smooth_param(v->smoothed_cutoff,
                                                       target_cutoff,
                                                       smooth_coeff * 32.0f);
                oxs_filter_calc_coeffs(&fc, v->smoothed_cutoff, mod_res,
                                       sample_rate);

                /* Filter 2 coefficient update */
                if (filter2_enabled) {
                    float target_co2 = filter2_cutoff
                        + filt_env * filter2_env_depth * filter2_cutoff;
                    if (target_co2 < 20.0f) target_co2 = 20.0f;
                    if (target_co2 > 20000.0f) target_co2 = 20000.0f;
                    v->smoothed_cutoff2 = oxs_smooth_param(v->smoothed_cutoff2,
                                                            target_co2,
                                                            smooth_coeff * 32.0f);
                    oxs_filter_calc_coeffs(&fc2, v->smoothed_cutoff2, filter2_res,
                                           sample_rate);
                }
            }

            /* Apply filters */
            if (filter2_enabled && filter_routing == 1) {
                /* Parallel: apply both filters to original signal, mix 50/50 */
                float l1 = left, r1 = right;
                float l2 = left, r2 = right;
                oxs_filter_apply(&v->filter, &fc, &l1, &r1);
                oxs_filter_apply(&v->filter2, &fc2, &l2, &r2);
                left  = (l1 + l2) * 0.5f;
                right = (r1 + r2) * 0.5f;
            } else {
                /* Serial (or filter2 off): filter1 → filter2 */
                oxs_filter_apply(&v->filter, &fc, &left, &right);
                if (filter2_enabled)
                    oxs_filter_apply(&v->filter2, &fc2, &left, &right);
            }

            /* Apply amplitude + LFOs + mod matrix → master volume */
            float amp_mod = 1.0f;
            if (lfo_amp)  amp_mod += lfo_val * 0.5f;
            if (lfo2_amp) amp_mod += lfo2_val * 0.5f;
            if (lfo3_amp) amp_mod += lfo3_val * 0.5f;
            if (mod && mod->active_count > 0) {
                amp_mod += oxs_mod_offset(mod, &msrc, OXS_PARAM_MASTER_VOLUME);
            }
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
                         const oxs_mod_routing_t *mod,
                         float *output, uint32_t num_frames,
                         uint32_t sample_rate)
{
    (void)mod; /* TODO: integrate mod matrix into FM render */
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
                                const oxs_mod_routing_t *mod,
                                float *output, uint32_t num_frames,
                                uint32_t sample_rate)
{
    (void)mod; /* TODO: integrate mod matrix into WT render */
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
                      const oxs_mod_routing_t *mod,
                      float *output, uint32_t num_frames,
                      uint32_t sample_rate)
{
    (void)wt; /* wavetables used for subtractive osc lookup */
    int mode = (int)snap->values[OXS_PARAM_SYNTH_MODE];
    switch (mode) {
    case 0: /* Subtractive */
        oxs_voice_render_subtractive(pool, snap, wt, mod, output, num_frames,
                                     sample_rate);
        break;
    case 1: /* FM */
        oxs_voice_render_fm(pool, snap, mod, output, num_frames, sample_rate);
        break;
    case 2: /* Wavetable */
        /* wt_banks is stored in synth handle, passed via API layer */
        break;
    default:
        oxs_voice_render_subtractive(pool, snap, wt, mod, output, num_frames,
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
