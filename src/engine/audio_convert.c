/*
 * 0xSYNTH Audio Format Conversion
 *
 * Converts recorded WAV files to FLAC or MP3.
 * Ported from 0x808 drum machine export engine.
 */

#define LOG_TAG "convert"
#include "log.h"
#include "audio_convert.h"
#include "dr_wav.h"

#ifdef OXS_HAVE_MP3
#include "layer3.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Path helpers ───────────────────────────────────────────────────────── */

/* Replace .wav extension with new extension. Caller provides buffer. */
static void replace_extension(const char *wav_path, const char *new_ext,
                               char *out, size_t out_size)
{
    snprintf(out, out_size, "%s", wav_path);
    size_t len = strlen(out);
    if (len >= 4 && (strcmp(out + len - 4, ".wav") == 0 ||
                     strcmp(out + len - 4, ".WAV") == 0)) {
        out[len - 4] = '\0';
    }
    size_t cur = strlen(out);
    snprintf(out + cur, out_size - cur, "%s", new_ext);
}

/* ─── FLAC encoder (verbatim, no external deps) ─────────────────────────── */

/* CRC-8 lookup table (polynomial 0x07) */
static const uint8_t flac_crc8_table[256] = {
    0x00,0x07,0x0E,0x09,0x1C,0x1B,0x12,0x15,0x38,0x3F,0x36,0x31,0x24,0x23,0x2A,0x2D,
    0x70,0x77,0x7E,0x79,0x6C,0x6B,0x62,0x65,0x48,0x4F,0x46,0x41,0x54,0x53,0x5A,0x5D,
    0xE0,0xE7,0xEE,0xE9,0xFC,0xFB,0xF2,0xF5,0xD8,0xDF,0xD6,0xD1,0xC4,0xC3,0xCA,0xCD,
    0x90,0x97,0x9E,0x99,0x8C,0x8B,0x82,0x85,0xA8,0xAF,0xA6,0xA1,0xB4,0xB3,0xBA,0xBD,
    0xC7,0xC0,0xC9,0xCE,0xDB,0xDC,0xD5,0xD2,0xFF,0xF8,0xF1,0xF6,0xE3,0xE4,0xED,0xEA,
    0xB7,0xB0,0xB9,0xBE,0xAB,0xAC,0xA5,0xA2,0x8F,0x88,0x81,0x86,0x93,0x94,0x9D,0x9A,
    0x27,0x20,0x29,0x2E,0x3B,0x3C,0x35,0x32,0x1F,0x18,0x11,0x16,0x03,0x04,0x0D,0x0A,
    0x57,0x50,0x59,0x5E,0x4B,0x4C,0x45,0x42,0x6F,0x68,0x61,0x66,0x73,0x74,0x7D,0x7A,
    0x89,0x8E,0x87,0x80,0x95,0x92,0x9B,0x9C,0xB1,0xB6,0xBF,0xB8,0xAD,0xAA,0xA3,0xA4,
    0xF9,0xFE,0xF7,0xF0,0xE5,0xE2,0xEB,0xEC,0xC1,0xC6,0xCF,0xC8,0xDD,0xDA,0xD3,0xD4,
    0x69,0x6E,0x67,0x60,0x75,0x72,0x7B,0x7C,0x51,0x56,0x5F,0x58,0x4D,0x4A,0x43,0x44,
    0x19,0x1E,0x17,0x10,0x05,0x02,0x0B,0x0C,0x21,0x26,0x2F,0x28,0x3D,0x3A,0x33,0x34,
    0x4E,0x49,0x40,0x47,0x52,0x55,0x5C,0x5B,0x76,0x71,0x78,0x7F,0x6A,0x6D,0x64,0x63,
    0x3E,0x39,0x30,0x37,0x22,0x25,0x2C,0x2B,0x06,0x01,0x08,0x0F,0x1A,0x1D,0x14,0x13,
    0xAE,0xA9,0xA0,0xA7,0xB2,0xB5,0xBC,0xBB,0x96,0x91,0x98,0x9F,0x8A,0x8D,0x84,0x83,
    0xDE,0xD9,0xD0,0xD7,0xC2,0xC5,0xCC,0xCB,0xE6,0xE1,0xE8,0xEF,0xFA,0xFD,0xF4,0xF3,
};

