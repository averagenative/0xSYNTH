/*
 * 0xSYNTH Step Sequencer Implementation
 */

#include "sequencer.h"
#include <string.h>

/* Xorshift32 PRNG — deterministic, no stdlib dependency */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void oxs_seq_init(oxs_sequencer_t *seq)
{
    memset(seq, 0, sizeof(*seq));
    seq->direction = 1;
    seq->rng_state = 0xDEADBEEF;

    /* Default pattern: C minor pentatonic over 8 steps */
    /* C3=48, Eb3=51, F3=53, G3=55, Bb3=58, C4=60, Bb3=58, G3=55 */
    static const uint8_t default_notes[] = { 48, 51, 53, 55, 58, 60, 58, 55 };
    for (int i = 0; i < 8; i++) {
        seq->steps[i].note     = default_notes[i];
        seq->steps[i].velocity = 100;
        seq->steps[i].slide    = 0;
        seq->steps[i].accent   = 0;
        seq->steps[i].gate_pct = 0.5f;
    }
    /* Remaining steps are zeroed (velocity=0 means rest) */
}

void oxs_seq_reset(oxs_sequencer_t *seq)
{
    seq->current_step   = 0;
    seq->direction      = 1;
    seq->note_is_on     = false;
    seq->current_note   = 0;
    seq->sample_counter = 0;
    seq->gate_samples   = 0;
    seq->playing        = false;
}

void oxs_seq_set_step(oxs_sequencer_t *seq, int index, const oxs_seq_step_t *step)
{
    if (index >= 0 && index < OXS_SEQ_MAX_STEPS) {
        seq->steps[index] = *step;
    }
}

void oxs_seq_get_step(const oxs_sequencer_t *seq, int index, oxs_seq_step_t *out)
{
    if (index >= 0 && index < OXS_SEQ_MAX_STEPS) {
        *out = seq->steps[index];
    } else {
        memset(out, 0, sizeof(*out));
    }
}

/* Advance to next step based on direction mode */
static void advance_step(oxs_sequencer_t *seq, int length, int dir_mode)
{
    switch (dir_mode) {
    case 0: /* Forward */
        seq->current_step++;
        if (seq->current_step >= length)
            seq->current_step = 0;
        break;

    case 1: /* Reverse */
        seq->current_step--;
        if (seq->current_step < 0)
            seq->current_step = length - 1;
        break;

    case 2: /* Ping-pong */
        seq->current_step += seq->direction;
        if (seq->current_step >= length - 1) {
            seq->current_step = length - 1;
            seq->direction = -1;
        }
        if (seq->current_step <= 0) {
            seq->current_step = 0;
            seq->direction = 1;
        }
        break;

    case 3: /* Random */
        seq->current_step = (int)(xorshift32(&seq->rng_state) % (uint32_t)length);
        break;

    default:
        seq->current_step++;
        if (seq->current_step >= length)
            seq->current_step = 0;
        break;
    }
}


void oxs_seq_process(oxs_sequencer_t *seq, uint32_t num_frames,
                     uint32_t sample_rate, float bpm, float swing,
                     int length, int dir_mode,
                     oxs_seq_note_cb callback, void *ctx)
{
    if (length <= 0) return;
    if (bpm < 20.0f) bpm = 20.0f;
    if (bpm > 300.0f) bpm = 300.0f;

    /* Step duration in samples: 1/16th note = (60/bpm)/4 seconds */
    double base_step_samples = 60.0 / (double)bpm * (double)sample_rate / 4.0;

    /* Start playing on first call */
    if (!seq->playing) {
        seq->playing = true;
        seq->sample_counter = base_step_samples; /* trigger immediately */
    }

    for (uint32_t i = 0; i < num_frames; i++) {
        seq->sample_counter++;

        /* Calculate step duration with swing applied */
        /* Swing: 0.5 = no swing, >0.5 = odd steps delayed, <0.5 = odd steps early */
        bool is_odd_step = (seq->current_step & 1) != 0;
        double step_samples;
        if (is_odd_step) {
            step_samples = base_step_samples * (double)(swing * 2.0f);
        } else {
            step_samples = base_step_samples * (double)((1.0f - swing) * 2.0f);
        }
        /* Clamp to reasonable range */
        if (step_samples < 1.0) step_samples = 1.0;

        /* Gate off: release note when gate period expires (unless slide) */
        if (seq->note_is_on && seq->sample_counter >= seq->gate_samples) {
            /* Check if current step has slide — if so, don't release yet */
            oxs_seq_step_t *cur = &seq->steps[seq->current_step];
            if (!cur->slide) {
                callback(ctx, seq->current_note, 0, false);
                seq->note_is_on = false;
            }
        }

        /* Step trigger: advance to next note */
        if (seq->sample_counter >= step_samples) {
            seq->sample_counter -= step_samples;

            /* If slide is active on current step, trigger next note BEFORE releasing */
            oxs_seq_step_t *prev_step = &seq->steps[seq->current_step];
            bool was_sliding = seq->note_is_on && prev_step->slide;

            /* Advance step */
            advance_step(seq, length, dir_mode);

            oxs_seq_step_t *step = &seq->steps[seq->current_step];

            /* Recalculate step_samples for new step (swing may differ) */
            bool new_is_odd = (seq->current_step & 1) != 0;
            double new_step_samples;
            if (new_is_odd) {
                new_step_samples = base_step_samples * (double)(swing * 2.0f);
            } else {
                new_step_samples = base_step_samples * (double)((1.0f - swing) * 2.0f);
            }
            if (new_step_samples < 1.0) new_step_samples = 1.0;

            if (step->velocity > 0) {
                /* Trigger new note */
                uint8_t vel = step->velocity;
                if (step->accent) {
                    vel = (uint8_t)((vel + 40 > 127) ? 127 : vel + 40);
                }

                /* Use step's own gate_pct, or fallback to step duration */
                float gate_pct = step->gate_pct;
                if (gate_pct < 0.1f) gate_pct = 0.1f;
                if (gate_pct > 1.0f) gate_pct = 1.0f;
                seq->gate_samples = new_step_samples * (double)gate_pct;

                /* For slide: trigger new note before releasing old */
                callback(ctx, step->note, vel, true);
                uint8_t new_note = step->note;

                /* Now release previous note if it was still on */
                if (was_sliding) {
                    callback(ctx, seq->current_note, 0, false);
                } else if (seq->note_is_on) {
                    callback(ctx, seq->current_note, 0, false);
                }

                seq->current_note = new_note;
                seq->note_is_on = true;
            } else {
                /* Rest step — release any playing note */
                if (seq->note_is_on) {
                    callback(ctx, seq->current_note, 0, false);
                    seq->note_is_on = false;
                }
                /* Still set gate_samples so counter works */
                seq->gate_samples = new_step_samples * 0.5;
            }
        }
    }
}
