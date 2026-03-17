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

/* Create MIDI input handler (default device). Starts a polling thread. */
oxs_midi_t *oxs_midi_create(oxs_synth_t *synth);

/* Create MIDI input handler for a specific device index. */
oxs_midi_t *oxs_midi_create_device(oxs_synth_t *synth, int device_index);

/* Stop and destroy MIDI input. */
void oxs_midi_destroy(oxs_midi_t *midi);

/* List available MIDI input devices to stdout. */
void oxs_midi_list_devices(void);

/* Get number of available MIDI input devices. */
int oxs_midi_get_device_count(void);

/* Get name of MIDI input device at index. Returns NULL if invalid. */
const char *oxs_midi_get_device_name(int index);

#endif /* OXS_MIDI_H */