/* CRC-16 lookup table (polynomial 0x8005) */
static const uint16_t flac_crc16_table[256] = {
    0x0000,0x8005,0x800F,0x000A,0x801B,0x001E,0x0014,0x8011,
    0x8033,0x0036,0x003C,0x8039,0x0028,0x802D,0x8027,0x0022,
    0x8063,0x0066,0x006C,0x8069,0x0078,0x807D,0x8077,0x0072,
    0x0050,0x8055,0x805F,0x005A,0x804B,0x004E,0x0044,0x8041,
    0x80C3,0x00C6,0x00CC,0x80C9,0x00D8,0x80DD,0x80D7,0x00D2,
    0x00F0,0x80F5,0x80FF,0x00FA,0x80EB,0x00EE,0x00E4,0x80E1,
    0x00A0,0x80A5,0x80AF,0x00AA,0x80BB,0x00BE,0x00B4,0x80B1,
    0x8093,0x0096,0x009C,0x8099,0x0088,0x808D,0x8087,0x0082,
    0x8183,0x0186,0x018C,0x8189,0x0198,0x819D,0x8197,0x0192,
    0x01B0,0x81B5,0x81BF,0x01BA,0x81AB,0x01AE,0x01A4,0x81A1,
    0x01E0,0x81E5,0x81EF,0x01EA,0x81FB,0x01FE,0x01F4,0x81F1,
    0x81D3,0x01D6,0x01DC,0x81D9,0x01C8,0x81CD,0x81C7,0x01C2,
    0x0140,0x8145,0x814F,0x014A,0x815B,0x015E,0x0154,0x8151,
    0x8173,0x0176,0x017C,0x8179,0x0168,0x816D,0x8167,0x0162,
    0x8123,0x0126,0x012C,0x8129,0x0138,0x813D,0x8137,0x0132,
    0x0110,0x8115,0x811F,0x011A,0x810B,0x010E,0x0104,0x8101,
    0x8303,0x0306,0x030C,0x8309,0x0318,0x831D,0x8317,0x0312,
    0x0330,0x8335,0x833F,0x033A,0x832B,0x032E,0x0324,0x8321,
    0x0360,0x8365,0x836F,0x036A,0x837B,0x037E,0x0374,0x8371,
    0x8353,0x0356,0x035C,0x8359,0x0348,0x834D,0x8347,0x0342,
    0x03C0,0x83C5,0x83CF,0x03CA,0x83DB,0x03DE,0x03D4,0x83D1,
    0x83F3,0x03F6,0x03FC,0x83F9,0x03E8,0x83ED,0x83E7,0x03E2,
    0x83A3,0x03A6,0x03AC,0x83A9,0x03B8,0x83BD,0x83B7,0x03B2,
    0x0390,0x8395,0x839F,0x039A,0x838B,0x038E,0x0384,0x8381,
    0x0280,0x8285,0x828F,0x028A,0x829B,0x029E,0x0294,0x8291,
    0x82B3,0x02B6,0x02BC,0x82B9,0x02A8,0x82AD,0x82A7,0x02A2,
    0x82E3,0x02E6,0x02EC,0x82E9,0x02F8,0x82FD,0x82F7,0x02F2,
    0x02D0,0x82D5,0x82DF,0x02DA,0x82CB,0x02CE,0x02C4,0x82C1,
    0x8243,0x0246,0x024C,0x8249,0x0258,0x825D,0x8257,0x0252,
    0x0270,0x8275,0x827F,0x027A,0x826B,0x026E,0x0264,0x8261,
    0x0220,0x8225,0x822F,0x022A,0x823B,0x023E,0x0234,0x8231,
    0x8213,0x0216,0x021C,0x8219,0x0208,0x820D,0x8207,0x0202,
};

