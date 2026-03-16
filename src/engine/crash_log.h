/*
 * 0xSYNTH Crash Logging
 *
 * Installs signal handlers to write crash info to a log file
 * before the process terminates.
 */

#ifndef OXS_CRASH_LOG_H
#define OXS_CRASH_LOG_H

/* Install signal handlers (SIGSEGV, SIGABRT, SIGFPE).
 * Call once at startup. Writes crash log to:
 *   Windows: %APPDATA%/0xSYNTH/crash.log
 *   Linux:   ~/.local/share/0xSYNTH/crash.log */
void oxs_crash_log_init(void);

#endif /* OXS_CRASH_LOG_H */
