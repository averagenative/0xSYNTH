/*
 * 0xSYNTH Streaming WAV Recorder
 *
 * Writes audio directly to disk during recording via dr_wav streaming API.
 * No large in-memory buffer — audio is flushed to the OS page cache each
 * callback (~256-512 frames = 1-2 KB per write).
 *
 * Ported from 0x808 drum machine.
 */

#ifndef OXS_RECORDER_H
#define OXS_RECORDER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifndef __cplusplus
#include <stdatomic.h>
#endif

typedef enum {
    OXS_REC_IDLE = 0,
    OXS_REC_ACTIVE,
    OXS_REC_ERROR
} oxs_rec_state_t;

typedef struct {
    void               *wav;
#ifdef __cplusplus
    volatile int        state;  /* oxs_rec_state_t */
#else
    _Atomic int         state;  /* oxs_rec_state_t, atomic for thread safety */
#endif
    uint32_t        bit_depth;
    uint32_t        sample_rate;
    uint64_t        frames_written;
    char            filepath[512];
    uint64_t        disk_free_bytes;
    bool            disk_low;
    uint32_t        disk_check_countdown;
    int             next_number;
} oxs_recorder_t;

/* Start recording to filepath. bit_depth: 16, 24, or 32. */
int oxs_recorder_start(oxs_recorder_t *rec, const char *filepath,
                       uint32_t sample_rate, uint32_t bit_depth);

/* Write interleaved stereo float frames. Called from audio callback. */
void oxs_recorder_write(oxs_recorder_t *rec, const float *output,
                        uint32_t num_frames);

/* Stop recording and finalize WAV file. */
void oxs_recorder_stop(oxs_recorder_t *rec);

/* Generate next auto-incrementing filename.
 * Returns the file number, or -1 on error. */
int oxs_recorder_next_filename(const char *output_dir, const char *prefix,
                               int last_known, char *out_path, size_t out_path_size);

/* Generate a timestamped filename: <output_dir>/<prefix>_YYYY-MM-DD_HHMMSS.wav */
void oxs_recorder_timestamp_filename(const char *output_dir, const char *prefix,
                                     char *out_path, size_t out_path_size);

/* Get free disk space for the given path. */
uint64_t oxs_recorder_disk_free(const char *path);

/* Get the default recording output directory. */
const char *oxs_recorder_output_dir(void);

#endif /* OXS_RECORDER_H */
