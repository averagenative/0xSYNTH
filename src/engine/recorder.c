/*
 * 0xSYNTH Streaming WAV Recorder
 *
 * Writes audio directly to disk during recording via dr_wav streaming API.
 * No large in-memory buffer — audio is flushed to the OS page cache each
 * callback (~256-512 frames = 1-2 KB per write).
 *
 * Ported from 0x808 drum machine.
 */

#define LOG_TAG "recorder"
#include "log.h"
#include "recorder.h"
#include "dr_wav.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <shlobj.h>
#define oxs_mkdir(p) _mkdir(p)
#else
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#define oxs_mkdir(p) mkdir(p, 0755)
#endif

/* Helper: create directory and all parents */
static void ensure_directory(const char *dir)
{
    if (!dir || !dir[0]) return;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        tmp[--len] = '\0';
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char c = tmp[i];
            tmp[i] = '\0';
            oxs_mkdir(tmp);
            tmp[i] = c;
        }
    }
    oxs_mkdir(tmp);
}

/* Helper: extract directory from a filepath */
static void extract_dir(const char *filepath, char *dir, size_t dir_size)
{
    snprintf(dir, dir_size, "%s", filepath);
    char *sep = strrchr(dir, '/');
#ifdef _WIN32
    char *sep2 = strrchr(dir, '\\');
    if (sep2 > sep) sep = sep2;
#endif
    if (sep) *(sep + 1) = '\0';
    else snprintf(dir, dir_size, ".");
}

/* ─── Start recording ────────────────────────────────────────────────────── */

int oxs_recorder_start(oxs_recorder_t *rec, const char *filepath,
                       uint32_t sample_rate, uint32_t bit_depth)
{
    if (!rec || !filepath) return -1;

    if (atomic_load(&rec->state) == OXS_REC_ACTIVE)
        oxs_recorder_stop(rec);

    if (bit_depth != 16 && bit_depth != 24 && bit_depth != 32)
        bit_depth = 16;

    char dir[512];
    extract_dir(filepath, dir, sizeof(dir));
    ensure_directory(dir);

    drwav *wav = (drwav *)calloc(1, sizeof(drwav));
    if (!wav) {
        LOG_ERROR("Failed to allocate drwav struct");
        return -1;
    }

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.channels = 2;
    format.sampleRate = sample_rate;
    if (bit_depth == 32) {
        format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        format.bitsPerSample = 32;
    } else {
        format.format = DR_WAVE_FORMAT_PCM;
        format.bitsPerSample = (drwav_uint32)bit_depth;
    }

    if (!drwav_init_file_write(wav, filepath, &format, NULL)) {
        LOG_ERROR("Failed to open %s for streaming WAV write", filepath);
        free(wav);
        return -1;
    }

    rec->wav = wav;
    atomic_store_explicit(&rec->state, OXS_REC_ACTIVE, memory_order_release);
    rec->bit_depth = bit_depth;
    rec->sample_rate = sample_rate;
    rec->frames_written = 0;
    rec->disk_low = false;
    rec->disk_check_countdown = 0;
    snprintf(rec->filepath, sizeof(rec->filepath), "%s", filepath);

    LOG_INFO("Recording started: %s (%u-bit, %u Hz)",
             filepath, bit_depth, sample_rate);
    return 0;
}

/* Helper: handle write failure */
static void rec_handle_error(oxs_recorder_t *rec, drwav_uint64 partial)
{
    atomic_store(&rec->state, OXS_REC_ERROR);
    rec->frames_written += (uint64_t)partial;
    LOG_ERROR("Recording write failed at frame %llu",
              (unsigned long long)rec->frames_written);
    drwav *wav = (drwav *)rec->wav;
    if (wav) {
        drwav_uninit(wav);
        free(wav);
        rec->wav = NULL;
    }
}

/* ─── Write audio frames ─────────────────────────────────────────────────── */

