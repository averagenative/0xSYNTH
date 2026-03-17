/*
 * 0xSYNTH Standalone MIDI Input Implementation
 *
 * Linux: ALSA raw MIDI with device enumeration
 * Windows: WinMM API with callback-based input
 * macOS: CoreMIDI (stub for future implementation)
 */

#include "midi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Windows — WinMM MIDI Input
 * ========================================================================= */
#ifdef OXS_PLATFORM_WINDOWS

#include <windows.h>
#include <mmsystem.h>

struct oxs_midi {
    oxs_synth_t *synth;
    HMIDIIN      handle;
    int          device_index;
};

/* Max devices we cache for enumeration */
#define MAX_MIDI_DEVICES 32
static char g_midi_device_names[MAX_MIDI_DEVICES][MAXPNAMELEN];
static int  g_midi_device_count_cached = -1;

static void midi_enumerate(void)
{
    if (g_midi_device_count_cached >= 0) return;
    UINT num = midiInGetNumDevs();
    if (num > MAX_MIDI_DEVICES) num = MAX_MIDI_DEVICES;
    g_midi_device_count_cached = (int)num;
    for (UINT i = 0; i < num; i++) {
        MIDIINCAPSA caps;
        if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            strncpy(g_midi_device_names[i], caps.szPname, MAXPNAMELEN - 1);
            g_midi_device_names[i][MAXPNAMELEN - 1] = '\0';
        } else {
            snprintf(g_midi_device_names[i], MAXPNAMELEN, "MIDI Device %u", i);
        }
    }
}

/* WinMM MIDI callback — called from a system thread */
static void CALLBACK midi_in_callback(HMIDIIN hMidiIn, UINT wMsg,
                                       DWORD_PTR dwInstance,
                                       DWORD_PTR dwParam1,
                                       DWORD_PTR dwParam2)
{
    (void)hMidiIn;
    (void)dwParam2;

    if (wMsg != MIM_DATA) return;

    oxs_midi_t *midi = (oxs_midi_t *)dwInstance;
    if (!midi || !midi->synth) return;

    uint8_t status  = (uint8_t)(dwParam1 & 0xFF);
    uint8_t data1   = (uint8_t)((dwParam1 >> 8) & 0xFF);
    uint8_t data2   = (uint8_t)((dwParam1 >> 16) & 0xFF);
    uint8_t channel = status & 0x0F;
    uint8_t msg     = status & 0xF0;

    switch (msg) {
    case 0x90: /* Note On */
        if (data2 > 0)
            oxs_synth_note_on(midi->synth, data1, data2, channel);
        else
            oxs_synth_note_off(midi->synth, data1, channel);
        break;
    case 0x80: /* Note Off */
        oxs_synth_note_off(midi->synth, data1, channel);
        break;
    case 0xB0: /* Control Change */
        oxs_synth_midi_cc(midi->synth, data1, data2);
        break;
    case 0xE0: /* Pitch Bend */
        {
            int16_t bend = (int16_t)(((uint16_t)data2 << 7) | (uint16_t)data1) - 8192;
            oxs_synth_pitch_bend(midi->synth, bend, channel);
        }
        break;
    case 0xD0: /* Channel Aftertouch */
        oxs_synth_channel_pressure(midi->synth, data1, channel);
        break;
    }
}

static oxs_midi_t *midi_open_device(oxs_synth_t *synth, int device_index)
{
    midi_enumerate();

    UINT num = midiInGetNumDevs();
    if (num == 0) {
        printf("MIDI: no devices found (standalone will use virtual keyboard only)\n");
        return NULL;
    }

    if (device_index < 0 || (UINT)device_index >= num) {
        printf("MIDI: invalid device index %d (have %u devices)\n", device_index, num);
        return NULL;
    }

    oxs_midi_t *midi = calloc(1, sizeof(oxs_midi_t));
    if (!midi) return NULL;

    midi->synth = synth;
    midi->device_index = device_index;

    MMRESULT result = midiInOpen(&midi->handle, (UINT)device_index,
                                  (DWORD_PTR)midi_in_callback,
                                  (DWORD_PTR)midi,
                                  CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        fprintf(stderr, "MIDI: failed to open device %d (error %u)\n",
                device_index, (unsigned)result);
        free(midi);
        return NULL;
    }

    result = midiInStart(midi->handle);
    if (result != MMSYSERR_NOERROR) {
        fprintf(stderr, "MIDI: failed to start input (error %u)\n", (unsigned)result);
        midiInClose(midi->handle);
        free(midi);
        return NULL;
    }

    printf("MIDI: opened device %d: %s\n", device_index,
           g_midi_device_names[device_index]);
    return midi;
}

