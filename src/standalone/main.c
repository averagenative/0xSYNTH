/*
 * 0xSYNTH Standalone Application
 *
 * Headless synth with audio output and MIDI input.
 * GTK GUI will be integrated in Phase 8.
 *
 * Usage:
 *   0xsynth                        # start with defaults
 *   0xsynth --preset path.json     # load a preset
 *   0xsynth --list-audio           # list audio devices
 *   0xsynth --list-midi            # list MIDI devices
 *   0xsynth --sample-rate 48000    # set sample rate
 *   0xsynth --buffer-size 512      # set buffer size
 */

#include "../api/synth_api.h"
#include "audio.h"
#include "midi.h"

#ifdef OXS_HAS_IMGUI
#include "../gui_imgui/imgui_app.h"
#endif
#ifdef OXS_HAS_GTK
#include "../gui_gtk/gtk_app.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef OXS_PLATFORM_WINDOWS
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

static volatile bool g_running = true;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

static void print_usage(const char *prog)
{
    printf("0xSYNTH — Multi-engine synthesizer\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --list-audio          List audio output devices\n");
    printf("  --list-midi           List MIDI input devices\n");
    printf("  --sample-rate <hz>    Set sample rate (default: 44100)\n");
    printf("  --buffer-size <n>     Set buffer size in frames (default: 256)\n");
    printf("  --preset <path>       Load a preset on startup\n");
    printf("  --help                Show this help\n");
    printf("\n");
    printf("Controls:\n");
    printf("  Connect a MIDI keyboard to play notes.\n");
    printf("  Press Ctrl+C to quit.\n");
}

int main(int argc, char *argv[])
{
    uint32_t sample_rate = 44100;
    uint32_t buffer_size = 256;
    const char *preset_path = NULL;
    bool headless = false;

    /* Parse CLI args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list-audio") == 0) {
            oxs_audio_list_devices();
            return 0;
        }
        if (strcmp(argv[i], "--list-midi") == 0) {
            oxs_midi_list_devices();
            return 0;
        }
        if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc) {
            sample_rate = (uint32_t)atoi(argv[++i]);
        }
        if (strcmp(argv[i], "--buffer-size") == 0 && i + 1 < argc) {
            buffer_size = (uint32_t)atoi(argv[++i]);
        }
        if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            preset_path = argv[++i];
        }
        if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* Create synth */
    printf("0xSYNTH v0.1.0\n");
    oxs_synth_t *synth = oxs_synth_create(sample_rate);
    if (!synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    /* Load state: CLI preset > saved session > default preset */
    if (preset_path) {
        if (oxs_synth_preset_load(synth, preset_path)) {
            printf("Preset: loaded %s\n", preset_path);
        } else {
            fprintf(stderr, "Warning: failed to load preset %s\n", preset_path);
        }
    } else if (oxs_synth_session_load(synth)) {
        printf("Session: restored previous state\n");
    } else {
        oxs_synth_load_default_preset(synth);
        printf("Preset: loaded default\n");
    }

    /* Create audio backend */
    oxs_audio_t *audio = oxs_audio_create(synth, sample_rate, buffer_size);
    if (!audio) {
        fprintf(stderr, "Failed to create audio backend\n");
        oxs_synth_destroy(synth);
        return 1;
    }

    /* Create MIDI input (optional — may fail if no device) */
    oxs_midi_t *midi = oxs_midi_create(synth);

    /* Start audio */
    if (!oxs_audio_start(audio)) {
        fprintf(stderr, "Failed to start audio\n");
        oxs_midi_destroy(midi);
        oxs_audio_destroy(audio);
        oxs_synth_destroy(synth);
        return 1;
    }

    /* Install signal handler for clean shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);


#ifdef OXS_HAS_IMGUI
    if (!headless) {
        printf("Launching ImGui GUI...\n");
        oxs_imgui_run(synth, argc, argv);
        g_running = false;
    } else
#elif defined(OXS_HAS_GTK)
    if (!headless) {
        printf("Launching GTK GUI...\n");
        oxs_gtk_run(synth, argc, argv);
        g_running = false;
    } else
#endif
    {
        (void)headless;
        printf("Running headless. Press Ctrl+C to quit.\n");

        while (g_running) {
            oxs_output_event_t ev;
            while (oxs_synth_pop_output_event(synth, &ev)) {
                (void)ev;
            }
            sleep_ms(50);
        }
    }

    printf("\nShutting down...\n");

    /* Save session state for next launch */
    if (oxs_synth_session_save(synth)) {
        printf("Session: saved\n");
    }

    /* Clean shutdown */
    oxs_audio_stop(audio);
    oxs_midi_destroy(midi);
    oxs_audio_destroy(audio);
    oxs_synth_destroy(synth);

    printf("Done.\n");
    return 0;
}
