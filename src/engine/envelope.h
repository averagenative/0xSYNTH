/*
 * 0xSYNTH ADSR Envelope and LFO
 *
 * Rate-based state machine for amplitude/filter shaping.
 * Ported from 0x808 envelope.c.
 */

#ifndef OXS_ENVELOPE_H
#define OXS_ENVELOPE_H

#include <stdint.h>

typedef enum {
    OXS_ENV_IDLE = 0,
    OXS_ENV_ATTACK,
    OXS_ENV_DECAY,
    OXS_ENV_SUSTAIN,
    OXS_ENV_RELEASE
} oxs_env_stage_t;

typedef struct {
    float attack;   /* seconds */
    float decay;    /* seconds */
    float sustain;  /* 0.0–1.0 */
    float release;  /* seconds */
} oxs_adsr_params_t;

typedef struct {
    oxs_env_stage_t stage;
    float           level;  /* 0.0–1.0 */
    float           rate;   /* per-sample increment */
} oxs_envelope_t;

typedef enum {
    OXS_LFO_DEST_NONE = 0,
    OXS_LFO_DEST_PITCH,
    OXS_LFO_DEST_FILTER,
    OXS_LFO_DEST_AMP
} oxs_lfo_dest_t;

typedef struct {
    int     waveform;   /* oxs_waveform_t */
    float   rate;       /* Hz */
    float   depth;      /* 0.0–1.0 */
    int     dest;       /* oxs_lfo_dest_t */
    double  phase;      /* 0.0–1.0 */
} oxs_lfo_t;

/* ADSR */
void  oxs_envelope_init(oxs_envelope_t *env);
void  oxs_envelope_trigger(oxs_envelope_t *env, const oxs_adsr_params_t *params,
                           uint32_t sample_rate);
void  oxs_envelope_release(oxs_envelope_t *env, const oxs_adsr_params_t *params,
                           uint32_t sample_rate);
float oxs_envelope_process(oxs_envelope_t *env, const oxs_adsr_params_t *params,
                           uint32_t sample_rate);

/* LFO */
void  oxs_lfo_init(oxs_lfo_t *lfo);
float oxs_lfo_process(oxs_lfo_t *lfo, uint32_t sample_rate);

/* BPM sync helper */
float oxs_lfo_bpm_sync_rate(double bpm, int division);

#endif /* OXS_ENVELOPE_H */