void oxs_recorder_write(oxs_recorder_t *rec, const float *output,
                        uint32_t num_frames)
{
    if (!rec || atomic_load_explicit(&rec->state, memory_order_acquire) != OXS_REC_ACTIVE
        || !rec->wav || !output)
        return;

    drwav *wav = (drwav *)rec->wav;

    if (rec->bit_depth == 32) {
        drwav_uint64 written = drwav_write_pcm_frames(wav, num_frames, output);
        if (written < num_frames) {
            rec_handle_error(rec, written);
            return;
        }
        rec->frames_written += num_frames;
    } else if (rec->bit_depth == 16) {
        int16_t pcm16[2048]; /* 1024 stereo frames */
        uint32_t remaining = num_frames;
        const float *src = output;
        while (remaining > 0) {
            uint32_t chunk = remaining > 1024 ? 1024 : remaining;
            for (uint32_t i = 0; i < chunk * 2; i++) {
                float v = src[i];
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                pcm16[i] = (int16_t)(v * 32767.0f);
            }
            drwav_uint64 w = drwav_write_pcm_frames(wav, chunk, pcm16);
            if (w < chunk) {
                rec_handle_error(rec, w);
                return;
            }
            rec->frames_written += chunk;
            src += chunk * 2;
            remaining -= chunk;
        }
    } else {
        /* 24-bit: convert float → packed 3-byte little-endian samples.
         * drwav_write_pcm_frames expects raw bytes matching the format. */
        uint8_t pcm24[1024 * 2 * 3]; /* 1024 stereo frames × 3 bytes each */
        uint32_t remaining = num_frames;
        const float *src = output;
        while (remaining > 0) {
            uint32_t chunk = remaining > 1024 ? 1024 : remaining;
            uint32_t total_samples = chunk * 2;
            for (uint32_t i = 0; i < total_samples; i++) {
                float v = src[i];
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                int32_t s = (int32_t)(v * 8388607.0f);
                uint32_t u = (uint32_t)s;
                pcm24[i * 3]     = (uint8_t)(u & 0xFF);
                pcm24[i * 3 + 1] = (uint8_t)((u >> 8) & 0xFF);
                pcm24[i * 3 + 2] = (uint8_t)((u >> 16) & 0xFF);
            }
            drwav_uint64 w = drwav_write_pcm_frames(wav, chunk, pcm24);
            if (w < chunk) {
                rec_handle_error(rec, w);
                return;
            }
            rec->frames_written += chunk;
            src += chunk * 2;
            remaining -= chunk;
        }
    }

    /* Periodic disk space check (~every 10 seconds) */
    if (rec->disk_check_countdown == 0) {
        rec->disk_free_bytes = oxs_recorder_disk_free(rec->filepath);
        rec->disk_low = (rec->disk_free_bytes > 0 &&
                         rec->disk_free_bytes < 500ULL * 1024 * 1024);
        rec->disk_check_countdown = rec->sample_rate * 10;
    } else if (rec->disk_check_countdown > num_frames) {
        rec->disk_check_countdown -= num_frames;
    } else {
        rec->disk_check_countdown = 0;
    }
}

/* ─── Stop recording ─────────────────────────────────────────────────────── */

void oxs_recorder_stop(oxs_recorder_t *rec)
{
    if (!rec) return;

    drwav *wav = (drwav *)rec->wav;
    if (wav) {
        drwav_uninit(wav);
        free(wav);
        rec->wav = NULL;
    }

    if (atomic_load(&rec->state) == OXS_REC_ACTIVE) {
        LOG_INFO("Recording stopped: %llu frames -> %s",
                 (unsigned long long)rec->frames_written, rec->filepath);
    }

    atomic_store(&rec->state, OXS_REC_IDLE);
}

/* ─── Auto-incrementing filenames ────────────────────────────────────────── */

