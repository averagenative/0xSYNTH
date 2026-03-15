/*
 * effects.h — Audio effects for 0xSYNTH.
 *
 * Ported from 0x808 effects system with oxs_ prefix.
 */

#ifndef OXS_EFFECTS_H
#define OXS_EFFECTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ─── Effect types ───────────────────────────────────────────────────────── */

typedef enum {
    OXS_EFFECT_NONE,
    OXS_EFFECT_FILTER,
    OXS_EFFECT_DELAY,
    OXS_EFFECT_REVERB,
    OXS_EFFECT_OVERDRIVE,
    OXS_EFFECT_FUZZ,
    OXS_EFFECT_CHORUS,
    OXS_EFFECT_BITCRUSHER,
    OXS_EFFECT_COMPRESSOR,
    OXS_EFFECT_PHASER,
    OXS_EFFECT_FLANGER,
    OXS_EFFECT_TREMOLO,
    OXS_EFFECT_RINGMOD,
    OXS_EFFECT_TAPE,
    OXS_EFFECT_SHIMMER,
    OXS_EFFECT_TYPE_COUNT
} oxs_effect_type_t;

/* ─── Biquad filter effect ───────────────────────────────────────────────── */

typedef enum {
    OXS_EFX_FILTER_LP,
    OXS_EFX_FILTER_HP,
    OXS_EFX_FILTER_BP
} oxs_efx_filter_mode_t;

typedef struct {
    oxs_efx_filter_mode_t mode;
    float cutoff;           /* Hz (20-20000) */
    float resonance;        /* Q (0.5-20) */
    float wet;              /* wet/dry mix (0-1) */
    /* State */
    float z1[2], z2[2];    /* biquad delay elements [L,R] */
    float b0, b1, b2, a1, a2; /* computed coefficients */
    float last_cutoff;      /* for detecting coefficient changes */
    float last_resonance;
} oxs_efx_filter_t;

/* ─── Delay effect ───────────────────────────────────────────────────────── */

#define DELAY_MAX_SAMPLES (48000 * 2)  /* 2 seconds at 48kHz */

typedef struct {
    float time;             /* delay time in seconds (0.001-2.0) */
    float feedback;         /* feedback amount (0-0.95) */
    float wet;              /* wet/dry mix (0-1) */
    bool  bpm_sync;         /* sync delay time to BPM */
    int   sync_division;    /* 0=1/1, 1=1/2, 2=1/4, 3=1/8 */
    /* State */
    float *buffer;          /* heap-allocated stereo circular buffer */
    int   write_pos;
    int   buffer_size;      /* actual size in stereo frames */
    bool  allocated;
} oxs_efx_delay_t;

/* ─── Reverb (Freeverb-style) ────────────────────────────────────────────── */

#define REVERB_NUM_COMBS    8
#define REVERB_NUM_ALLPASS  4
#define REVERB_COMB_MAX     2048
#define REVERB_ALLPASS_MAX  1024

typedef struct {
    float buffer[REVERB_COMB_MAX];
    int   size;
    int   idx;
    float filterstore;
} oxs_reverb_comb_t;

typedef struct {
    float buffer[REVERB_ALLPASS_MAX];
    int   size;
    int   idx;
} oxs_reverb_allpass_t;

typedef struct {
    float room_size;        /* 0.0-1.0 */
    float damping;          /* 0.0-1.0 */
    float wet;              /* wet/dry mix (0-1) */
    /* State — heap allocated on first use */
    oxs_reverb_comb_t    *combs;     /* [2][REVERB_NUM_COMBS] */
    oxs_reverb_allpass_t *allpasses; /* [2][REVERB_NUM_ALLPASS] */
    bool initialized;
} oxs_efx_reverb_t;

/* ─── Overdrive effect (soft-clipping saturation) ───────────────────────── */

typedef struct {
    float drive;            /* drive amount (0-1) */
    float tone;             /* tone control (0-1), LP filter on output */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float tone_z1[2];       /* one-pole LP filter state [L,R] */
} oxs_efx_overdrive_t;

/* ─── Fuzz effect (hard-clipping distortion) ────────────────────────────── */

typedef struct {
    float gain;             /* gain/distortion amount (0-1) */
    float tone;             /* tone control (0-1), LP filter on output */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float tone_z1[2];       /* one-pole LP filter state [L,R] */
} oxs_efx_fuzz_t;

/* ─── Chorus effect (modulated delay) ───────────────────────────────────── */

#define CHORUS_MAX_SAMPLES 4096  /* ~93ms at 44100 Hz */

typedef struct {
    float rate;             /* LFO rate in Hz (0.1-10) */
    float depth;            /* modulation depth (0-1) */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float *buffer;          /* heap-allocated stereo delay buffer */
    int   write_pos;
    int   buffer_size;
    float lfo_phase;        /* 0-1 */
    bool  allocated;
} oxs_efx_chorus_t;

/* ─── Bitcrusher effect ──────────────────────────────────────────────────── */

typedef struct {
    float bits;             /* bit depth (1-16, default 8) */
    float downsample;       /* sample-and-hold factor (1-32, default 1) */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float hold_l, hold_r;  /* held sample values */
    int   hold_count;       /* frames until next sample */
} oxs_efx_bitcrusher_t;

/* ─── Compressor effect ──────────────────────────────────────────────────── */

