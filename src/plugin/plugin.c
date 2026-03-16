/*
 * 0xSYNTH CPLUG Plugin Implementation
 *
 * Thin bridge between CPLUG callbacks and synth_api.h.
 * All DSP happens inside oxs_synth_process().
 */

#include "config.h"
#include "../api/synth_api.h"
#ifdef OXS_HAS_PLUGIN_GUI
#include "plugin_gui.h"
#endif
#include "cplug.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ─── Plugin Instance ────────────────────────────────────────────────────── */

typedef struct {
    oxs_synth_t *synth;
    float       *interleave_buf;       /* scratch buffer for format conversion */
    uint32_t     interleave_buf_frames;
} OxsPlugin;

/* ─── Library Load/Unload ────────────────────────────────────────────────── */

void cplug_libraryLoad(void)   {}
void cplug_libraryUnload(void) {}

/* ─── Lifecycle ──────────────────────────────────────────────────────────── */

void *cplug_createPlugin(CplugHostContext *ctx)
{
    (void)ctx;
    OxsPlugin *p = calloc(1, sizeof(OxsPlugin));
    if (!p) return NULL;
    p->synth = oxs_synth_create(44100); /* default, overridden by setSampleRate */
    return p;
}

void cplug_destroyPlugin(void *ptr)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    if (!p) return;
    oxs_synth_destroy(p->synth);
    free(p->interleave_buf);
    free(p);
}

/* ─── Bus Layout (instrument: no input, stereo output) ───────────────────── */

uint32_t cplug_getNumInputBusses(void *ptr)  { (void)ptr; return 0; }
uint32_t cplug_getNumOutputBusses(void *ptr) { (void)ptr; return 1; }

uint32_t cplug_getInputBusChannelCount(void *ptr, uint32_t idx)
{
    (void)ptr; (void)idx; return 0;
}

uint32_t cplug_getOutputBusChannelCount(void *ptr, uint32_t idx)
{
    (void)ptr; (void)idx; return 2; /* stereo */
}

void cplug_getInputBusName(void *ptr, uint32_t idx, char *buf, size_t len)
{
    (void)ptr; (void)idx;
    snprintf(buf, len, "Input");
}

void cplug_getOutputBusName(void *ptr, uint32_t idx, char *buf, size_t len)
{
    (void)ptr; (void)idx;
    snprintf(buf, len, "Output");
}

/* ─── Timing ─────────────────────────────────────────────────────────────── */

uint32_t cplug_getLatencyInSamples(void *ptr) { (void)ptr; return 0; }
uint32_t cplug_getTailInSamples(void *ptr)    { (void)ptr; return 0; }

void cplug_setSampleRateAndBlockSize(void *ptr, double sampleRate, uint32_t maxBlockSize)
{
    OxsPlugin *p = (OxsPlugin *)ptr;

    /* Destroy and recreate engine at new sample rate */
    oxs_synth_destroy(p->synth);
    p->synth = oxs_synth_create((uint32_t)sampleRate);

    /* Reallocate interleave buffer */
    free(p->interleave_buf);
    p->interleave_buf = calloc(maxBlockSize * 2, sizeof(float));
    p->interleave_buf_frames = maxBlockSize;
}

/* ─── Parameters ─────────────────────────────────────────────────────────── */

uint32_t cplug_getNumParameters(void *ptr)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    return oxs_synth_param_count(p->synth);
}

uint32_t cplug_getParameterID(void *ptr, uint32_t paramIndex)
{
    /* We need to map sequential index to actual param ID.
     * Our param IDs have gaps, so we iterate the registry. */
    OxsPlugin *p = (OxsPlugin *)ptr;
    uint32_t count = 0;
    for (uint32_t i = 0; i < OXS_PARAM_SLOT_COUNT; i++) {
        oxs_param_info_t info;
        if (oxs_synth_param_info(p->synth, i, &info)) {
            if (count == paramIndex) return i;
            count++;
        }
    }
    return 0;
}

uint32_t cplug_getParameterFlags(void *ptr, uint32_t paramId)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_param_info_t info;
    if (!oxs_synth_param_info(p->synth, paramId, &info)) return 0;

    uint32_t flags = 0;
    if (info.flags & OXS_PARAM_FLAG_AUTOMATABLE)
        flags |= CPLUG_FLAG_PARAMETER_IS_AUTOMATABLE;
    if (info.flags & OXS_PARAM_FLAG_INTEGER)
        flags |= CPLUG_FLAG_PARAMETER_IS_INTEGER;
    if (info.flags & OXS_PARAM_FLAG_BOOLEAN)
        flags |= CPLUG_FLAG_PARAMETER_IS_BOOL;
    return flags;
}