int oxs_recorder_next_filename(const char *output_dir, const char *prefix,
                               int last_known, char *out_path, size_t out_path_size)
{
    if (!output_dir || !prefix || !out_path) return -1;

    ensure_directory(output_dir);

    int max_num = last_known > 0 ? last_known : 0;
    size_t prefix_len = strlen(prefix);

#ifdef _WIN32
    char search_pattern[600];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\%s_*.wav",
             output_dir, prefix);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            const char *name = fd.cFileName;
            if (strncmp(name, prefix, prefix_len) == 0 && name[prefix_len] == '_') {
                int num = atoi(name + prefix_len + 1);
                if (num > max_num) max_num = num;
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir(output_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;
            if (strncmp(name, prefix, prefix_len) == 0 && name[prefix_len] == '_') {
                int num = atoi(name + prefix_len + 1);
                if (num > max_num) max_num = num;
            }
        }
        closedir(dir);
    }
#endif

    int next = max_num + 1;

    const char *sep = "/";
#ifdef _WIN32
    sep = "\\";
#endif
    size_t dirlen = strlen(output_dir);
    if (dirlen > 0 && (output_dir[dirlen - 1] == '/' || output_dir[dirlen - 1] == '\\'))
        sep = "";

    snprintf(out_path, out_path_size, "%s%s%s_%03d.wav",
             output_dir, sep, prefix, next);

    return next;
}

/* ─── Timestamped filenames ──────────────────────────────────────────────── */

void oxs_recorder_timestamp_filename(const char *output_dir, const char *prefix,
                                     char *out_path, size_t out_path_size)
{
    if (!output_dir || !prefix || !out_path) return;

    ensure_directory(output_dir);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    const char *sep = "/";
#ifdef _WIN32
    sep = "\\";
#endif
    size_t dirlen = strlen(output_dir);
    if (dirlen > 0 && (output_dir[dirlen - 1] == '/' || output_dir[dirlen - 1] == '\\'))
        sep = "";

    snprintf(out_path, out_path_size, "%s%s%s_%04d-%02d-%02d_%02d%02d%02d.wav",
             output_dir, sep, prefix,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

/* ─── Disk free space ────────────────────────────────────────────────────── */

uint64_t oxs_recorder_disk_free(const char *path)
{
    if (!path || !path[0]) return 0;

#ifdef _WIN32
    ULARGE_INTEGER free_bytes;
    char dir[512];
    extract_dir(path, dir, sizeof(dir));
    if (GetDiskFreeSpaceExA(dir, &free_bytes, NULL, NULL))
        return (uint64_t)free_bytes.QuadPart;
    return 0;
#else
    char dir[512];
    extract_dir(path, dir, sizeof(dir));
    struct statvfs st;
    if (statvfs(dir, &st) == 0)
        return (uint64_t)st.f_bavail * (uint64_t)st.f_frsize;
    return 0;
#endif
}

/* ─── Default recording output directory ─────────────────────────────────── */

const char *oxs_recorder_output_dir(void)
{
    static char dir[512] = "";
    if (dir[0]) return dir;

#ifdef _WIN32
    /* Save alongside the exe: <exe_dir>/recordings/ */
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (len > 0) {
        /* Strip filename to get directory */
        char *sep = strrchr(exe_path, '\\');
        if (!sep) sep = strrchr(exe_path, '/');
        if (sep) *(sep + 1) = '\0';
        snprintf(dir, sizeof(dir), "%srecordings", exe_path);
    } else {
        snprintf(dir, sizeof(dir), ".\\recordings");
    }
#elif defined(__APPLE__)
    /* ~/Music/0xSYNTH/recordings/ — standard macOS music location */
    const char *home = getenv("HOME");
    if (home) snprintf(dir, sizeof(dir), "%s/Music/0xSYNTH/recordings", home);
    else snprintf(dir, sizeof(dir), "./recordings");
#else
    /* ~/Music/0xSYNTH/recordings/ on Linux (XDG music convention) */
    const char *music = getenv("XDG_MUSIC_DIR");
    if (music) {
        snprintf(dir, sizeof(dir), "%s/0xSYNTH/recordings", music);
    } else {
        const char *home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home) snprintf(dir, sizeof(dir), "%s/Music/0xSYNTH/recordings", home);
        else snprintf(dir, sizeof(dir), "./recordings");
    }
#endif

    return dir;
}
