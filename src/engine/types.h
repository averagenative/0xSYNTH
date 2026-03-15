/*
 * 0xSYNTH Shared Types
 *
 * Types used by both the public API and internal engine.
 * Included by synth_api.h and engine headers.
 */

#ifndef OXS_TYPES_H
#define OXS_TYPES_H

#include <stdint.h>

#define OXS_MAX_VOICES 16

/* Parameter metadata — exposed to consumers for CLAP bridging, UI generation */
typedef struct {
    uint32_t    id;
    char        name[48];
    char        group[24];
    float       min;
    float       max;
    float       default_val;
    float       step;
    char        units[8];
    uint32_t    flags;
} oxs_param_info_t;

/* Output event (audio→GUI readback) */
typedef struct {
    float     peak_l;
    float     peak_r;
    uint16_t  voice_active;
    uint8_t   voice_env_stage[OXS_MAX_VOICES];
} oxs_output_event_t;

/* Parameter flags */
#define OXS_PARAM_FLAG_AUTOMATABLE  (1u << 0)
#define OXS_PARAM_FLAG_INTEGER      (1u << 1)
#define OXS_PARAM_FLAG_BOOLEAN      (1u << 2)
#define OXS_PARAM_FLAG_MODULATABLE  (1u << 3)

#endif /* OXS_TYPES_H */
