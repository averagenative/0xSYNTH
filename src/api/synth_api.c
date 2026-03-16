/*
 * 0xSYNTH Public API Implementation
 *
 * This file defines the opaque oxs_synth struct and implements all
 * public API functions. It is the only file that bridges the public
 * API header with internal engine headers.
 */

#include "synth_api.h"
#include "../engine/params.h"
#include "../engine/command_queue.h"
#include "../engine/output_events.h"
#include "../engine/voice.h"
#include "../engine/oscillator.h"
#include "../engine/wavetable.h"
#include "../engine/effects.h"
#include "../engine/preset.h"
#include "../engine/sampler.h"
#include "../engine/arpeggiator.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Maximum oversampled buffer: 4096 frames * 4x * 2 channels = 32768 floats */
#define OXS_OS_MAX_FRAMES 4096
#define OXS_OS_MAX_FACTOR 4

/* The opaque synth handle — all internals hidden from consumers */
struct oxs_synth {
    uint32_t              sample_rate;

    /* Parameter system */
    oxs_param_registry_t  registry;
    oxs_param_store_t     params;
    oxs_param_snapshot_t  snapshot;
    oxs_midi_cc_map_t     cc_map;

    /* Communication queues */
    oxs_cmd_queue_t       cmd_queue;
    oxs_output_queue_t    output_queue;

    /* Voice system */
    oxs_voice_pool_t      voice_pool;
    oxs_wavetables_t      wavetables;
    oxs_wt_banks_t        wt_banks;

    /* Effect chain (3 slots, master bus) */
    oxs_effect_slot_t     effects[OXS_MAX_EFFECTS];

    /* Sampler */
    oxs_sampler_t         sampler;

    /* MIDI learn state */
    int32_t               midi_learn_param; /* -1 = not learning */

    /* Arpeggiator */
    oxs_arpeggiator_t     arp;

    /* Oscilloscope buffer (audio thread writes, GUI reads) */
#define OXS_SCOPE_SIZE 1024
    float                 scope_buf[OXS_SCOPE_SIZE]; /* mono mixdown */
    _Atomic uint32_t      scope_write_pos;

    /* Oversampling buffer (heap-allocated, avoids stack overflow in plugins) */
    float                *os_buf;
    uint32_t              os_buf_size; /* in floats */
};

/* === Lifecycle === */

oxs_synth_t *oxs_synth_create(uint32_t sample_rate)
{
    oxs_synth_t *s = calloc(1, sizeof(oxs_synth_t));
    if (!s) return NULL;

    s->sample_rate = sample_rate;

    /* Initialize parameter system */
    oxs_param_registry_init(&s->registry);
    oxs_param_store_init(&s->params, &s->registry);
    oxs_midi_cc_map_init(&s->cc_map);

    /* Initialize queues */
    oxs_cmd_queue_init(&s->cmd_queue);
    oxs_output_queue_init(&s->output_queue);

    /* Initialize voice system */
    oxs_voice_pool_init(&s->voice_pool);
    oxs_wavetables_init(&s->wavetables);
    oxs_wt_banks_init(&s->wt_banks);

    /* Initialize sampler */
    oxs_sampler_init(&s->sampler);

    /* MIDI learn off */
    s->midi_learn_param = -1;

    /* Arpeggiator */
    oxs_arp_init(&s->arp);

    /* Oversampling buffer (4096 frames * 4x * 2 channels) */
    s->os_buf_size = OXS_OS_MAX_FRAMES * OXS_OS_MAX_FACTOR * 2;
    s->os_buf = (float *)calloc(s->os_buf_size, sizeof(float));

    /* Pre-allocate effect slots (no effects active by default) */
    for (int i = 0; i < OXS_MAX_EFFECTS; i++) {
        memset(&s->effects[i], 0, sizeof(oxs_effect_slot_t));
        s->effects[i].type = OXS_EFFECT_NONE;
    }

    return s;
}

