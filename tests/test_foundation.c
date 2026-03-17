/*
 * 0xSYNTH Foundation Tests
 *
 * Tests for Phase 1: parameter system, command queue, output events,
 * and API lifecycle.
 */

#include "synth_api.h"
#include "../src/engine/params.h"
#include "../src/engine/command_queue.h"
#include "../src/engine/output_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %-50s ", name); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
    } while (0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { FAIL(msg); return; } \
    } while (0)

#define ASSERT_FLOAT_EQ(a, b, eps, msg) \
    ASSERT(fabsf((a) - (b)) < (eps), msg)

/* ===== Parameter Registry Tests ===== */

static void test_registry_init(void)
{
    TEST("registry initializes with params");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);
    ASSERT(reg.initialized, "registry not initialized");
    ASSERT(reg.count > 0, "no params registered");
    ASSERT(reg.count <= OXS_PARAM_COUNT, "too many params");
    PASS();
}

static void test_registry_defaults(void)
{
    TEST("registry has correct defaults");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);

    ASSERT_FLOAT_EQ(reg.info[OXS_PARAM_MASTER_VOLUME].default_val, 0.8f, 0.001f,
                    "master volume default");
    ASSERT_FLOAT_EQ(reg.info[OXS_PARAM_FILTER_CUTOFF].default_val, 20000.0f, 1.0f,
                    "filter cutoff default");
    ASSERT_FLOAT_EQ(reg.info[OXS_PARAM_AMP_SUSTAIN].default_val, 0.7f, 0.001f,
                    "amp sustain default");
    PASS();
}

static void test_registry_metadata(void)
{
    TEST("registry metadata is populated");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);

    oxs_param_info_t info;
    ASSERT(oxs_param_get_info(&reg, OXS_PARAM_FILTER_CUTOFF, &info), "get info failed");
    ASSERT(strcmp(info.name, "Filter Cutoff") == 0, "wrong name");
    ASSERT(strcmp(info.group, "Filter") == 0, "wrong group");
    ASSERT_FLOAT_EQ(info.min, 20.0f, 0.1f, "wrong min");
    ASSERT_FLOAT_EQ(info.max, 20000.0f, 1.0f, "wrong max");
    ASSERT(strcmp(info.units, "Hz") == 0, "wrong units");
    ASSERT(info.flags & OXS_PARAM_FLAG_AUTOMATABLE, "not automatable");
    PASS();
}

static void test_registry_lookup_by_name(void)
{
    TEST("registry lookup by name");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);

    int32_t id = oxs_param_id_by_name(&reg, "Filter Cutoff");
    ASSERT(id == OXS_PARAM_FILTER_CUTOFF, "wrong id for Filter Cutoff");

    id = oxs_param_id_by_name(&reg, "nonexistent");
    ASSERT(id == -1, "should return -1 for nonexistent param");
    PASS();
}

static void test_registry_invalid_id(void)
{
    TEST("registry rejects invalid IDs");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);

    oxs_param_info_t info;
    ASSERT(!oxs_param_get_info(&reg, OXS_PARAM_COUNT + 10, &info), "should reject out of bounds");
    /* Slot 5 is in the gap between master and oscillator — should be empty */
    ASSERT(!oxs_param_get_info(&reg, 5, &info), "should reject empty slot");
    PASS();
}

/* ===== Atomic Param Store Tests ===== */

static void test_param_store_defaults(void)
{
    TEST("param store initialized with defaults");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);

    oxs_param_store_t store;
    oxs_param_store_init(&store, &reg);

    ASSERT_FLOAT_EQ(oxs_param_get(&store, OXS_PARAM_MASTER_VOLUME), 0.8f, 0.001f,
                    "master volume default");
    ASSERT_FLOAT_EQ(oxs_param_get(&store, OXS_PARAM_FILTER_CUTOFF), 20000.0f, 1.0f,
                    "filter cutoff default");
    PASS();
}

