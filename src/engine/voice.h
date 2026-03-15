/*
 * 0xSYNTH Voice Manager
 *
 * Polyphonic voice pool with allocation, stealing, and release.
 * Each voice holds per-note state for oscillators, envelopes, filter, LFO.
 */

#ifndef OXS_VOICE_H
#define OXS_VOICE_H

#include "types.h"
#include "envelope.h"
#include "filter.h"
#include "oscillator.h"
#include "params.h"

#include <stdint.h>
#include <stdbool.h>

#define OXS_MAX_UNISON 7

typedef enum {
    OXS_VOICE_IDLE = 0,
    OXS_VOICE_ACTIVE,
    OXS_VOICE_RELEASING
} oxs_voice_state_t;

typedef enum {
    OXS_STEAL_OLDEST = 0,
    OXS_STEAL_QUIETEST,
    OXS_STEAL_LOWEST,
    OXS_STEAL_HIGHEST
} oxs_steal_mode_t;

typedef struct oxs_voice_s {
    oxs_voice_state_t  state;
    uint8_t            note;        /* MIDI note number */
    uint8_t            channel;
    float              frequency;   /* Hz, computed from note */
    float              velocity;    /* 0.0–1.0, from MIDI velocity / 127 */
    uint64_t           start_time;  /* sample counter at trigger */

    /* Oscillator state */
    double             osc1_phase;
    double             osc2_phase;
    double             unison_phases[OXS_MAX_UNISON];

    /* Envelopes */
    oxs_envelope_t     amp_env;
    oxs_envelope_t     filter_env;

    /* Filter state */
    oxs_filter_state_t filter;
    float              smoothed_cutoff;

    /* LFO state (per-voice, starts at phase 0 on trigger) */
    oxs_lfo_t          lfo;

    /* FM operator state (used only in FM mode, defined here for allocation) */
    double             fm_phase[4];
    float              fm_feedback_state[4];
    oxs_envelope_t     fm_env[4];

    /* Wavetable state */
    double             wt_phase;
    float              wt_smoothed_pos;
} oxs_voice_t;

typedef struct {
    oxs_voice_t voices[OXS_MAX_VOICES];
    uint64_t    sample_counter;  /* monotonic counter for voice age */
} oxs_voice_pool_t;

/* Initialize voice pool */
void oxs_voice_pool_init(oxs_voice_pool_t *pool);

/* Allocate a voice for a new note. Steals if pool is full. */
int oxs_voice_alloc(oxs_voice_pool_t *pool, int max_voices,
                    oxs_steal_mode_t steal_mode);

/* Trigger a voice with note parameters */
void oxs_voice_trigger(oxs_voice_pool_t *pool, int voice_idx,
                       uint8_t note, uint8_t velocity, uint8_t channel,
                       const oxs_param_snapshot_t *snap, uint32_t sample_rate);

/* Release a specific note (finds matching voice) */
void oxs_voice_release_note(oxs_voice_pool_t *pool, uint8_t note,
                            uint8_t channel,
                            const oxs_param_snapshot_t *snap,
                            uint32_t sample_rate);

/* Release all active voices (panic) */
void oxs_voice_release_all(oxs_voice_pool_t *pool,
                           const oxs_param_snapshot_t *snap,
                           uint32_t sample_rate);

/* Render all active voices into output buffer (additive).
 * Dispatches to subtractive, FM, or wavetable based on synth mode. */
void oxs_voice_render(oxs_voice_pool_t *pool,
                      const oxs_param_snapshot_t *snap,
                      const oxs_wavetables_t *wt,
                      float *output, uint32_t num_frames,
                      uint32_t sample_rate);

/* Mode-specific render paths (called by oxs_voice_render) */
void oxs_voice_render_subtractive(oxs_voice_pool_t *pool,
                                  const oxs_param_snapshot_t *snap,
                                  const oxs_wavetables_t *wt,
                                  float *output, uint32_t num_frames,
                                  uint32_t sample_rate);

void oxs_voice_render_fm(oxs_voice_pool_t *pool,
                         const oxs_param_snapshot_t *snap,
                         float *output, uint32_t num_frames,
                         uint32_t sample_rate);

/* Forward declare wavetable banks */
struct oxs_wt_banks_t_tag;
void oxs_voice_render_wavetable(oxs_voice_pool_t *pool,
                                const oxs_param_snapshot_t *snap,
                                const void *wt_banks, /* oxs_wt_banks_t* */
                                float *output, uint32_t num_frames,
                                uint32_t sample_rate);

/* Get voice activity bitmask for output events */
uint16_t oxs_voice_activity_mask(const oxs_voice_pool_t *pool);

/* Get envelope stages for output events */
void oxs_voice_env_stages(const oxs_voice_pool_t *pool,
                          uint8_t stages[OXS_MAX_VOICES]);

#endif /* OXS_VOICE_H */
