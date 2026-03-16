/*
 * 0xSYNTH Audio Format Conversion
 *
 * Converts WAV files to MP3 or FLAC after recording.
 * WAV recording is always done in real-time; conversion happens on stop.
 */

#ifndef OXS_AUDIO_CONVERT_H
#define OXS_AUDIO_CONVERT_H

#include <stdint.h>

typedef enum {
    OXS_REC_FORMAT_WAV = 0,
    OXS_REC_FORMAT_FLAC,
    OXS_REC_FORMAT_MP3,
    OXS_REC_FORMAT_COUNT
} oxs_rec_format_t;

/* Convert a WAV file to FLAC (verbatim encoding). Returns 0 on success.
 * bit_depth: 16 or 24. Output replaces .wav with .flac. */
int oxs_convert_wav_to_flac(const char *wav_path, int bit_depth);

/* Convert a WAV file to MP3. Returns 0 on success.
 * bitrate: 128, 192, 256, or 320. Output replaces .wav with .mp3. */
int oxs_convert_wav_to_mp3(const char *wav_path, int bitrate);

#endif /* OXS_AUDIO_CONVERT_H */
