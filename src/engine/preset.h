/*
 * 0xSYNTH Preset Management
 *
 * JSON-based preset serialization with factory + user preset support.
 * Uses cJSON for parsing/generation.
 *
 * Preset JSON format:
 * {
 *   "name": "FM Bell",
 *   "author": "0xSYNTH",
 *   "category": "Keys",
 *   "params": {
 *     "Master Volume": 0.8,
 *     "Synth Mode": 1,
 *     "FM Algorithm": 0,
 *     ...
 *   },
 *   "midi_cc_map": {
 *     "1": "Filter Cutoff",
 *     "74": "Filter Resonance"
 *   }
 * }
 */

#ifndef OXS_PRESET_H
#define OXS_PRESET_H

#include "params.h"
#include <stdint.h>
#include <stdbool.h>

/* Maximum presets in a listing */
#define OXS_MAX_PRESET_LIST 256

/* Save current synth state to a JSON preset file.
 * name/author/category are optional metadata (can be NULL). */
bool oxs_preset_save(const oxs_param_store_t *store,
                     const oxs_param_registry_t *reg,
                     const oxs_midi_cc_map_t *cc_map,
                     const char *path,
                     const char *name,
                     const char *author,
                     const char *category);

/* Load a preset from a JSON file into the param store.
 * Validates param ranges against registry. Returns true on success. */
bool oxs_preset_load(oxs_param_store_t *store,
                     const oxs_param_registry_t *reg,
                     oxs_midi_cc_map_t *cc_map,
                     const char *path);

/* List preset files in a directory.
 * Returns count of presets found. Names are allocated — caller must free each. */
int oxs_preset_list(const char *directory, char **names_out, int max_names);

/* Get platform-specific user preset directory path.
 * Returns static string — do not free. Creates directory if it doesn't exist. */
const char *oxs_preset_user_dir(void);

/* Get factory preset directory path (relative to executable). */
const char *oxs_preset_factory_dir(void);

#endif /* OXS_PRESET_H */