typedef struct {
    float threshold;        /* threshold (0-1, default 0.5) */
    float ratio;            /* compression ratio (1-20, default 4) */
    float attack;           /* attack time in seconds (0.001-0.5) */
    float release;          /* release time in seconds (0.01-1.0) */
    float makeup;           /* makeup gain (0-2, default 1) */
    /* State */
    float envelope;         /* current envelope level */
} oxs_efx_compressor_t;

/* ─── Phaser effect ──────────────────────────────────────────────────────── */

#define PHASER_NUM_STAGES 6

typedef struct {
    float rate;             /* LFO rate in Hz (0.1-10) */
    float depth;            /* modulation depth (0-1) */
    float feedback;         /* feedback amount (0-0.9) */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float lfo_phase;        /* 0-1 */
    float ap_z1[2][PHASER_NUM_STAGES]; /* allpass delay elements [ch][stage] */
    float last_out[2];      /* for feedback */
} oxs_efx_phaser_t;

/* ─── Flanger effect ─────────────────────────────────────────────────────── */

#define FLANGER_MAX_SAMPLES 512  /* ~11ms at 44100 Hz */

typedef struct {
    float rate;             /* LFO rate in Hz (0.1-10) */
    float depth;            /* modulation depth (0-1) */
    float feedback;         /* feedback (-0.9 to 0.9) */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float *buffer;          /* heap-allocated stereo delay buffer */
    int   write_pos;
    int   buffer_size;
    float lfo_phase;        /* 0-1 */
    bool  allocated;
} oxs_efx_flanger_t;

/* ─── Tremolo effect ─────────────────────────────────────────────────────── */

typedef struct {
    float rate;             /* LFO rate in Hz (0.1-20) */
    float depth;            /* modulation depth (0-1) */
    int   wave;             /* 0=sine, 1=square, 2=triangle */
    /* State */
    float lfo_phase;        /* 0-1 */
} oxs_efx_tremolo_t;

/* ─── Ring modulator effect ──────────────────────────────────────────────── */

typedef struct {
    float freq;             /* modulation frequency in Hz (20-5000) */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float phase;            /* oscillator phase (0-1) */
} oxs_efx_ringmod_t;

/* ─── Tape saturation effect ─────────────────────────────────────────────── */

typedef struct {
    float drive;            /* saturation amount (0-1) */
    float warmth;           /* low-pass warmth (0-1) */
    float mix;              /* wet/dry mix (0-1) */
    /* State */
    float warmth_z1[2];     /* one-pole LP filter state [L,R] */
} oxs_efx_tape_t;

/* ─── Shimmer reverb effect ──────────────────────────────────────────────── */

#define SHIMMER_BUF_SIZE 48000  /* 1 second at 48kHz */

typedef struct {
    float decay;            /* reverb decay (0-1) */
    float shimmer;          /* pitch-shift feedback amount (0-1) */
    float mix;              /* wet/dry mix (0-1) */
    /* State — heap allocated */
    float *buffer;          /* circular reverb buffer (stereo) */
    int   write_pos;
    int   buffer_size;
    float read_phase;       /* fractional read position for pitch shift */
    float ap_z1[2];         /* allpass for phaser modulation */
    float lfo_phase;        /* phaser LFO */
    bool  allocated;
} oxs_efx_shimmer_t;

/* ─── Generic effect slot ────────────────────────────────────────────────── */

#define OXS_MAX_EFFECTS 3

typedef struct {
    oxs_effect_type_t type;
    bool bypass;
    union {
        oxs_efx_filter_t    filter;
        oxs_efx_delay_t     delay;
        oxs_efx_reverb_t    reverb;
        oxs_efx_overdrive_t overdrive;
        oxs_efx_fuzz_t      fuzz;
        oxs_efx_chorus_t    chorus;
        oxs_efx_bitcrusher_t bitcrusher;
        oxs_efx_compressor_t compressor;
        oxs_efx_phaser_t    phaser;
        oxs_efx_flanger_t   flanger;
        oxs_efx_tremolo_t   tremolo;
        oxs_efx_ringmod_t   ringmod;
        oxs_efx_tape_t      tape;
        oxs_efx_shimmer_t   shimmer;
    };
} oxs_effect_slot_t;

/* ─── API ────────────────────────────────────────────────────────────────── */

/* Free heap-allocated buffers inside an effect slot (delay buffer, reverb
   comb/allpass arrays).  Resets type to OXS_EFFECT_NONE.  Safe to call on an
   already-empty or zeroed slot. */
void oxs_effect_free(oxs_effect_slot_t *slot);

/* Initialize effect to default state (calls oxs_effect_free first to avoid leaks) */
void oxs_effect_init(oxs_effect_slot_t *slot, oxs_effect_type_t type, uint32_t sample_rate);

/* Process a stereo buffer through an effect (in-place) */
void oxs_effect_process(oxs_effect_slot_t *slot, float *buffer, uint32_t num_frames,
                        uint32_t sample_rate, double bpm);

/* Process a chain of effects */
void oxs_effects_chain_process(oxs_effect_slot_t *slots, int num_slots,
                               float *buffer, uint32_t num_frames,
                               uint32_t sample_rate, double bpm);

#ifdef __cplusplus
}
#endif

#endif /* OXS_EFFECTS_H */