static void test_param_set_get_roundtrip(void)
{
    TEST("param set/get round-trip");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);
    oxs_param_store_t store;
    oxs_param_store_init(&store, &reg);

    oxs_param_set(&store, OXS_PARAM_FILTER_CUTOFF, 1234.5f);
    ASSERT_FLOAT_EQ(oxs_param_get(&store, OXS_PARAM_FILTER_CUTOFF), 1234.5f, 0.01f,
                    "roundtrip failed");

    oxs_param_set(&store, OXS_PARAM_AMP_ATTACK, 0.05f);
    ASSERT_FLOAT_EQ(oxs_param_get(&store, OXS_PARAM_AMP_ATTACK), 0.05f, 0.001f,
                    "roundtrip 2 failed");
    PASS();
}

static void test_param_snapshot(void)
{
    TEST("param snapshot captures current state");
    oxs_param_registry_t reg;
    oxs_param_registry_init(&reg);
    oxs_param_store_t store;
    oxs_param_store_init(&store, &reg);

    oxs_param_set(&store, OXS_PARAM_FILTER_CUTOFF, 5000.0f);
    oxs_param_set(&store, OXS_PARAM_LFO_RATE, 3.5f);

    oxs_param_snapshot_t snap;
    oxs_param_snapshot(&store, &snap);

    ASSERT_FLOAT_EQ(snap.values[OXS_PARAM_FILTER_CUTOFF], 5000.0f, 0.1f, "snapshot cutoff");
    ASSERT_FLOAT_EQ(snap.values[OXS_PARAM_LFO_RATE], 3.5f, 0.01f, "snapshot lfo rate");
    ASSERT_FLOAT_EQ(snap.values[OXS_PARAM_MASTER_VOLUME], 0.8f, 0.001f, "snapshot master vol");
    PASS();
}

/* ===== MIDI CC Mapping Tests ===== */

static void test_cc_map_init(void)
{
    TEST("CC map initialized with defaults");
    oxs_midi_cc_map_t map;
    oxs_midi_cc_map_init(&map);

    /* Most CCs should be unassigned, but some have standard defaults */
    int assigned_count = 0;
    for (int i = 0; i < OXS_MIDI_CC_COUNT; i++) {
        if (map.param_id[i] != OXS_MIDI_CC_UNASSIGNED)
            assigned_count++;
    }
    /* Verify defaults exist (CC7=volume, CC74=cutoff, etc.) */
    ASSERT(map.param_id[7] == OXS_PARAM_MASTER_VOLUME, "CC7 should be master volume");
    ASSERT(map.param_id[74] == OXS_PARAM_FILTER_CUTOFF, "CC74 should be filter cutoff");
    ASSERT(assigned_count > 0, "Should have some default mappings");
    PASS();
}

static void test_cc_map_assign_unassign(void)
{
    TEST("CC map assign and unassign");
    oxs_midi_cc_map_t map;
    oxs_midi_cc_map_init(&map);

    oxs_midi_cc_assign(&map, 74, OXS_PARAM_FILTER_CUTOFF);
    ASSERT(map.param_id[74] == OXS_PARAM_FILTER_CUTOFF, "assign failed");

    oxs_midi_cc_unassign(&map, 74);
    ASSERT(map.param_id[74] == OXS_MIDI_CC_UNASSIGNED, "unassign failed");
    PASS();
}

/* ===== Command Queue Tests ===== */

static void test_cmd_queue_push_pop(void)
{
    TEST("command queue push and pop");
    oxs_cmd_queue_t q;
    oxs_cmd_queue_init(&q);

    oxs_command_t cmd = oxs_cmd_note_on(60, 100, 0);
    ASSERT(oxs_cmd_queue_push(&q, cmd), "push failed");

    oxs_command_t out;
    ASSERT(oxs_cmd_queue_pop(&q, &out), "pop failed");
    ASSERT(out.type == OXS_CMD_NOTE_ON, "wrong type");
    ASSERT(out.data.note.note == 60, "wrong note");
    ASSERT(out.data.note.velocity == 100, "wrong velocity");
    PASS();
}

static void test_cmd_queue_empty(void)
{
    TEST("command queue empty returns false");
    oxs_cmd_queue_t q;
    oxs_cmd_queue_init(&q);

    oxs_command_t out;
    ASSERT(!oxs_cmd_queue_pop(&q, &out), "should be empty");
    PASS();
}