void oxs_synth_destroy(oxs_synth_t *synth)
{
    if (!synth) return;
    /* Free effect chain buffers */
    for (int i = 0; i < OXS_MAX_EFFECTS; i++) {
        oxs_effect_free(&synth->effects[i]);
    }
    /* Free loaded samples */
    oxs_sampler_free(&synth->sampler);
    free(synth->os_buf);
    free(synth);
}

/* === Audio Processing === */

/* Arp callback — triggers/releases voices */
static void arp_note_callback(void *ctx, uint8_t note, uint8_t velocity, bool on)
{
    oxs_synth_t *synth = (oxs_synth_t *)ctx;
    if (on) {
        int max_v = (int)oxs_param_get(&synth->params, OXS_PARAM_POLY_VOICES);
        int steal = (int)oxs_param_get(&synth->params, OXS_PARAM_POLY_STEAL_MODE);
        oxs_param_snapshot_t snap;
        oxs_param_snapshot(&synth->params, &snap);
        int vi = oxs_voice_alloc(&synth->voice_pool, max_v, (oxs_steal_mode_t)steal);
        oxs_voice_trigger(&synth->voice_pool, vi, note, velocity, 0,
                          &snap, synth->sample_rate);
    } else {
        oxs_param_snapshot_t snap;
        oxs_param_snapshot(&synth->params, &snap);
        oxs_voice_release_note(&synth->voice_pool, note, 0, &snap, synth->sample_rate);
    }
}

static void trigger_voice_direct(oxs_synth_t *synth, uint8_t note, uint8_t velocity, uint8_t channel)
{
    int max_v = (int)oxs_param_get(&synth->params, OXS_PARAM_POLY_VOICES);
    int steal = (int)oxs_param_get(&synth->params, OXS_PARAM_POLY_STEAL_MODE);
    oxs_param_snapshot_t snap;
    oxs_param_snapshot(&synth->params, &snap);
    int vi = oxs_voice_alloc(&synth->voice_pool, max_v, (oxs_steal_mode_t)steal);
    oxs_voice_trigger(&synth->voice_pool, vi, note, velocity, channel,
                      &snap, synth->sample_rate);
}

static void release_voice_direct(oxs_synth_t *synth, uint8_t note, uint8_t channel)
{
    oxs_param_snapshot_t snap;
    oxs_param_snapshot(&synth->params, &snap);
    oxs_voice_release_note(&synth->voice_pool, note, channel, &snap, synth->sample_rate);
}

/* Find an active voice on a given MIDI channel (for MPE per-note messages) */
static int find_voice_by_channel(oxs_synth_t *synth, uint8_t channel)
{
    for (int i = 0; i < OXS_MAX_VOICES; i++) {
        oxs_voice_t *v = &synth->voice_pool.voices[i];
        if (v->state != OXS_VOICE_IDLE && v->channel == channel)
            return i;
    }
    return -1;
}

