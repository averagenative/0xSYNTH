/*
 * log.h — Simple leveled logging with timestamps.
 *
 * Usage:
 *   #define LOG_TAG "engine"
 *   LOG_INFO("Audio started: %s", device_name);
 *   LOG_DEBUG("Step %d triggered", step);
 *   LOG_WARN("Buffer underrun detected");
 *   LOG_ERROR("Failed to init device");
 *
 * Set minimum level at compile time:  -DOXS_LOG_LEVEL=OXS_LOG_DEBUG
 * Or at runtime:                      oxs_log_set_level(OXS_LOG_WARN);
 *
 * Default level: OXS_LOG_INFO (shows info, warn, error).
 */

#ifndef OXS_LOG_H
#define OXS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <time.h>

typedef enum {
    OXS_LOG_DEBUG = 0,
    OXS_LOG_INFO  = 1,
    OXS_LOG_WARN  = 2,
    OXS_LOG_ERROR = 3,
    OXS_LOG_NONE  = 4
} oxs_log_level_t;

extern oxs_log_level_t g_oxs_log_level;
extern double          g_oxs_log_start_ms;
extern FILE           *g_oxs_log_file;

void oxs_log_init(void);
void oxs_log_set_level(oxs_log_level_t level);
void oxs_log_open_file(const char *path); /* also write to file */

#ifdef _WIN32
#include <windows.h>
static inline double oxs_log_elapsed_ms(void) {
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (now.QuadPart * 1000.0 / freq.QuadPart) - g_oxs_log_start_ms;
}
#else
static inline double oxs_log_elapsed_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6) - g_oxs_log_start_ms;
}
#endif

static inline const char *oxs_log_level_str(oxs_log_level_t lvl) {
    switch (lvl) {
        case OXS_LOG_DEBUG: return "DEBUG";
        case OXS_LOG_INFO:  return "INFO ";
        case OXS_LOG_WARN:  return "WARN ";
        case OXS_LOG_ERROR: return "ERROR";
        default:            return "?????";
    }
}

static inline void oxs_log_localtime(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (tm) strftime(buf, len, "%H:%M:%S", tm);
    else buf[0] = '\0';
}

#define OXS_LOG(level, tag, fmt, ...) do { \
    if ((level) >= g_oxs_log_level) { \
        char _oxs_timebuf[12]; \
        oxs_log_localtime(_oxs_timebuf, sizeof(_oxs_timebuf)); \
        fprintf(stderr, "%s [%9.1f ms] %s %s: " fmt "\n", \
                _oxs_timebuf, oxs_log_elapsed_ms(), oxs_log_level_str(level), \
                (tag), ##__VA_ARGS__); \
        fflush(stderr); \
        if (g_oxs_log_file) { \
            fprintf(g_oxs_log_file, "%s [%9.1f ms] %s %s: " fmt "\n", \
                    _oxs_timebuf, oxs_log_elapsed_ms(), oxs_log_level_str(level), \
                    (tag), ##__VA_ARGS__); \
            fflush(g_oxs_log_file); \
        } \
    } \
} while(0)

#ifndef LOG_TAG
#define LOG_TAG "app"
#endif

#define LOG_DEBUG(fmt, ...) OXS_LOG(OXS_LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  OXS_LOG(OXS_LOG_INFO,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  OXS_LOG(OXS_LOG_WARN,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) OXS_LOG(OXS_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* OXS_LOG_H */