static void test_cmd_queue_fifo_order(void)
{
    TEST("command queue maintains FIFO order");
    oxs_cmd_queue_t q;
    oxs_cmd_queue_init(&q);

    oxs_cmd_queue_push(&q, oxs_cmd_note_on(60, 100, 0));
    oxs_cmd_queue_push(&q, oxs_cmd_note_on(64, 80, 0));
    oxs_cmd_queue_push(&q, oxs_cmd_note_off(60, 0));

    oxs_command_t out;
    oxs_cmd_queue_pop(&q, &out);
    ASSERT(out.data.note.note == 60 && out.type == OXS_CMD_NOTE_ON, "wrong first");

    oxs_cmd_queue_pop(&q, &out);
    ASSERT(out.data.note.note == 64 && out.type == OXS_CMD_NOTE_ON, "wrong second");

    oxs_cmd_queue_pop(&q, &out);
    ASSERT(out.data.note.note == 60 && out.type == OXS_CMD_NOTE_OFF, "wrong third");
    PASS();
}

static void test_cmd_queue_full(void)
{
    TEST("command queue full returns false");
    oxs_cmd_queue_t q;
    oxs_cmd_queue_init(&q);

    /* Fill queue (capacity is SIZE - 1 = 255) */
    for (int i = 0; i < OXS_CMD_QUEUE_SIZE - 1; i++) {
        ASSERT(oxs_cmd_queue_push(&q, oxs_cmd_note_on(60, 100, 0)), "push should succeed");
    }
    ASSERT(!oxs_cmd_queue_push(&q, oxs_cmd_note_on(60, 100, 0)), "should be full");
    PASS();
}

/* ===== Output Event Queue Tests ===== */

static void test_output_queue_push_pop(void)
{
    TEST("output queue push and pop");
    oxs_output_queue_t q;
    oxs_output_queue_init(&q);

    oxs_output_event_t ev = { .peak_l = 0.5f, .peak_r = 0.3f, .voice_active = 0x0003 };
    ASSERT(oxs_output_queue_push(&q, &ev), "push failed");

    oxs_output_event_t out;
    ASSERT(oxs_output_queue_pop(&q, &out), "pop failed");
    ASSERT_FLOAT_EQ(out.peak_l, 0.5f, 0.001f, "wrong peak_l");
    ASSERT_FLOAT_EQ(out.peak_r, 0.3f, 0.001f, "wrong peak_r");
    ASSERT(out.voice_active == 0x0003, "wrong voice_active");
    PASS();
}

static void test_output_queue_empty(void)
{
    TEST("output queue empty returns false");
    oxs_output_queue_t q;
    oxs_output_queue_init(&q);

    oxs_output_event_t out;
    ASSERT(!oxs_output_queue_pop(&q, &out), "should be empty");
    PASS();
}

/* ===== API Lifecycle Tests ===== */

static void test_api_create_destroy(void)
{
    TEST("API create and destroy");
    oxs_synth_t *s = oxs_synth_create(44100);
    ASSERT(s != NULL, "create returned NULL");
    ASSERT(oxs_synth_sample_rate(s) == 44100, "wrong sample rate");
    oxs_synth_destroy(s);
    PASS();
}

static void test_api_param_access(void)
{
    TEST("API param set/get through opaque handle");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Read default */
    float vol = oxs_synth_get_param(s, OXS_PARAM_MASTER_VOLUME);
    ASSERT_FLOAT_EQ(vol, 0.8f, 0.001f, "wrong default master vol");

    /* Set and read back */
    oxs_synth_set_param(s, OXS_PARAM_FILTER_CUTOFF, 2000.0f);
    float cutoff = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    ASSERT_FLOAT_EQ(cutoff, 2000.0f, 0.1f, "set/get roundtrip failed");

    oxs_synth_destroy(s);
    PASS();
}

static void test_api_param_info(void)
{
    TEST("API param info query");
    oxs_synth_t *s = oxs_synth_create(44100);

    uint32_t count = oxs_synth_param_count(s);
    ASSERT(count > 50, "too few params");

    oxs_param_info_t info;
    ASSERT(oxs_synth_param_info(s, OXS_PARAM_FILTER_CUTOFF, &info), "info query failed");
    ASSERT(strcmp(info.name, "Filter Cutoff") == 0, "wrong name via API");

    int32_t id = oxs_synth_param_id_by_name(s, "LFO Rate");
    ASSERT(id == OXS_PARAM_LFO_RATE, "name lookup failed");

    oxs_synth_destroy(s);
    PASS();
}