void cplug_getParameterRange(void *ptr, uint32_t paramId, double *min, double *max)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_param_info_t info;
    if (oxs_synth_param_info(p->synth, paramId, &info)) {
        *min = (double)info.min;
        *max = (double)info.max;
    } else {
        *min = 0.0; *max = 1.0;
    }
}

void cplug_getParameterName(void *ptr, uint32_t paramId, char *buf, size_t len)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_param_info_t info;
    if (oxs_synth_param_info(p->synth, paramId, &info)) {
        snprintf(buf, len, "%s", info.name);
    } else {
        snprintf(buf, len, "Unknown");
    }
}

double cplug_getParameterValue(void *ptr, uint32_t paramId)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    return (double)oxs_synth_get_param(p->synth, paramId);
}

double cplug_getDefaultParameterValue(void *ptr, uint32_t paramId)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_param_info_t info;
    if (oxs_synth_param_info(p->synth, paramId, &info))
        return (double)info.default_val;
    return 0.0;
}

void cplug_setParameterValue(void *ptr, uint32_t paramId, double value)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_synth_set_param(p->synth, paramId, (float)value);
}

double cplug_denormaliseParameterValue(void *ptr, uint32_t paramId, double normalised)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_param_info_t info;
    if (oxs_synth_param_info(p->synth, paramId, &info))
        return (double)info.min + normalised * ((double)info.max - (double)info.min);
    return normalised;
}

double cplug_normaliseParameterValue(void *ptr, uint32_t paramId, double value)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_param_info_t info;
    if (oxs_synth_param_info(p->synth, paramId, &info)) {
        double range = (double)info.max - (double)info.min;
        if (range > 0.0) return (value - (double)info.min) / range;
    }
    return value;
}

double cplug_parameterStringToValue(void *ptr, uint32_t paramId, const char *str)
{
    (void)ptr; (void)paramId;
    return atof(str);
}

void cplug_parameterValueToString(void *ptr, uint32_t paramId, char *buf,
                                  size_t bufsize, double value)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    oxs_param_info_t info;
    if (oxs_synth_param_info(p->synth, paramId, &info)) {
        if (info.flags & OXS_PARAM_FLAG_INTEGER)
            snprintf(buf, bufsize, "%d", (int)value);
        else
            snprintf(buf, bufsize, "%.2f", value);
    } else {
        snprintf(buf, bufsize, "%.2f", value);
    }
}

/* ─── Process ────────────────────────────────────────────────────────────── */

void cplug_process(void *ptr, CplugProcessContext *ctx)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    if (!p->synth || !p->interleave_buf) return;

    CplugEvent event;
    uint32_t frame = 0;

    while (ctx->dequeueEvent(ctx, &event, frame)) {
        switch (event.type) {
        case CPLUG_EVENT_PARAM_CHANGE_UPDATE:
            oxs_synth_set_param(p->synth, event.parameter.id,
                                (float)event.parameter.value);
            break;

        case CPLUG_EVENT_MIDI: {
            uint8_t status = event.midi.status & 0xF0;
            uint8_t channel = event.midi.status & 0x0F;
            uint8_t note = event.midi.data1;
            uint8_t vel = event.midi.data2;

            if (status == 0x90 && vel > 0) {
                oxs_synth_note_on(p->synth, note, vel, channel);
            } else if (status == 0x80 || (status == 0x90 && vel == 0)) {
                oxs_synth_note_off(p->synth, note, channel);
            } else if (status == 0xB0) {
                /* MIDI CC */
                oxs_synth_midi_cc(p->synth, note, vel);
            }
            break;
        }

        case CPLUG_EVENT_PROCESS_AUDIO: {
            uint32_t num_frames = event.processAudio.endFrame - frame;
            if (num_frames > p->interleave_buf_frames)
                num_frames = p->interleave_buf_frames;

            /* Render into interleaved scratch buffer */
            oxs_synth_process(p->synth, p->interleave_buf, num_frames);

            /* De-interleave into CPLUG's non-interleaved output */
            float **output = ctx->getAudioOutput(ctx, 0);
            if (output && output[0] && output[1]) {
                for (uint32_t i = 0; i < num_frames; i++) {
                    output[0][frame + i] = p->interleave_buf[i * 2];
                    output[1][frame + i] = p->interleave_buf[i * 2 + 1];
                }
            }

            frame = event.processAudio.endFrame;
            break;
        }

        default:
            break;
        }
    }
}