static void drain_command_queue(oxs_synth_t *synth)
{
    bool arp_on = oxs_param_get(&synth->params, OXS_PARAM_ARP_ENABLED) > 0.5f;

    oxs_command_t cmd;
    while (oxs_cmd_queue_pop(&synth->cmd_queue, &cmd)) {
        switch (cmd.type) {
        case OXS_CMD_NOTE_ON: {
            if (arp_on) {
                /* Feed note to arpeggiator — it will trigger voices via callback */
                oxs_arp_note_on(&synth->arp, cmd.data.note.note, cmd.data.note.velocity);
            } else {
                trigger_voice_direct(synth, cmd.data.note.note,
                                     cmd.data.note.velocity, cmd.data.note.channel);
            }
            break;
        }
        case OXS_CMD_NOTE_OFF: {
            if (arp_on) {
                oxs_arp_note_off(&synth->arp, cmd.data.note.note);
            } else {
                release_voice_direct(synth, cmd.data.note.note, cmd.data.note.channel);
            }
            break;
        }
        case OXS_CMD_PANIC: {
            oxs_param_snapshot_t snap;
            oxs_param_snapshot(&synth->params, &snap);
            oxs_voice_release_all(&synth->voice_pool, &snap, synth->sample_rate);
            oxs_arp_all_off(&synth->arp);
            break;
        }
        case OXS_CMD_MIDI_CC: {
            bool mpe_on = oxs_param_get(&synth->params, OXS_PARAM_MPE_ENABLED) > 0.5f;
            uint8_t cc_ch = cmd.data.midi_cc.channel;

            /* MPE: CC74 (slide) on member channels → per-voice */
            if (mpe_on && cc_ch >= 1 && cc_ch <= 15 && cmd.data.midi_cc.cc == 74) {
                int vi = find_voice_by_channel(synth, cc_ch);
                if (vi >= 0) {
                    synth->voice_pool.voices[vi].mpe_slide =
                        (float)cmd.data.midi_cc.value / 127.0f;
                }
                break;
            }

            /* MIDI learn mode: auto-assign this CC to the learning param */
            if (synth->midi_learn_param >= 0) {
                oxs_midi_cc_assign(&synth->cc_map, cmd.data.midi_cc.cc,
                                   synth->midi_learn_param);
                synth->midi_learn_param = -1; /* exit learn mode */
            }

            /* CC1 (mod wheel) always writes to MOD_WHEEL param */
            if (cmd.data.midi_cc.cc == 1) {
                oxs_param_set(&synth->params, OXS_PARAM_MOD_WHEEL,
                              (float)cmd.data.midi_cc.value / 127.0f);
            }
            /* Channel aftertouch (CC received as aftertouch) */
            if (cmd.data.midi_cc.cc == 128) { /* sentinel for aftertouch */
                oxs_param_set(&synth->params, OXS_PARAM_AFTERTOUCH,
                              (float)cmd.data.midi_cc.value / 127.0f);
            }

            int32_t pid = synth->cc_map.param_id[cmd.data.midi_cc.cc];
            if (pid >= 0 && pid < OXS_PARAM_COUNT) {
                /* Scale 0-127 to param range */
                oxs_param_info_t info;
                if (oxs_param_get_info(&synth->registry, (uint32_t)pid, &info)) {
                    float normalized = (float)cmd.data.midi_cc.value / 127.0f;
                    float value = info.min + normalized * (info.max - info.min);
                    oxs_param_set(&synth->params, (uint32_t)pid, value);
                }
            }
            break;
        }
        case OXS_CMD_PITCH_BEND: {
            bool mpe_on = oxs_param_get(&synth->params, OXS_PARAM_MPE_ENABLED) > 0.5f;
            uint8_t pb_ch = cmd.data.pitch_bend.channel;
            float normalized = (float)cmd.data.pitch_bend.value / 8192.0f; /* -1.0..+1.0 */

            if (mpe_on && pb_ch >= 1 && pb_ch <= 15) {
                /* MPE: per-voice pitch bend on member channel */
                int vi = find_voice_by_channel(synth, pb_ch);
                if (vi >= 0) {
                    synth->voice_pool.voices[vi].mpe_pitch_bend = normalized;
                }
            } else {
                /* Global pitch bend (channel 0 or MPE off) */
                oxs_param_set(&synth->params, OXS_PARAM_PITCH_BEND, normalized);
            }
            break;
        }
        case OXS_CMD_CHANNEL_PRESSURE: {
            bool mpe_on = oxs_param_get(&synth->params, OXS_PARAM_MPE_ENABLED) > 0.5f;
            uint8_t pr_ch = cmd.data.pressure.channel;
            float pval = (float)cmd.data.pressure.value / 127.0f;

            if (mpe_on && pr_ch >= 1 && pr_ch <= 15) {
                /* MPE: per-voice pressure on member channel */
                int vi = find_voice_by_channel(synth, pr_ch);
                if (vi >= 0) {
                    synth->voice_pool.voices[vi].mpe_pressure = pval;
                }
            } else {
                /* Global aftertouch */
                oxs_param_set(&synth->params, OXS_PARAM_AFTERTOUCH, pval);
            }
            break;
        }
        case OXS_CMD_SET_SYNTH_MODE:
            oxs_param_set(&synth->params, OXS_PARAM_SYNTH_MODE,
                          (float)cmd.data.synth_mode.mode);
            break;
        default:
            break;
        }
    }
}

