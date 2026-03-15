/*
 * 0xSYNTH Lock-Free SPSC Command Queue Implementation
 * Ported from 0x808's command_queue.c
 */

#include "command_queue.h"
#include <string.h>

void oxs_cmd_queue_init(oxs_cmd_queue_t *q)
{
    memset(q->commands, 0, sizeof(q->commands));
    atomic_store_explicit(&q->write_idx, 0, memory_order_relaxed);
    atomic_store_explicit(&q->read_idx, 0, memory_order_relaxed);
}

bool oxs_cmd_queue_push(oxs_cmd_queue_t *q, oxs_command_t cmd)
{
    uint32_t w = atomic_load_explicit(&q->write_idx, memory_order_relaxed);
    uint32_t r = atomic_load_explicit(&q->read_idx, memory_order_acquire);
    uint32_t next = (w + 1) & (OXS_CMD_QUEUE_SIZE - 1);

    if (next == r) {
        return false; /* queue full */
    }

    q->commands[w] = cmd;
    atomic_store_explicit(&q->write_idx, next, memory_order_release);
    return true;
}

bool oxs_cmd_queue_pop(oxs_cmd_queue_t *q, oxs_command_t *out)
{
    uint32_t r = atomic_load_explicit(&q->read_idx, memory_order_relaxed);
    uint32_t w = atomic_load_explicit(&q->write_idx, memory_order_acquire);

    if (r == w) {
        return false; /* queue empty */
    }

    *out = q->commands[r];
    atomic_store_explicit(&q->read_idx, (r + 1) & (OXS_CMD_QUEUE_SIZE - 1),
                          memory_order_release);
    return true;
}
