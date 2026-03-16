/*
 * 0xSYNTH Arpeggiator
 *
 * Captures held notes and plays them in a pattern at a tempo-synced rate.
 * Modes: Up, Down, Up-Down, Random, As-Played.
 */

#ifndef OXS_ARPEGGIATOR_H
#define OXS_ARPEGGIATOR_H

#include <stdint.h>
#include <stdbool.h>

#define OXS_ARP_MAX_NOTES 32

typedef enum {
    OXS_ARP_UP = 0,
    OXS_ARP_DOWN,
    OXS_ARP_UPDOWN,
    OXS_ARP_RANDOM,
    OXS_ARP_AS_PLAYED
} oxs_arp_mode_t;

typedef struct {
    /* Held notes buffer (notes currently held by the user) */
    uint8_t  held_notes[OXS_ARP_MAX_NOTES];
    uint8_t  held_velocities[OXS_ARP_MAX_NOTES];
    int      held_count;

    /* Sorted note list for playback (includes octave expansion) */
    uint8_t  play_notes[OXS_ARP_MAX_NOTES * 4]; /* up to 4 octaves */
    int      play_count;

    /* Playback state */
    int      current_index;    /* current position in play_notes */
    int      direction;        /* +1 or -1 for up-down mode */
    bool     note_is_on;       /* is the current arp note sounding? */
    uint8_t  current_note;     /* MIDI note currently playing */
    double   sample_counter;   /* samples since last step */
    double   gate_counter;     /* samples since note-on (for gate off) */

    bool     enabled;
} oxs_arpeggiator_t;

/* Initialize arpeggiator */
void oxs_arp_init(oxs_arpeggiator_t *arp);

/* Add/remove held notes (called from note_on/note_off commands) */
void oxs_arp_note_on(oxs_arpeggiator_t *arp, uint8_t note, uint8_t velocity);
void oxs_arp_note_off(oxs_arpeggiator_t *arp, uint8_t note);
void oxs_arp_all_off(oxs_arpeggiator_t *arp);

/* Rebuild the play list based on mode and octave range */
void oxs_arp_rebuild(oxs_arpeggiator_t *arp, int mode, int octaves);

/* Process one audio buffer — triggers note on/off events via callback.
 * Returns the number of note events generated. */
typedef void (*oxs_arp_note_cb)(void *ctx, uint8_t note, uint8_t velocity, bool on);
void oxs_arp_process(oxs_arpeggiator_t *arp, uint32_t num_frames,
                     uint32_t sample_rate, float bpm, int rate_div,
                     float gate, int mode, int octaves,
                     oxs_arp_note_cb callback, void *ctx);

#endif /* OXS_ARPEGGIATOR_H */