static void oxs_process_internal(oxs_synth_t *synth, float *buf,
                                 uint32_t frames, uint32_t render_sr)
{
    /* Clear buffer */
    memset(buf, 0, frames * 2 * sizeof(float));

    /* Initialize mod matrix routing from snapshot */
    oxs_mod_routing_t mod_routing;
    oxs_mod_routing_init(&mod_routing, &synth->snapshot, &synth->registry);

    /* Render voices based on synth mode */
    int synth_mode = (int)synth->snapshot.values[OXS_PARAM_SYNTH_MODE];
    if (synth_mode == 2) {
        oxs_voice_render_wavetable(&synth->voice_pool, &synth->snapshot,
                                   &synth->wt_banks, &mod_routing,
                                   buf, frames, render_sr);
    } else {
        oxs_voice_render(&synth->voice_pool, &synth->snapshot,
                         &synth->wavetables, &mod_routing,
                         buf, frames, render_sr);
    }

    /* Render sampler voices (additive, same buffer) */
    oxs_sampler_render(&synth->sampler, buf, frames);

    /* Sync effect types from params and apply chain */
    {
        const uint32_t efx_bases[] = {
            OXS_PARAM_EFX0_TYPE, OXS_PARAM_EFX1_TYPE, OXS_PARAM_EFX2_TYPE
        };
        for (int slot = 0; slot < OXS_MAX_EFFECTS; slot++) {
            int wanted_type = (int)synth->snapshot.values[efx_bases[slot]];
            int wanted_bypass = (int)synth->snapshot.values[efx_bases[slot] + 1];

            /* Re-init if effect type changed */
            if (wanted_type != (int)synth->effects[slot].type) {
                oxs_effect_init(&synth->effects[slot],
                                (oxs_effect_type_t)wanted_type,
                                render_sr);
            }
            synth->effects[slot].bypass = (wanted_bypass != 0);
        }

        oxs_effects_chain_process(synth->effects, OXS_MAX_EFFECTS,
                                  buf, frames,
                                  render_sr, 120.0 /* BPM placeholder */);
    }

    /* Apply master volume */
    float master_vol = synth->snapshot.values[OXS_PARAM_MASTER_VOLUME];
    for (uint32_t i = 0; i < frames * 2; i++) {
        buf[i] *= master_vol;
    }
}

