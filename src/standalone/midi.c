/*
 * 0xSYNTH Standalone MIDI Input Implementation
 *
 * Currently supports ALSA raw MIDI on Linux.
 * macOS (CoreMIDI) and Windows (WinMM) stubs for future implementation.
 */

#include "midi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef OXS_PLATFORM_LINUX

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

struct oxs_midi {
    oxs_synth_t *synth;
    int           fd;
    pthread_t     thread;
    volatile bool running;
};

static void *midi_thread_func(void *arg)
{
    oxs_midi_t *midi = (oxs_midi_t *)arg;
    uint8_t buf[3];
    int pos = 0;
    int expected = 0;

    while (midi->running) {
        uint8_t byte;
        ssize_t n = read(midi->fd, &byte, 1);
        if (n <= 0) {
            usleep(1000); /* 1ms poll */
            continue;
        }

        /* Parse MIDI byte stream */
        if (byte & 0x80) {
            /* Status byte */
            buf[0] = byte;
            pos = 1;
            uint8_t status = byte & 0xF0;
            if (status == 0x80 || status == 0x90 || status == 0xB0)
                expected = 3;
            else
                expected = 0; /* ignore other message types for now */
        } else if (pos > 0 && pos < expected) {
            buf[pos++] = byte;
        }

        if (pos == expected && expected > 0) {
            uint8_t status  = buf[0] & 0xF0;
            uint8_t channel = buf[0] & 0x0F;
            uint8_t data1   = buf[1];
            uint8_t data2   = buf[2];

            if (status == 0x90 && data2 > 0) {
                oxs_synth_note_on(midi->synth, data1, data2, channel);
            } else if (status == 0x80 || (status == 0x90 && data2 == 0)) {
                oxs_synth_note_off(midi->synth, data1, channel);
            } else if (status == 0xB0) {
                oxs_synth_midi_cc(midi->synth, data1, data2);
            }

            pos = 0;
            expected = 0;
        }
    }

    return NULL;
}

oxs_midi_t *oxs_midi_create(oxs_synth_t *synth)
{
    /* Try to open the first available MIDI input */
    const char *devices[] = {
        "/dev/snd/midiC0D0",
        "/dev/snd/midiC1D0",
        "/dev/midi0",
        "/dev/midi1",
        NULL
    };

    int fd = -1;
    const char *opened = NULL;
    for (int i = 0; devices[i]; i++) {
        fd = open(devices[i], O_RDONLY | O_NONBLOCK);
        if (fd >= 0) { opened = devices[i]; break; }
    }

    if (fd < 0) {
        printf("MIDI: no device found (standalone will use virtual keyboard only)\n");
        return NULL;
    }

    oxs_midi_t *midi = calloc(1, sizeof(oxs_midi_t));
    if (!midi) { close(fd); return NULL; }

    midi->synth = synth;
    midi->fd = fd;
    midi->running = true;

    printf("MIDI: opened %s\n", opened);

    if (pthread_create(&midi->thread, NULL, midi_thread_func, midi) != 0) {
        fprintf(stderr, "Failed to create MIDI thread\n");
        close(fd);
        free(midi);
        return NULL;
    }

    return midi;
}

void oxs_midi_destroy(oxs_midi_t *midi)
{
    if (!midi) return;
    midi->running = false;
    pthread_join(midi->thread, NULL);
    close(midi->fd);
    free(midi);
}

void oxs_midi_list_devices(void)
{
    printf("MIDI devices:\n");
    DIR *dir = opendir("/dev/snd");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "midi", 4) == 0) {
                printf("  /dev/snd/%s\n", entry->d_name);
            }
        }
        closedir(dir);
    }

    /* Also check legacy devices */
    for (int i = 0; i < 4; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/midi%d", i);
        if (access(path, R_OK) == 0) {
            printf("  %s\n", path);
        }
    }
}

#else /* Non-Linux platforms: stubs */

struct oxs_midi {
    oxs_synth_t *synth;
};

oxs_midi_t *oxs_midi_create(oxs_synth_t *synth)
{
    (void)synth;
    printf("MIDI: not yet implemented on this platform\n");
    return NULL;
}

void oxs_midi_destroy(oxs_midi_t *midi) { free(midi); }

void oxs_midi_list_devices(void)
{
    printf("MIDI device listing not yet implemented on this platform\n");
}

#endif /* OXS_PLATFORM_LINUX */