oxs_midi_t *oxs_midi_create(oxs_synth_t *synth)
{
    return midi_open_device(synth, 0);
}

oxs_midi_t *oxs_midi_create_device(oxs_synth_t *synth, int device_index)
{
    return midi_open_device(synth, device_index);
}

void oxs_midi_destroy(oxs_midi_t *midi)
{
    if (!midi) return;
    midiInStop(midi->handle);
    midiInClose(midi->handle);
    free(midi);
}

void oxs_midi_list_devices(void)
{
    midi_enumerate();
    printf("MIDI input devices:\n");
    for (int i = 0; i < g_midi_device_count_cached; i++) {
        printf("  [%d] %s\n", i, g_midi_device_names[i]);
    }
    if (g_midi_device_count_cached == 0) {
        printf("  (none)\n");
    }
}

int oxs_midi_get_device_count(void)
{
    midi_enumerate();
    return g_midi_device_count_cached;
}

const char *oxs_midi_get_device_name(int index)
{
    midi_enumerate();
    if (index < 0 || index >= g_midi_device_count_cached) return NULL;
    return g_midi_device_names[index];
}

/* =========================================================================
 * Linux — ALSA Raw MIDI Input
 * ========================================================================= */
#elif defined(OXS_PLATFORM_LINUX)

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

struct oxs_midi {
    oxs_synth_t *synth;
    int           fd;
    int           device_index;
    pthread_t     thread;
    volatile bool running;
};

/* Device enumeration */
#define MAX_MIDI_DEVICES 32
static char g_midi_device_paths[MAX_MIDI_DEVICES][280];
static char g_midi_device_names[MAX_MIDI_DEVICES][280];
static int  g_midi_device_count_cached = -1;

static void midi_enumerate(void)
{
    if (g_midi_device_count_cached >= 0) return;
    g_midi_device_count_cached = 0;

    /* Scan /dev/snd/midi* */
    DIR *dir = opendir("/dev/snd");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "midi", 4) == 0 &&
                g_midi_device_count_cached < MAX_MIDI_DEVICES) {
                int idx = g_midi_device_count_cached;
                snprintf(g_midi_device_paths[idx], sizeof(g_midi_device_paths[idx]),
                         "/dev/snd/%s", entry->d_name);
                snprintf(g_midi_device_names[idx], sizeof(g_midi_device_names[idx]),
                         "ALSA: %s", entry->d_name);
                g_midi_device_count_cached++;
            }
        }
        closedir(dir);
    }

    /* Also check legacy /dev/midi* */
    for (int i = 0; i < 4 && g_midi_device_count_cached < MAX_MIDI_DEVICES; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/midi%d", i);
        if (access(path, R_OK) == 0) {
            int idx = g_midi_device_count_cached;
            strncpy(g_midi_device_paths[idx], path, sizeof(g_midi_device_paths[idx]) - 1);
            snprintf(g_midi_device_names[idx], sizeof(g_midi_device_names[idx]),
                     "OSS: midi%d", i);
            g_midi_device_count_cached++;
        }
    }
}

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
            else if (status == 0xE0) /* Pitch bend */
                expected = 3;
            else if (status == 0xD0) /* Channel aftertouch */
                expected = 2;
            else
                expected = 0; /* ignore other message types */
        } else if (pos > 0 && pos < expected) {
            buf[pos++] = byte;
        }

        if (pos == expected && expected > 0) {
            uint8_t status  = buf[0] & 0xF0;
            uint8_t channel = buf[0] & 0x0F;
            uint8_t data1   = buf[1];
            uint8_t data2   = (expected >= 3) ? buf[2] : 0;

            switch (status) {
            case 0x90:
                if (data2 > 0)
                    oxs_synth_note_on(midi->synth, data1, data2, channel);
                else
                    oxs_synth_note_off(midi->synth, data1, channel);
                break;
            case 0x80:
                oxs_synth_note_off(midi->synth, data1, channel);
                break;
            case 0xB0:
                oxs_synth_midi_cc(midi->synth, data1, data2);
                break;
            case 0xE0:
                {
                    int16_t bend = (int16_t)(((uint16_t)data2 << 7) | (uint16_t)data1) - 8192;
                    oxs_synth_pitch_bend(midi->synth, bend, channel);
                }
                break;
            case 0xD0:
                oxs_synth_channel_pressure(midi->synth, data1, channel);
                break;
            }

            pos = 0;
            expected = 0;
        }
    }

    return NULL;
}