static uint8_t flac_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) crc = flac_crc8_table[crc ^ data[i]];
    return crc;
}

static uint16_t flac_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) crc = (uint16_t)((crc << 8) ^ flac_crc16_table[(crc >> 8) ^ data[i]]);
    return crc;
}

static void flac_write_be16(FILE *fp, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    fwrite(b, 1, 2, fp);
}

static void flac_write_be24(FILE *fp, uint32_t v)
{
    uint8_t b[3] = { (uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 8) & 0xFF),
                      (uint8_t)(v & 0xFF) };
    fwrite(b, 1, 3, fp);
}

static size_t flac_encode_utf8(uint32_t val, uint8_t *buf)
{
    if (val < 0x80)       { buf[0] = (uint8_t)val; return 1; }
    if (val < 0x800)      { buf[0] = (uint8_t)(0xC0|(val>>6)); buf[1] = (uint8_t)(0x80|(val&0x3F)); return 2; }
    if (val < 0x10000)    { buf[0] = (uint8_t)(0xE0|(val>>12)); buf[1] = (uint8_t)(0x80|((val>>6)&0x3F)); buf[2] = (uint8_t)(0x80|(val&0x3F)); return 3; }
    if (val < 0x200000)   { buf[0] = (uint8_t)(0xF0|(val>>18)); buf[1] = (uint8_t)(0x80|((val>>12)&0x3F)); buf[2] = (uint8_t)(0x80|((val>>6)&0x3F)); buf[3] = (uint8_t)(0x80|(val&0x3F)); return 4; }
    if (val < 0x4000000)  { buf[0] = (uint8_t)(0xF8|(val>>24)); buf[1] = (uint8_t)(0x80|((val>>18)&0x3F)); buf[2] = (uint8_t)(0x80|((val>>12)&0x3F)); buf[3] = (uint8_t)(0x80|((val>>6)&0x3F)); buf[4] = (uint8_t)(0x80|(val&0x3F)); return 5; }
    buf[0] = (uint8_t)(0xFC|(val>>30)); buf[1] = (uint8_t)(0x80|((val>>24)&0x3F)); buf[2] = (uint8_t)(0x80|((val>>18)&0x3F)); buf[3] = (uint8_t)(0x80|((val>>12)&0x3F)); buf[4] = (uint8_t)(0x80|((val>>6)&0x3F)); buf[5] = (uint8_t)(0x80|(val&0x3F)); return 6;
}

#define FLAC_BLOCK_SIZE 4096

static int flac_sample_rate_code(uint32_t sr)
{
    switch (sr) {
    case  8000: return 4;  case 16000: return 5;  case 22050: return 6;
    case 24000: return 7;  case 32000: return 8;  case 44100: return 9;
    case 48000: return 10; case 96000: return 11;  default: return 0;
    }
}

