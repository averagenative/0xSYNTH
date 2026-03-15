/*
 * 0xSYNTH Output Event Queue Implementation
 */

#include "output_events.h"
#include <string.h>

void oxs_output_queue_init(oxs_output_queue_t *q)
{
    memset(q->events, 0, sizeof(q->events));
    atomic_store_explicit(&q->write_idx, 0, memory_order_relaxed);
    atomic_store_explicit(&q->read_idx, 0, memory_order_relaxed);
}

bool oxs_output_queue_push(oxs_output_queue_t *q, const oxs_output_event_t *ev)
{
    uint32_t w = atomic_load_explicit(&q->write_idx, memory_order_relaxed);
    uint32_t r = atomic_load_explicit(&q->read_idx, memory_order_acquire);
    uint32_t next = (w + 1) & (OXS_OUTPUT_QUEUE_SIZE - 1);

    if (next == r) {
        return false; /* full — drop event (GUI can miss meter updates) */
    }

    q->events[w] = *ev;
    atomic_store_explicit(&q->write_idx, next, memory_order_release);
    return true;
}

bool oxs_output_queue_pop(oxs_output_queue_t *q, oxs_output_event_t *out)
{
    uint32_t r = atomic_load_explicit(&q->read_idx, memory_order_relaxed);
    uint32_t w = atomic_load_explicit(&q->write_idx, memory_order_acquire);

    if (r == w) {
        return false; /* empty */
    }

    *out = q->events[r];
    atomic_store_explicit(&q->read_idx, (r + 1) & (OXS_OUTPUT_QUEUE_SIZE - 1),
                          memory_order_release);
    return true;
}
