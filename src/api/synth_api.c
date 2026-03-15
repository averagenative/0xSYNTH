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

#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    free(synth);
}

/* === Audio Processing === */

static void drain_command_queue(oxs_synth_t *synth)
{
    oxs_command_t cmd;
    while (oxs_cmd_queue_pop(&synth->cmd_queue, &cmd)) {
        switch (cmd.type) {
        case OXS_CMD_NOTE_ON: {
            int max_v = (int)oxs_param_get(&synth->params, OXS_PARAM_POLY_VOICES);
            int steal = (int)oxs_param_get(&synth->params, OXS_PARAM_POLY_STEAL_MODE);
            oxs_param_snapshot_t snap;
            oxs_param_snapshot(&synth->params, &snap);
            int vi = oxs_voice_alloc(&synth->voice_pool, max_v,
                                     (oxs_steal_mode_t)steal);
            oxs_voice_trigger(&synth->voice_pool, vi,
                              cmd.data.note.note, cmd.data.note.velocity,
                              cmd.data.note.channel, &snap, synth->sample_rate);
            break;
        }
        case OXS_CMD_NOTE_OFF: {
            oxs_param_snapshot_t snap;
            oxs_param_snapshot(&synth->params, &snap);
            oxs_voice_release_note(&synth->voice_pool,
                                   cmd.data.note.note, cmd.data.note.channel,
                                   &snap, synth->sample_rate);
            break;
        }
        case OXS_CMD_PANIC: {
            oxs_param_snapshot_t snap;
            oxs_param_snapshot(&synth->params, &snap);
            oxs_voice_release_all(&synth->voice_pool, &snap, synth->sample_rate);
            break;
        }
        case OXS_CMD_MIDI_CC: {
            /* MIDI learn mode: auto-assign this CC to the learning param */
            if (synth->midi_learn_param >= 0) {
                oxs_midi_cc_assign(&synth->cc_map, cmd.data.midi_cc.cc,
                                   synth->midi_learn_param);
                synth->midi_learn_param = -1; /* exit learn mode */
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
        case OXS_CMD_SET_SYNTH_MODE:
            oxs_param_set(&synth->params, OXS_PARAM_SYNTH_MODE,
                          (float)cmd.data.synth_mode.mode);
            break;
        default:
            break;
        }
    }
}

void oxs_synth_process(oxs_synth_t *synth, float *output, uint32_t num_frames)
{
    /* 1. Drain command queue */
    drain_command_queue(synth);

    /* 2. Snapshot atomic params */
    oxs_param_snapshot(&synth->params, &synth->snapshot);

    /* 3. Clear output buffer */
    memset(output, 0, num_frames * 2 * sizeof(float));

    /* 4. Render voices based on synth mode */
    int synth_mode = (int)synth->snapshot.values[OXS_PARAM_SYNTH_MODE];
    if (synth_mode == 2) {
        /* Wavetable needs wt_banks from synth handle */
        oxs_voice_render_wavetable(&synth->voice_pool, &synth->snapshot,
                                   &synth->wt_banks, output, num_frames,
                                   synth->sample_rate);
    } else {
        /* Subtractive and FM handled by voice dispatch */
        oxs_voice_render(&synth->voice_pool, &synth->snapshot,
                         &synth->wavetables, output, num_frames,
                         synth->sample_rate);
    }

    /* 4b. Render sampler voices (additive, same buffer) */
    oxs_sampler_render(&synth->sampler, output, num_frames);

    /* 5. Sync effect types from params and apply chain */
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
                                synth->sample_rate);
            }
            synth->effects[slot].bypass = (wanted_bypass != 0);

            /* TODO: map generic P0-P7 params to effect-specific fields
             * based on effect type. For now effects use their init defaults. */
        }

        oxs_effects_chain_process(synth->effects, OXS_MAX_EFFECTS,
                                  output, num_frames,
                                  synth->sample_rate, 120.0 /* BPM placeholder */);
    }

    /* 6. Apply master volume */
    float master_vol = synth->snapshot.values[OXS_PARAM_MASTER_VOLUME];
    for (uint32_t i = 0; i < num_frames * 2; i++) {
        output[i] *= master_vol;
    }

    /* 8. Compute peaks and push output event */
    float peak_l = 0.0f, peak_r = 0.0f;
    for (uint32_t i = 0; i < num_frames; i++) {
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