int oxs_convert_wav_to_flac(const char *wav_path, int bit_depth)
{
    if (!wav_path) return -1;
    if (bit_depth != 16 && bit_depth != 24) bit_depth = 16;

    /* Read WAV file */
    drwav wav;
    if (!drwav_init_file(&wav, wav_path, NULL)) {
        LOG_ERROR("FLAC: failed to open %s", wav_path);
        return -1;
    }

    uint32_t channels = wav.channels;
    uint32_t sample_rate = wav.sampleRate;
    uint64_t total_frames64 = wav.totalPCMFrameCount;
    if (channels != 2 || total_frames64 == 0) {
        drwav_uninit(&wav);
        return -1;
    }
    uint32_t total_frames = (uint32_t)total_frames64;

    /* Read all samples as float */
    float *data = (float *)malloc(total_frames * channels * sizeof(float));
    if (!data) { drwav_uninit(&wav); return -1; }
    drwav_read_pcm_frames_f32(&wav, total_frames, data);
    drwav_uninit(&wav);

    /* Build output path */
    char flac_path[512];
    replace_extension(wav_path, ".flac", flac_path, sizeof(flac_path));

    int bytes_per_sample = bit_depth / 8;

    FILE *fp = fopen(flac_path, "wb");
    if (!fp) { free(data); return -1; }

    /* fLaC marker */
    fwrite("fLaC", 1, 4, fp);

    /* STREAMINFO metadata block */
    uint8_t meta_header[4] = { 0x80, 0x00, 0x00, 34 };
    fwrite(meta_header, 1, 4, fp);

    flac_write_be16(fp, FLAC_BLOCK_SIZE);
    flac_write_be16(fp, FLAC_BLOCK_SIZE);
    flac_write_be24(fp, 0);
    flac_write_be24(fp, 0);

    uint64_t si = 0;
    si |= ((uint64_t)sample_rate & 0xFFFFF) << 44;
    si |= ((uint64_t)(channels - 1) & 0x7) << 41;
    si |= ((uint64_t)(bit_depth - 1) & 0x1F) << 36;
    si |= (uint64_t)total_frames & 0xFFFFFFFFFULL;
    uint8_t si_bytes[8];
    for (int i = 7; i >= 0; i--) { si_bytes[i] = (uint8_t)(si & 0xFF); si >>= 8; }
    fwrite(si_bytes, 1, 8, fp);

    uint8_t md5[16] = {0};
    fwrite(md5, 1, 16, fp);

    /* Encode frames */
    size_t max_fb = 16 + channels * (1 + (size_t)FLAC_BLOCK_SIZE * bytes_per_sample) + 2;
    uint8_t *fb = (uint8_t *)malloc(max_fb);
    if (!fb) { free(data); fclose(fp); return -1; }

    uint32_t written = 0, fn = 0;
    while (written < total_frames) {
        uint32_t bs = FLAC_BLOCK_SIZE;
        if (written + bs > total_frames) bs = total_frames - written;

        size_t pos = 0;
        fb[pos++] = 0xFF; fb[pos++] = 0xF8;

        int bs_code, need_bs16 = 0;
        if (bs == FLAC_BLOCK_SIZE) { bs_code = 0x0C; }
        else if (bs <= 256) { bs_code = 0x06; }
        else { bs_code = 0x07; need_bs16 = 1; }

        fb[pos++] = (uint8_t)((bs_code << 4) | flac_sample_rate_code(sample_rate));
        int bps_code = (bit_depth == 24) ? 6 : 4;
        fb[pos++] = (uint8_t)(0x10 | (bps_code << 1));
        pos += flac_encode_utf8(fn, fb + pos);

        if (bs_code == 0x06) { fb[pos++] = (uint8_t)(bs - 1); }
        else if (need_bs16) { fb[pos++] = (uint8_t)((bs-1)>>8); fb[pos++] = (uint8_t)((bs-1)&0xFF); }

        fb[pos] = flac_crc8(fb, pos); pos++;

        const float *src = data + (size_t)written * channels;
        for (uint32_t ch = 0; ch < channels; ch++) {
            fb[pos++] = 0x02; /* verbatim subframe */
            for (uint32_t s = 0; s < bs; s++) {
                float v = src[s * channels + ch];
                if (v > 1.0f) v = 1.0f; if (v < -1.0f) v = -1.0f;
                if (bit_depth == 16) {
                    int16_t sv = (int16_t)(v * 32767.0f);
                    fb[pos++] = (uint8_t)((uint16_t)sv >> 8);
                    fb[pos++] = (uint8_t)((uint16_t)sv & 0xFF);
                } else {
                    int32_t sv = (int32_t)(v * 8388607.0f);
                    fb[pos++] = (uint8_t)((uint32_t)sv >> 16);
                    fb[pos++] = (uint8_t)(((uint32_t)sv >> 8) & 0xFF);
                    fb[pos++] = (uint8_t)((uint32_t)sv & 0xFF);
                }
            }
        }

        uint16_t crc16 = flac_crc16(fb, pos);
        fb[pos++] = (uint8_t)(crc16 >> 8);
        fb[pos++] = (uint8_t)(crc16 & 0xFF);
        fwrite(fb, 1, pos, fp);

        written += bs; fn++;
    }

    free(fb); free(data); fclose(fp);

    /* Delete original WAV */
    remove(wav_path);
    LOG_INFO("Converted to FLAC: %s (%u frames, %d-bit)", flac_path, total_frames, bit_depth);
    return 0;
}