void oxs_synth_process(oxs_synth_t *synth, float *output, uint32_t num_frames)
{
    /* 1. Drain command queue */
    drain_command_queue(synth);

    /* 1b. Process arpeggiator (generates note on/off events into voices) */
    if (oxs_param_get(&synth->params, OXS_PARAM_ARP_ENABLED) > 0.5f) {
        int mode = (int)oxs_param_get(&synth->params, OXS_PARAM_ARP_MODE);
        int rate = (int)oxs_param_get(&synth->params, OXS_PARAM_ARP_RATE);
        float gate = oxs_param_get(&synth->params, OXS_PARAM_ARP_GATE);
        int octaves = (int)oxs_param_get(&synth->params, OXS_PARAM_ARP_OCTAVES);
        float bpm = oxs_param_get(&synth->params, OXS_PARAM_ARP_BPM);
        oxs_arp_process(&synth->arp, num_frames, synth->sample_rate,
                        bpm, rate, gate, mode, octaves,
                        arp_note_callback, synth);
    }

    /* 2. Snapshot atomic params */
    oxs_param_snapshot(&synth->params, &synth->snapshot);

    /* 3. Determine oversampling factor */
    int os_setting = (int)synth->snapshot.values[OXS_PARAM_OVERSAMPLING];
    int os_factor = 1;
    if (os_setting == 1) os_factor = 2;
    else if (os_setting == 2) os_factor = 4;

    /* Clamp num_frames so oversampled buffer fits on stack */
    uint32_t safe_frames = num_frames;
    if (safe_frames > OXS_OS_MAX_FRAMES) safe_frames = OXS_OS_MAX_FRAMES;

    uint32_t os_frames = safe_frames * (uint32_t)os_factor;
    uint32_t render_sr = synth->sample_rate * (uint32_t)os_factor;

    if (os_factor == 1) {
        /* No oversampling — render directly into output */
        oxs_process_internal(synth, output, safe_frames, render_sr);
    } else {
        /* Render at oversampled rate into pre-allocated buffer, then downsample */
        if (!synth->os_buf) return;
        oxs_process_internal(synth, synth->os_buf, os_frames, render_sr);

        /* Downsample: average every os_factor stereo samples */
        float inv = 1.0f / (float)os_factor;
        for (uint32_t i = 0; i < safe_frames; i++) {
            float sum_l = 0.0f, sum_r = 0.0f;
            for (int j = 0; j < os_factor; j++) {
                uint32_t src = (i * (uint32_t)os_factor + (uint32_t)j) * 2;
                sum_l += synth->os_buf[src];
                sum_r += synth->os_buf[src + 1];
            }
            output[i * 2]     = sum_l * inv;
            output[i * 2 + 1] = sum_r * inv;
        }
    }

    /* Fill oscilloscope buffer (mono mixdown) — from final output */
    {
        uint32_t wp = atomic_load_explicit(&synth->scope_write_pos, memory_order_relaxed);
        for (uint32_t i = 0; i < safe_frames; i++) {
            synth->scope_buf[wp % OXS_SCOPE_SIZE] = (output[i*2] + output[i*2+1]) * 0.5f;
            wp++;
        }
        atomic_store_explicit(&synth->scope_write_pos, wp, memory_order_release);
    }

    /* Compute peaks and push output event */
    float peak_l = 0.0f, peak_r = 0.0f;
    for (uint32_t i = 0; i < safe_frames; i++) {
        float al = fabsf(output[i * 2]);
        float ar = fabsf(output[i * 2 + 1]);
        if (al > peak_l) peak_l = al;
        if (ar > peak_r) peak_r = ar;
    }

    oxs_output_event_t ev = {
        .peak_l = peak_l,
        .peak_r = peak_r,
        .voice_active = oxs_voice_activity_mask(&synth->voice_pool),
    };
    oxs_voice_env_stages(&synth->voice_pool, ev.voice_env_stage);
    oxs_output_queue_push(&synth->output_queue, &ev);
}

/* === Parameter Access === */

void oxs_synth_set_param(oxs_synth_t *synth, uint32_t param_id, float value)
{
    oxs_param_set(&synth->params, param_id, value);
}

float oxs_synth_get_param(const oxs_synth_t *synth, uint32_t param_id)
{
    return oxs_param_get(&synth->params, param_id);
}

uint32_t oxs_synth_param_count(const oxs_synth_t *synth)
{
    return oxs_param_count(&synth->registry);
}

bool oxs_synth_param_info(const oxs_synth_t *synth, uint32_t param_id,
                          oxs_param_info_t *out)
{
    return oxs_param_get_info(&synth->registry, param_id, out);
}

int32_t oxs_synth_param_id_by_name(const oxs_synth_t *synth, const char *name)
{
    return oxs_param_id_by_name(&synth->registry, name);
}

/* === Note Events === */

void oxs_synth_note_on(oxs_synth_t *synth, uint8_t note, uint8_t velocity,
                       uint8_t channel)
{
    oxs_cmd_queue_push(&synth->cmd_queue, oxs_cmd_note_on(note, velocity, channel));
}

void oxs_synth_note_off(oxs_synth_t *synth, uint8_t note, uint8_t channel)
{
    oxs_cmd_queue_push(&synth->cmd_queue, oxs_cmd_note_off(note, channel));
}

void oxs_synth_panic(oxs_synth_t *synth)
{
    oxs_cmd_queue_push(&synth->cmd_queue, oxs_cmd_panic());
}

/* === MIDI CC === */

void oxs_synth_midi_cc(oxs_synth_t *synth, uint8_t cc, uint8_t value)
{
    oxs_cmd_queue_push(&synth->cmd_queue, oxs_cmd_midi_cc(cc, value));
}