static oxs_midi_t *midi_open_device(oxs_synth_t *synth, int device_index)
{
    midi_enumerate();

    int fd = -1;
    const char *opened = NULL;

    if (device_index >= 0 && device_index < g_midi_device_count_cached) {
        fd = open(g_midi_device_paths[device_index], O_RDONLY | O_NONBLOCK);
        if (fd >= 0) opened = g_midi_device_paths[device_index];
    } else if (device_index < 0) {
        /* Auto-detect: try all enumerated devices */
        for (int i = 0; i < g_midi_device_count_cached; i++) {
            fd = open(g_midi_device_paths[i], O_RDONLY | O_NONBLOCK);
            if (fd >= 0) { opened = g_midi_device_paths[i]; device_index = i; break; }
        }
        /* Also try hardcoded paths if enumeration found nothing */
        if (fd < 0) {
            const char *fallbacks[] = {
                "/dev/snd/midiC0D0", "/dev/snd/midiC1D0",
                "/dev/midi0", "/dev/midi1", NULL
            };
            for (int i = 0; fallbacks[i]; i++) {
                fd = open(fallbacks[i], O_RDONLY | O_NONBLOCK);
                if (fd >= 0) { opened = fallbacks[i]; break; }
            }
        }
    }

    if (fd < 0) {
        printf("MIDI: no device found (standalone will use virtual keyboard only)\n");
        return NULL;
    }

    oxs_midi_t *midi = calloc(1, sizeof(oxs_midi_t));
    if (!midi) { close(fd); return NULL; }

    midi->synth = synth;
    midi->fd = fd;
    midi->device_index = device_index;
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

oxs_midi_t *oxs_midi_create(oxs_synth_t *synth)
{
    return midi_open_device(synth, -1); /* auto-detect */
}

oxs_midi_t *oxs_midi_create_device(oxs_synth_t *synth, int device_index)
{
    return midi_open_device(synth, device_index);
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
    midi_enumerate();
    printf("MIDI input devices:\n");
    for (int i = 0; i < g_midi_device_count_cached; i++) {
        printf("  [%d] %s (%s)\n", i, g_midi_device_names[i],
               g_midi_device_paths[i]);
    }
    if (g_midi_device_count_cached == 0) {
        printf("  (none found — check /dev/snd/midi*)\n");
    }
}

int oxs_midi_get_device_count(void)
{
    midi_enumerate();
    return g_midi_device_count_cached;
}

const char *oxs_midi_get_device_name(int index)
{
    midi_enumerate();
    if (index < 0 || index >= g_midi_device_count_cached) return NULL;
    return g_midi_device_names[index];
}

/* =========================================================================
 * Other platforms — stubs
 * ========================================================================= */
#else

struct oxs_midi {
    oxs_synth_t *synth;
};

oxs_midi_t *oxs_midi_create(oxs_synth_t *synth)
{
    (void)synth;
    printf("MIDI: not yet implemented on this platform\n");
    return NULL;
}

oxs_midi_t *oxs_midi_create_device(oxs_synth_t *synth, int device_index)
{
    (void)synth;
    (void)device_index;
    printf("MIDI: not yet implemented on this platform\n");
    return NULL;
}

void oxs_midi_destroy(oxs_midi_t *midi) { free(midi); }

void oxs_midi_list_devices(void)
{
    printf("MIDI device listing not yet implemented on this platform\n");
}

int oxs_midi_get_device_count(void) { return 0; }
const char *oxs_midi_get_device_name(int index) { (void)index; return NULL; }

#endif