static void test_api_process_silent(void)
{
    TEST("API process outputs silence with no voices");
    oxs_synth_t *s = oxs_synth_create(44100);

    float buf[512]; /* 256 frames stereo */
    memset(buf, 0xFF, sizeof(buf)); /* fill with garbage */
    oxs_synth_process(s, buf, 256);

    /* Should be all zeros (no active voices) */
    int silent = 1;
    for (int i = 0; i < 512; i++) {
        if (fabsf(buf[i]) > 1e-6f) { silent = 0; break; }
    }
    ASSERT(silent, "output should be silent with no voices");

    /* Should have pushed an output event */
    oxs_output_event_t ev;
    ASSERT(oxs_synth_pop_output_event(s, &ev), "no output event after process");
    ASSERT_FLOAT_EQ(ev.peak_l, 0.0f, 0.001f, "peak should be 0");

    oxs_synth_destroy(s);
    PASS();
}

static void test_api_note_commands(void)
{
    TEST("API note on/off queues commands");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Queue some notes */
    oxs_synth_note_on(s, 60, 100, 0);
    oxs_synth_note_on(s, 64, 80, 0);
    oxs_synth_note_off(s, 60, 0);

    /* Process should drain them without crashing */
    float buf[512];
    oxs_synth_process(s, buf, 256);

    oxs_synth_destroy(s);
    PASS();
}

static void test_api_panic(void)
{
    TEST("API panic doesn't crash");
    oxs_synth_t *s = oxs_synth_create(44100);

    oxs_synth_note_on(s, 60, 100, 0);
    oxs_synth_panic(s);

    float buf[512];
    oxs_synth_process(s, buf, 256);

    oxs_synth_destroy(s);
    PASS();
}

static void test_api_midi_cc(void)
{
    TEST("API MIDI CC assignment and processing");
    oxs_synth_t *s = oxs_synth_create(44100);

    /* Assign CC 74 to filter cutoff */
    oxs_synth_cc_assign(s, 74, OXS_PARAM_FILTER_CUTOFF);

    /* Send CC 74 = 64 (midpoint) */
    oxs_synth_midi_cc(s, 74, 64);

    /* Process to drain command queue */
    float buf[512];
    oxs_synth_process(s, buf, 256);

    /* Filter cutoff should now be ~midpoint of 20-20000 range */
    float cutoff = oxs_synth_get_param(s, OXS_PARAM_FILTER_CUTOFF);
    /* 64/127 * (20000-20) + 20 ≈ 10089 */
    ASSERT(cutoff > 9000.0f && cutoff < 11000.0f, "CC didn't scale correctly");

    oxs_synth_destroy(s);
    PASS();
}

/* ===== Main ===== */

int main(void)
{
    printf("0xSYNTH Foundation Tests\n");
    printf("========================\n\n");

    /* Parameter registry */
    printf("Parameter Registry:\n");
    test_registry_init();
    test_registry_defaults();
    test_registry_metadata();
    test_registry_lookup_by_name();
    test_registry_invalid_id();

    /* Param store */
    printf("\nAtomic Parameter Store:\n");
    test_param_store_defaults();
    test_param_set_get_roundtrip();
    test_param_snapshot();

    /* MIDI CC */
    printf("\nMIDI CC Mapping:\n");
    test_cc_map_init();
    test_cc_map_assign_unassign();

    /* Command queue */
    printf("\nCommand Queue:\n");
    test_cmd_queue_push_pop();
    test_cmd_queue_empty();
    test_cmd_queue_fifo_order();
    test_cmd_queue_full();

    /* Output events */
    printf("\nOutput Event Queue:\n");
    test_output_queue_push_pop();
    test_output_queue_empty();

    /* API lifecycle */
    printf("\nPublic API:\n");
    test_api_create_destroy();
    test_api_param_access();
    test_api_param_info();
    test_api_process_silent();
    test_api_note_commands();
    test_api_panic();
    test_api_midi_cc();

    printf("\n========================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
