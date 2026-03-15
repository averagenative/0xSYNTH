/*
 * 0xSYNTH Public API
 *
 * This is the SOLE interface for all consumers (GTK GUI, CLAP/VST3 plugin,
 * standalone app, custom embedders). No internal headers should be included
 * from outside src/engine/.
 *
 * Thread safety:
 *   - oxs_param_set/get: safe to call from any thread (atomic)
 *   - oxs_synth_note_on/off/panic: safe from GUI thread (command queue)
 *   - oxs_synth_process: called from audio thread only
 *   - oxs_synth_pop_output_event: safe from GUI thread
 *   - oxs_synth_create/destroy: not thread-safe, call from main thread
 */

#ifndef OXS_SYNTH_API_H
#define OXS_SYNTH_API_H

#include <stdint.h>
#include <stdbool.h>
#include "../engine/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque synth handle — internals hidden */
typedef struct oxs_synth oxs_synth_t;

/* Total parameter slots (for iteration) */
#define OXS_PARAM_SLOT_COUNT 200

/* === Lifecycle === */

/* Create a new synth instance. sample_rate is in Hz (e.g., 44100, 48000). */
oxs_synth_t *oxs_synth_create(uint32_t sample_rate);

/* Destroy a synth instance and free all resources. */
void oxs_synth_destroy(oxs_synth_t *synth);

/* === Audio Processing (audio thread only) === */

/* Render num_frames of stereo interleaved audio into output buffer.
 * output must hold at least num_frames * 2 floats. */
void oxs_synth_process(oxs_synth_t *synth, float *output, uint32_t num_frames);

/* === Parameter Access (any thread) === */

/* Set a parameter value. Thread-safe (atomic). */
void oxs_synth_set_param(oxs_synth_t *synth, uint32_t param_id, float value);

/* Get a parameter value. Thread-safe (atomic). */
float oxs_synth_get_param(const oxs_synth_t *synth, uint32_t param_id);

/* Get total number of registered parameters. */
uint32_t oxs_synth_param_count(const oxs_synth_t *synth);

/* Get parameter metadata. Returns false if id is invalid. */
bool oxs_synth_param_info(const oxs_synth_t *synth, uint32_t param_id,
                          oxs_param_info_t *out);

/* Find parameter ID by name. Returns -1 if not found. */
int32_t oxs_synth_param_id_by_name(const oxs_synth_t *synth, const char *name);

/* === Note Events (GUI thread → command queue) === */

/* Trigger a note. velocity: 1-127, channel: 0-15. */
void oxs_synth_note_on(oxs_synth_t *synth, uint8_t note, uint8_t velocity,
                       uint8_t channel);

/* Release a note. */
void oxs_synth_note_off(oxs_synth_t *synth, uint8_t note, uint8_t channel);

/* All notes off — release all active voices immediately. */
void oxs_synth_panic(oxs_synth_t *synth);

/* === MIDI CC (GUI thread → command queue) === */

/* Send a MIDI CC message. */
void oxs_synth_midi_cc(oxs_synth_t *synth, uint8_t cc, uint8_t value);

/* Assign a MIDI CC to a parameter. */
void oxs_synth_cc_assign(oxs_synth_t *synth, uint8_t cc, int32_t param_id);

/* Remove a MIDI CC assignment. */
void oxs_synth_cc_unassign(oxs_synth_t *synth, uint8_t cc);

/* === Output Events (GUI thread reads) === */

/* Pop the next output event (peaks, voice activity). Returns false if empty. */
bool oxs_synth_pop_output_event(oxs_synth_t *synth, oxs_output_event_t *out);

/* === Utility === */

/* Get the sample rate this instance was created with. */
uint32_t oxs_synth_sample_rate(const oxs_synth_t *synth);

/* === Presets === */

/* Save current state to a JSON preset file. */
bool oxs_synth_preset_save(const oxs_synth_t *synth, const char *path,
                           const char *name, const char *author,
                           const char *category);

/* Load a preset from a JSON file. */
bool oxs_synth_preset_load(oxs_synth_t *synth, const char *path);

/* List presets in a directory. Returns count. Caller frees each name. */
int oxs_synth_preset_list(const char *directory, char **names_out, int max);

/* Get platform-specific user preset directory. */
const char *oxs_synth_preset_user_dir(void);

#ifdef __cplusplus
}
#endif

#endif /* OXS_SYNTH_API_H */