void oxs_synth_midi_cc_channel(oxs_synth_t *synth, uint8_t cc, uint8_t value,
                                uint8_t channel)
{
    oxs_cmd_queue_push(&synth->cmd_queue, oxs_cmd_midi_cc_ch(cc, value, channel));
}

void oxs_synth_pitch_bend(oxs_synth_t *synth, int16_t value, uint8_t channel)
{
    oxs_cmd_queue_push(&synth->cmd_queue, oxs_cmd_pitch_bend(value, channel));
}

void oxs_synth_channel_pressure(oxs_synth_t *synth, uint8_t value,
                                 uint8_t channel)
{
    oxs_cmd_queue_push(&synth->cmd_queue, oxs_cmd_channel_pressure(value, channel));
}

void oxs_synth_cc_assign(oxs_synth_t *synth, uint8_t cc, int32_t param_id)
{
    oxs_midi_cc_assign(&synth->cc_map, cc, param_id);
}

void oxs_synth_cc_unassign(oxs_synth_t *synth, uint8_t cc)
{
    oxs_midi_cc_unassign(&synth->cc_map, cc);
}

/* === MIDI Learn === */

void oxs_synth_midi_learn_start(oxs_synth_t *synth, int32_t param_id)
{
    synth->midi_learn_param = param_id;
}

void oxs_synth_midi_learn_cancel(oxs_synth_t *synth)
{
    synth->midi_learn_param = -1;
}

int32_t oxs_synth_midi_learn_active(const oxs_synth_t *synth)
{
    return synth->midi_learn_param;
}

/* === Output Events === */

bool oxs_synth_pop_output_event(oxs_synth_t *synth, oxs_output_event_t *out)
{
    return oxs_output_queue_pop(&synth->output_queue, out);
}

/* === Utility === */

uint32_t oxs_synth_sample_rate(const oxs_synth_t *synth)
{
    return synth->sample_rate;
}

void oxs_synth_reset_to_default(oxs_synth_t *synth)
{
    oxs_param_store_init(&synth->params, &synth->registry);
    oxs_synth_panic(synth);
}