/* ─── State Save/Load ────────────────────────────────────────────────────── */

typedef struct {
    uint32_t id;
    double   value;
} oxs_param_state_t;

void cplug_saveState(void *ptr, const void *stateCtx, cplug_writeProc writeProc)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    uint32_t num_params = oxs_synth_param_count(p->synth);

    oxs_param_state_t *state = calloc(num_params, sizeof(oxs_param_state_t));
    if (!state) return;

    uint32_t idx = 0;
    for (uint32_t i = 0; i < OXS_PARAM_SLOT_COUNT && idx < num_params; i++) {
        oxs_param_info_t info;
        if (oxs_synth_param_info(p->synth, i, &info)) {
            state[idx].id = i;
            state[idx].value = (double)oxs_synth_get_param(p->synth, i);
            idx++;
        }
    }

    writeProc(stateCtx, state, idx * sizeof(oxs_param_state_t));
    free(state);
}

void cplug_loadState(void *ptr, const void *stateCtx, cplug_readProc readProc)
{
    OxsPlugin *p = (OxsPlugin *)ptr;

    /* Read up to 2x expected params for forward compat */
    size_t max_entries = OXS_PARAM_SLOT_COUNT * 2;
    oxs_param_state_t *state = calloc(max_entries, sizeof(oxs_param_state_t));
    if (!state) return;

    int64_t bytes = readProc(stateCtx, state, max_entries * sizeof(oxs_param_state_t));
    if (bytes > 0) {
        size_t count = (size_t)bytes / sizeof(oxs_param_state_t);
        for (size_t i = 0; i < count; i++) {
            if (state[i].id < OXS_PARAM_SLOT_COUNT) {
                oxs_synth_set_param(p->synth, state[i].id, (float)state[i].value);
            }
        }
    }

    free(state);
}

/* ─── GUI ────────────────────────────────────────────────────────────────── */

#ifdef OXS_HAS_PLUGIN_GUI

void *cplug_createGUI(void *ptr)
{
    OxsPlugin *p = (OxsPlugin *)ptr;
    return oxs_plugin_gui_create(p->synth);
}
void cplug_destroyGUI(void *gui) { oxs_plugin_gui_destroy((oxs_plugin_gui_t *)gui); }
void cplug_setParent(void *gui, void *hwnd)
{
    if (hwnd) oxs_plugin_gui_attach((oxs_plugin_gui_t *)gui, hwnd);
    else oxs_plugin_gui_detach((oxs_plugin_gui_t *)gui);
}
void cplug_setVisible(void *gui, bool visible) { oxs_plugin_gui_set_visible((oxs_plugin_gui_t *)gui, visible); }
void cplug_setScaleFactor(void *gui, float scale) { (void)gui; (void)scale; }
void cplug_getSize(void *gui, uint32_t *w, uint32_t *h) { oxs_plugin_gui_get_size((oxs_plugin_gui_t *)gui, w, h); }
void cplug_checkSize(void *gui, uint32_t *w, uint32_t *h) { (void)gui; (void)w; (void)h; }
bool cplug_setSize(void *gui, uint32_t w, uint32_t h) { return oxs_plugin_gui_set_size((oxs_plugin_gui_t *)gui, w, h); }

#else /* No GUI — stubs */

void *cplug_createGUI(void *ptr) { (void)ptr; return NULL; }
void  cplug_destroyGUI(void *gui) { (void)gui; }
void  cplug_setParent(void *gui, void *hwnd) { (void)gui; (void)hwnd; }
void  cplug_setVisible(void *gui, bool visible) { (void)gui; (void)visible; }
void  cplug_setScaleFactor(void *gui, float scale) { (void)gui; (void)scale; }
void  cplug_getSize(void *gui, uint32_t *w, uint32_t *h) { (void)gui; *w = 800; *h = 600; }
void  cplug_checkSize(void *gui, uint32_t *w, uint32_t *h) { (void)gui; (void)w; (void)h; }
bool  cplug_setSize(void *gui, uint32_t w, uint32_t h) { (void)gui; (void)w; (void)h; return true; }

#endif
