/*
 * 0xSYNTH Arpeggiator Implementation
 */

#include "arpeggiator.h"
#include <string.h>
#include <stdlib.h>

void oxs_arp_init(oxs_arpeggiator_t *arp)
{
    memset(arp, 0, sizeof(*arp));
    arp->direction = 1;
}

void oxs_arp_note_on(oxs_arpeggiator_t *arp, uint8_t note, uint8_t velocity)
{
    /* Don't add duplicates */
    for (int i = 0; i < arp->held_count; i++) {
        if (arp->held_notes[i] == note) return;
    }
    if (arp->held_count < OXS_ARP_MAX_NOTES) {
        arp->held_notes[arp->held_count] = note;
        arp->held_velocities[arp->held_count] = velocity;
        arp->held_count++;
    }
}

void oxs_arp_note_off(oxs_arpeggiator_t *arp, uint8_t note)
{
    for (int i = 0; i < arp->held_count; i++) {
        if (arp->held_notes[i] == note) {
            /* Shift remaining notes down */
            for (int j = i; j < arp->held_count - 1; j++) {
                arp->held_notes[j] = arp->held_notes[j + 1];
                arp->held_velocities[j] = arp->held_velocities[j + 1];
            }
            arp->held_count--;
            return;
        }
    }
}

void oxs_arp_all_off(oxs_arpeggiator_t *arp)
{
    arp->held_count = 0;
    arp->play_count = 0;
    arp->current_index = 0;
    arp->note_is_on = false;
}

/* Sort helper for qsort */
static int note_compare(const void *a, const void *b)
{
    return (int)(*(const uint8_t *)a) - (int)(*(const uint8_t *)b);
}

void oxs_arp_rebuild(oxs_arpeggiator_t *arp, int mode, int octaves)
{
    if (arp->held_count == 0) {
        arp->play_count = 0;
        return;
    }

    if (octaves < 1) octaves = 1;
    if (octaves > 4) octaves = 4;

    /* Build sorted base note list */
    uint8_t sorted[OXS_ARP_MAX_NOTES];
    int count = arp->held_count;
    memcpy(sorted, arp->held_notes, (size_t)count);

    if (mode != OXS_ARP_AS_PLAYED) {
        qsort(sorted, (size_t)count, sizeof(uint8_t), note_compare);
    }

    /* Expand across octaves */
    arp->play_count = 0;
    for (int oct = 0; oct < octaves; oct++) {
        for (int i = 0; i < count && arp->play_count < OXS_ARP_MAX_NOTES * 4; i++) {
            int note = sorted[i] + oct * 12;
            if (note <= 127) {
                arp->play_notes[arp->play_count++] = (uint8_t)note;
            }
        }
    }

    /* For down mode, reverse the list */
    if (mode == OXS_ARP_DOWN) {
        for (int i = 0; i < arp->play_count / 2; i++) {
            uint8_t tmp = arp->play_notes[i];
            arp->play_notes[i] = arp->play_notes[arp->play_count - 1 - i];
            arp->play_notes[arp->play_count - 1 - i] = tmp;
        }
    }

    /* Clamp current index */
    if (arp->current_index >= arp->play_count)
        arp->current_index = 0;
}

void oxs_arp_process(oxs_arpeggiator_t *arp, uint32_t num_frames,
                     uint32_t sample_rate, float bpm, int rate_div,
                     float gate, int mode, int octaves,
                     oxs_arp_note_cb callback, void *ctx)
{
    if (arp->held_count == 0) {
        /* No held notes — silence any playing note */
        if (arp->note_is_on) {
            callback(ctx, arp->current_note, 0, false);
            arp->note_is_on = false;
        }
        arp->sample_counter = 0;
        arp->current_index = 0;
        return;
    }

    /* Rebuild play list if notes changed */
    oxs_arp_rebuild(arp, mode, octaves);
    if (arp->play_count == 0) return;

    /* Calculate step length in samples */
    /* rate_div: 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, 5=1/32 */
    static const float div_mult[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f };
    if (rate_div < 0) rate_div = 0;
    if (rate_div > 5) rate_div = 5;
    float beats_per_step = div_mult[rate_div];
    float seconds_per_step = beats_per_step * 60.0f / bpm;
    double step_samples = (double)seconds_per_step * (double)sample_rate;
    double gate_samples = step_samples * (double)gate;

    /* Process the buffer */
    for (uint32_t i = 0; i < num_frames; i++) {
        arp->sample_counter++;
        arp->gate_counter++;

        /* Gate off: turn off note when gate period expires */
        if (arp->note_is_on && arp->gate_counter >= gate_samples) {
            callback(ctx, arp->current_note, 0, false);
            arp->note_is_on = false;
        }

        /* Step trigger: advance to next note */
        if (arp->sample_counter >= step_samples) {
            arp->sample_counter -= step_samples;
            arp->gate_counter = 0;

            /* Turn off previous note if still on */
            if (arp->note_is_on) {
                callback(ctx, arp->current_note, 0, false);
                arp->note_is_on = false;
            }

            /* Advance index based on mode */
            if (mode == OXS_ARP_UPDOWN) {
                arp->current_index += arp->direction;
                if (arp->current_index >= arp->play_count - 1) {
                    arp->current_index = arp->play_count - 1;
                    arp->direction = -1;
                }
                if (arp->current_index <= 0) {
                    arp->current_index = 0;
                    arp->direction = 1;
                }
            } else if (mode == OXS_ARP_RANDOM) {
                arp->current_index = rand() % arp->play_count;
            } else {
                /* Up, Down, As-Played — sequential */
                arp->current_index++;
                if (arp->current_index >= arp->play_count)
                    arp->current_index = 0;
            }

            /* Trigger the note */
            arp->current_note = arp->play_notes[arp->current_index];
            callback(ctx, arp->current_note, 100, true);
            arp->note_is_on = true;
        }
    }
}
