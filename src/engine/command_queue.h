/*
 * 0xSYNTH Lock-Free SPSC Command Queue
 *
 * GUI thread pushes commands, audio thread pops them.
 * Adapted from 0x808's sq_command_queue_t.
 */

#ifndef OXS_COMMAND_QUEUE_H
#define OXS_COMMAND_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

/* Queue size must be power of 2 */
#define OXS_CMD_QUEUE_SIZE 256

typedef enum {
    OXS_CMD_NONE = 0,
    OXS_CMD_NOTE_ON,
    OXS_CMD_NOTE_OFF,
    OXS_CMD_LOAD_PRESET,    /* load preset from path (queued, not real-time) */
    OXS_CMD_SET_SYNTH_MODE,
    OXS_CMD_PANIC,           /* all notes off */
    OXS_CMD_MIDI_CC,         /* MIDI CC message */
} oxs_cmd_type_t;

typedef struct {
    oxs_cmd_type_t type;
    union {
        struct { uint8_t note; uint8_t velocity; uint8_t channel; } note;
        struct { int32_t mode; } synth_mode;
        struct { uint8_t cc; uint8_t value; } midi_cc;
        int32_t int_val;
    } data;
} oxs_command_t;

typedef struct {
    oxs_command_t commands[OXS_CMD_QUEUE_SIZE];
    _Atomic uint32_t write_idx;
    _Atomic uint32_t read_idx;
} oxs_cmd_queue_t;

void oxs_cmd_queue_init(oxs_cmd_queue_t *q);
bool oxs_cmd_queue_push(oxs_cmd_queue_t *q, oxs_command_t cmd);
bool oxs_cmd_queue_pop(oxs_cmd_queue_t *q, oxs_command_t *out);

/* Convenience: inline command builders */
static inline oxs_command_t oxs_cmd_note_on(uint8_t note, uint8_t velocity, uint8_t channel)
{
    oxs_command_t c = { .type = OXS_CMD_NOTE_ON };
    c.data.note.note = note;
    c.data.note.velocity = velocity;
    c.data.note.channel = channel;
    return c;
}

static inline oxs_command_t oxs_cmd_note_off(uint8_t note, uint8_t channel)
{
    oxs_command_t c = { .type = OXS_CMD_NOTE_OFF };
    c.data.note.note = note;
    c.data.note.velocity = 0;
    c.data.note.channel = channel;
    return c;
}

static inline oxs_command_t oxs_cmd_panic(void)
{
    return (oxs_command_t){ .type = OXS_CMD_PANIC };
}

static inline oxs_command_t oxs_cmd_midi_cc(uint8_t cc, uint8_t value)
{
    oxs_command_t c = { .type = OXS_CMD_MIDI_CC };
    c.data.midi_cc.cc = cc;
    c.data.midi_cc.value = value;
    return c;
}

#endif /* OXS_COMMAND_QUEUE_H */
