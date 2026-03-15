/*
 * 0xSYNTH Standalone MIDI Input
 *
 * Platform-specific MIDI input that dispatches to synth API.
 * Linux: ALSA raw MIDI
 * macOS: CoreMIDI (future)
 * Windows: Windows MIDI API (future)
 */

#ifndef OXS_MIDI_H
#define OXS_MIDI_H

#include "../api/synth_api.h"
#include <stdbool.h>

typedef struct oxs_midi oxs_midi_t;

/* Create MIDI input handler. Starts a polling thread. */
oxs_midi_t *oxs_midi_create(oxs_synth_t *synth);

/* Stop and destroy MIDI input. */
void oxs_midi_destroy(oxs_midi_t *midi);

/* List available MIDI input devices to stdout. */
void oxs_midi_list_devices(void);

#endif /* OXS_MIDI_H */