void oxs_synth_randomize(oxs_synth_t *synth)
{
    /* Randomize synth mode (0-2) */
    int mode = rand() % 3;
    oxs_param_set(&synth->params, OXS_PARAM_SYNTH_MODE, (float)mode);

    /* Randomize each registered param within its range */
    for (uint32_t i = 0; i < OXS_PARAM_COUNT; i++) {
        oxs_param_info_t info;
        if (!oxs_param_get_info(&synth->registry, i, &info)) continue;
        if (i == OXS_PARAM_MASTER_VOLUME) continue; /* don't randomize master */
        if (i == OXS_PARAM_SYNTH_MODE) continue;    /* already set above */

        float range = info.max - info.min;
        float val;

        if (info.flags & OXS_PARAM_FLAG_BOOLEAN) {
            val = (rand() % 2) ? 1.0f : 0.0f;
        } else if (info.flags & OXS_PARAM_FLAG_INTEGER) {
            int ival = (int)info.min + (rand() % (int)(range + 1));
            val = (float)ival;
        } else {
            float r = (float)rand() / (float)RAND_MAX;
            val = info.min + r * range;
        }

        oxs_param_set(&synth->params, i, val);
    }

    /* Ensure envelope times produce audible output */
    float att = 0.001f + (float)rand() / (float)RAND_MAX * 0.3f;
    float dec = 0.1f + (float)rand() / (float)RAND_MAX * 1.5f;
    float sus = 0.2f + (float)rand() / (float)RAND_MAX * 0.8f;
    float rel = 0.1f + (float)rand() / (float)RAND_MAX * 1.0f;
    oxs_param_set(&synth->params, OXS_PARAM_AMP_ATTACK, att);
    oxs_param_set(&synth->params, OXS_PARAM_AMP_DECAY, dec);
    oxs_param_set(&synth->params, OXS_PARAM_AMP_SUSTAIN, sus);
    oxs_param_set(&synth->params, OXS_PARAM_AMP_RELEASE, rel);

    /* Ensure filter is open enough to hear something */
    float cutoff = 200.0f + (float)rand() / (float)RAND_MAX * 15000.0f;
    oxs_param_set(&synth->params, OXS_PARAM_FILTER_CUTOFF, cutoff);

    /* Ensure at least some resonance but not extreme */
    float reso = 0.7f + (float)rand() / (float)RAND_MAX * 5.0f;
    oxs_param_set(&synth->params, OXS_PARAM_FILTER_RESONANCE, reso);

    /* Ensure polyphony is reasonable */
    oxs_param_set(&synth->params, OXS_PARAM_POLY_VOICES,
                  (float)(1 + rand() % 16));

    /* For FM mode, ensure carrier op has audible level */
    if (mode == 1) {
        oxs_param_set(&synth->params, OXS_PARAM_FM_OP0_RATIO, 1.0f);
        float op0_lvl = 0.5f + (float)rand() / (float)RAND_MAX * 0.5f;
        oxs_param_set(&synth->params, OXS_PARAM_FM_OP0_RATIO + 1, op0_lvl);
    }

    /* For subtractive, ensure osc mix isn't completely muted */
    if (mode == 0) {
        float mix = (float)rand() / (float)RAND_MAX;
        oxs_param_set(&synth->params, OXS_PARAM_OSC_MIX, mix);
        oxs_param_set(&synth->params, OXS_PARAM_UNISON_VOICES,
                      (float)(1 + rand() % 5));
    }

    /* Constrain filter type to safe types (LP, HP, BP, Notch, Ladder) */
    int ftype = rand() % 5; /* 0-4: LP, HP, BP, Notch, Ladder — skip Comb/Formant */
    oxs_param_set(&synth->params, OXS_PARAM_FILTER_TYPE, (float)ftype);

    /* Filter 2: mostly off, occasionally enable with safe settings */
    if (rand() % 4 == 0) {
        /* 25% chance of filter 2 being on */
        int f2type = 1 + rand() % 4; /* 1-4: LP, HP, BP, Notch */
        oxs_param_set(&synth->params, OXS_PARAM_FILTER2_TYPE, (float)f2type);
        float co2 = 500.0f + (float)rand() / (float)RAND_MAX * 15000.0f;
        oxs_param_set(&synth->params, OXS_PARAM_FILTER2_CUTOFF, co2);
        float res2 = 0.7f + (float)rand() / (float)RAND_MAX * 3.0f;
        oxs_param_set(&synth->params, OXS_PARAM_FILTER2_RESONANCE, res2);
        oxs_param_set(&synth->params, OXS_PARAM_FILTER2_ENV_DEPTH, 0.0f);
        oxs_param_set(&synth->params, OXS_PARAM_FILTER_ROUTING, (float)(rand() % 2));
    } else {
        oxs_param_set(&synth->params, OXS_PARAM_FILTER2_TYPE, 0); /* off */
    }

    /* Keep noise and sub at reasonable levels */
    float noise = (rand() % 3 == 0) ? (float)rand() / (float)RAND_MAX * 0.3f : 0.0f;
    oxs_param_set(&synth->params, OXS_PARAM_NOISE_LEVEL, noise);
    float sub = (rand() % 2 == 0) ? (float)rand() / (float)RAND_MAX * 0.6f : 0.0f;
    oxs_param_set(&synth->params, OXS_PARAM_SUB_LEVEL, sub);

    /* Clear mod matrix to avoid random routing killing the sound */
    for (int m = 0; m < 8; m++) {
        uint32_t base = OXS_PARAM_MOD0_SRC + (uint32_t)m * 3;
        oxs_param_set(&synth->params, base, 0); /* source = none */
        oxs_param_set(&synth->params, base + 2, 0); /* depth = 0 */
    }

    /* Reset macros */
    oxs_param_set(&synth->params, OXS_PARAM_MACRO1, 0);
    oxs_param_set(&synth->params, OXS_PARAM_MACRO2, 0);
    oxs_param_set(&synth->params, OXS_PARAM_MACRO3, 0);
    oxs_param_set(&synth->params, OXS_PARAM_MACRO4, 0);

    /* Disable arpeggiator on randomize */
    oxs_param_set(&synth->params, OXS_PARAM_ARP_ENABLED, 0);

    /* Reset effects to none so random doesn't get weird combos */
    oxs_param_set(&synth->params, OXS_PARAM_EFX0_TYPE, 0);
    oxs_param_set(&synth->params, OXS_PARAM_EFX1_TYPE, 0);
    oxs_param_set(&synth->params, OXS_PARAM_EFX2_TYPE, 0);

    oxs_synth_panic(synth);
}

