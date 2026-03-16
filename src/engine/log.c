/*
 * log.c — Logging implementation
 */

#include "log.h"

oxs_log_level_t g_oxs_log_level = OXS_LOG_INFO;
double          g_oxs_log_start_ms = 0.0;
FILE           *g_oxs_log_file = NULL;

void oxs_log_init(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    g_oxs_log_start_ms = now.QuadPart * 1000.0 / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_oxs_log_start_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
#endif
}

void oxs_log_set_level(oxs_log_level_t level)
{
    g_oxs_log_level = level;
}

void oxs_log_open_file(const char *path)
{
    if (g_oxs_log_file) fclose(g_oxs_log_file);
    g_oxs_log_file = fopen(path, "a");
}
