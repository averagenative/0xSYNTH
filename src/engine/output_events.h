/*
 * 0xSYNTH Output Event Queue (Audio → GUI)
 *
 * SPSC ring buffer for transient state: peak meters, voice activity, envelope stages.
 * Audio thread pushes one event per process() call.
 * GUI thread pops at its own rate (~30-60fps).
 */

#ifndef OXS_OUTPUT_EVENTS_H
#define OXS_OUTPUT_EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "types.h"

#define OXS_OUTPUT_QUEUE_SIZE 64

typedef struct {
    oxs_output_event_t events[OXS_OUTPUT_QUEUE_SIZE];
    _Atomic uint32_t   write_idx;
    _Atomic uint32_t   read_idx;
} oxs_output_queue_t;

void oxs_output_queue_init(oxs_output_queue_t *q);
bool oxs_output_queue_push(oxs_output_queue_t *q, const oxs_output_event_t *ev);
bool oxs_output_queue_pop(oxs_output_queue_t *q, oxs_output_event_t *out);

#endif /* OXS_OUTPUT_EVENTS_H */