void oxs_synth_load_default_preset(oxs_synth_t *synth)
{
    /* Try to load "SuperSaw" — it's a crowd-pleaser as a default */
    const char *defaults[] = {
        "presets/factory/SuperSaw.json",
        "../presets/factory/SuperSaw.json",
        NULL
    };
    for (int i = 0; defaults[i]; i++) {
        if (oxs_synth_preset_load(synth, defaults[i])) return;
    }
    /* Fallback: just use registry defaults (init saw) */
}

/* === Sampler === */

int oxs_synth_load_sample(oxs_synth_t *synth, const char *path)
{
    return oxs_sampler_load(&synth->sampler, path);
}

void oxs_synth_sample_trigger(oxs_synth_t *synth, int sample_index,
                              float velocity, int pitch_offset)
{
    float vol = oxs_param_get(&synth->params, OXS_PARAM_SAMPLER_VOLUME);
    float pan = oxs_param_get(&synth->params, OXS_PARAM_SAMPLER_PAN);
    oxs_sampler_trigger(&synth->sampler, sample_index, velocity,
                        pitch_offset, vol, pan);
}

/* === Presets === */

bool oxs_synth_preset_save(const oxs_synth_t *synth, const char *path,
                           const char *name, const char *author,
                           const char *category)
{
    return oxs_preset_save(&synth->params, &synth->registry, &synth->cc_map,
                           path, name, author, category);
}

bool oxs_synth_preset_load(oxs_synth_t *synth, const char *path)
{
    return oxs_preset_load(&synth->params, &synth->registry, &synth->cc_map,
                           path);
}

int oxs_synth_preset_list(const char *directory, char **names_out, int max)
{
    return oxs_preset_list(directory, names_out, max);
}

const char *oxs_synth_preset_user_dir(void)
{
    return oxs_preset_user_dir();
}

bool oxs_synth_session_save(const oxs_synth_t *synth)
{
    const char *user_dir = oxs_preset_user_dir();
    char path[576];
    snprintf(path, sizeof(path), "%s/../session.json", user_dir);
    return oxs_preset_save(&synth->params, &synth->registry, &synth->cc_map,
                           path, "Session", "0xSYNTH", "Session");
}

bool oxs_synth_session_load(oxs_synth_t *synth)
{
    const char *user_dir = oxs_preset_user_dir();
    char path[576];
    snprintf(path, sizeof(path), "%s/../session.json", user_dir);
    return oxs_preset_load(&synth->params, &synth->registry, &synth->cc_map,
                           path);
}

int oxs_synth_load_wavetable(oxs_synth_t *synth, const char *path, int frame_size)
{
    if (!synth || !path) return -1;
    return oxs_wt_load_wav(&synth->wt_banks, path, frame_size);
}

uint32_t oxs_synth_wavetable_bank_count(const oxs_synth_t *synth)
{
    return synth ? synth->wt_banks.num_banks : 0;
}

const char *oxs_synth_wavetable_bank_name(const oxs_synth_t *synth, uint32_t index)
{
    if (!synth || index >= synth->wt_banks.num_banks) return "";
    return synth->wt_banks.banks[index].name;
}

uint32_t oxs_synth_get_scope(const oxs_synth_t *synth, float *buf, uint32_t buf_size)
{
    if (!synth || !buf) return 0;
    uint32_t n = buf_size < OXS_SCOPE_SIZE ? buf_size : OXS_SCOPE_SIZE;
    uint32_t wp = atomic_load_explicit(&synth->scope_write_pos, memory_order_acquire);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (wp - n + i) % OXS_SCOPE_SIZE;
        buf[i] = synth->scope_buf[idx];
    }
    return n;
}
