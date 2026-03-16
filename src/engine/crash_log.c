/*
 * 0xSYNTH Crash Logging Implementation
 */

#include "crash_log.h"
#include "preset.h" /* for oxs_preset_user_dir */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <string.h>

static char g_crash_log_path[512] = "";

static void crash_signal_handler(int sig)
{
    const char *sig_name = "UNKNOWN";
    switch (sig) {
    case SIGSEGV: sig_name = "SIGSEGV (Segmentation fault)"; break;
    case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
    case SIGFPE:  sig_name = "SIGFPE (Floating point exception)"; break;
#ifdef SIGBUS
    case SIGBUS:  sig_name = "SIGBUS (Bus error)"; break;
#endif
    }

    FILE *f = fopen(g_crash_log_path, "a");
    if (f) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

        fprintf(f, "\n=== CRASH ===\n");
        fprintf(f, "Time: %s\n", timebuf);
        fprintf(f, "Signal: %s (%d)\n", sig_name, sig);
        fprintf(f, "Version: 0xSYNTH v0.1.0\n");
        fprintf(f, "=============\n");
        fclose(f);
    }

    /* Re-raise to get default behavior (core dump / termination) */
    signal(sig, SIG_DFL);
    raise(sig);
}

void oxs_crash_log_init(void)
{
    /* Build log path next to user presets */
    const char *user_dir = oxs_preset_user_dir();
    if (user_dir && user_dir[0]) {
        snprintf(g_crash_log_path, sizeof(g_crash_log_path),
                 "%s/../crash.log", user_dir);
    } else {
        snprintf(g_crash_log_path, sizeof(g_crash_log_path), "crash.log");
    }

    signal(SIGSEGV, crash_signal_handler);
    signal(SIGABRT, crash_signal_handler);
    signal(SIGFPE,  crash_signal_handler);
#ifdef SIGBUS
    signal(SIGBUS,  crash_signal_handler);
#endif
}
