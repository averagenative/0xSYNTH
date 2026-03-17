/*
 * 0xSYNTH Step Sequencer
 *
 * 32-step sequencer with per-step note, velocity, gate, slide, and accent.
 * Modes: Forward, Reverse, Ping-Pong, Random.
 * Swing control for groove. Accent boosts velocity.
 * Slide creates legato overlap between consecutive steps.
 */

#ifndef OXS_SEQUENCER_H
#define OXS_SEQUENCER_H

#include <stdint.h>
#include <stdbool.h>

#define OXS_SEQ_MAX_STEPS 32

typedef struct {
    uint8_t  note;       /* MIDI note 0-127 */
    uint8_t  velocity;   /* 0-127, 0 = rest */
    uint8_t  slide;      /* 0 or 1 — legato to next step */
    uint8_t  accent;     /* 0 or 1 — velocity boost */
    float    gate_pct;   /* 0.1-1.0, fraction of step duration */
} oxs_seq_step_t;

typedef struct {
    oxs_seq_step_t steps[OXS_SEQ_MAX_STEPS];
    int      current_step;
    int      direction;       /* +1 or -1 for ping-pong */
    bool     note_is_on;
    uint8_t  current_note;
    double   sample_counter;
    double   gate_samples;    /* how many samples the current note should play */
    bool     playing;
    uint32_t rng_state;
} oxs_sequencer_t;

/* Callback type for note events */
typedef void (*oxs_seq_note_cb)(void *ctx, uint8_t note, uint8_t velocity, bool on);

/* Initialize sequencer with default C minor pentatonic pattern */
void oxs_seq_init(oxs_sequencer_t *seq);

/* Reset playback state (stop and rewind) */
void oxs_seq_reset(oxs_sequencer_t *seq);

/* Set a step's data */
void oxs_seq_set_step(oxs_sequencer_t *seq, int index, const oxs_seq_step_t *step);

/* Get a step's data */
void oxs_seq_get_step(const oxs_sequencer_t *seq, int index, oxs_seq_step_t *out);

/* Process one audio buffer — triggers note on/off events via callback.
 * direction: 0=fwd, 1=rev, 2=pingpong, 3=random */
void oxs_seq_process(oxs_sequencer_t *seq, uint32_t num_frames,
                     uint32_t sample_rate, float bpm, float swing,
                     int length, int dir_mode,
                     oxs_seq_note_cb callback, void *ctx);

#endif /* OXS_SEQUENCER_H */