/* ─── MP3 encoder (via shine) ────────────────────────────────────────────── */

#ifdef OXS_HAVE_MP3

int oxs_convert_wav_to_mp3(const char *wav_path, int bitrate)
{
    if (!wav_path) return -1;

    drwav wav;
    if (!drwav_init_file(&wav, wav_path, NULL)) {
        LOG_ERROR("MP3: failed to open %s", wav_path);
        return -1;
    }

    uint32_t sample_rate = wav.sampleRate;
    uint64_t total_frames64 = wav.totalPCMFrameCount;
    if (wav.channels != 2 || total_frames64 == 0) {
        drwav_uninit(&wav);
        return -1;
    }
    uint32_t total_frames = (uint32_t)total_frames64;

    /* Read as int16 for shine */
    int16_t *pcm = (int16_t *)malloc(total_frames * 2 * sizeof(int16_t));
    if (!pcm) { drwav_uninit(&wav); return -1; }
    drwav_read_pcm_frames_s16(&wav, total_frames, pcm);
    drwav_uninit(&wav);

    if (shine_check_config((int)sample_rate, bitrate) < 0) {
        LOG_ERROR("Unsupported MP3 config: %u Hz, %d kbps", sample_rate, bitrate);
        free(pcm);
        return -1;
    }

    shine_config_t config;
    shine_set_config_mpeg_defaults(&config.mpeg);
    config.mpeg.bitr = bitrate;
    config.mpeg.mode = STEREO;
    config.wave.channels = PCM_STEREO;
    config.wave.samplerate = (int)sample_rate;

    shine_t encoder = shine_initialise(&config);
    if (!encoder) { free(pcm); return -1; }

    int spp = shine_samples_per_pass(encoder);

    char mp3_path[512];
    replace_extension(wav_path, ".mp3", mp3_path, sizeof(mp3_path));

    FILE *fp = fopen(mp3_path, "wb");
    if (!fp) { shine_close(encoder); free(pcm); return -1; }

    uint32_t pos = 0;
    while (pos < total_frames) {
        int16_t *chunk = pcm + pos * 2;
        int16_t *buf = chunk;
        int16_t *padded = NULL;
        uint32_t remaining = total_frames - pos;
        if ((int)remaining < spp) {
            padded = (int16_t *)calloc((size_t)spp * 2, sizeof(int16_t));
            if (padded) { memcpy(padded, chunk, remaining * 2 * sizeof(int16_t)); buf = padded; }
        }

        int written = 0;
        unsigned char *mp3_data = shine_encode_buffer_interleaved(encoder, buf, &written);
        if (written > 0 && mp3_data) fwrite(mp3_data, 1, (size_t)written, fp);
        free(padded);
        pos += (uint32_t)spp;
    }

    int flushed = 0;
    unsigned char *flush_data = shine_flush(encoder, &flushed);
    if (flushed > 0 && flush_data) fwrite(flush_data, 1, (size_t)flushed, fp);

    fclose(fp);
    shine_close(encoder);
    free(pcm);

    remove(wav_path);
    LOG_INFO("Converted to MP3: %s (%d kbps)", mp3_path, bitrate);
    return 0;
}

#else /* no MP3 support */

int oxs_convert_wav_to_mp3(const char *wav_path, int bitrate)
{
    (void)wav_path; (void)bitrate;
    LOG_WARN("MP3 encoding not available (built without shine)");
    return -1;
}

#endif
