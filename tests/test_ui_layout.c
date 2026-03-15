/*
 * 0xSYNTH UI Layout Tests (Phase 8)
 */

#include "../src/ui/ui_types.h"
#include "../src/engine/params.h"

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-55s ", name); } while (0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while (0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

/* Count total widgets recursively */
static int count_widgets(const oxs_ui_widget_t *w)
{
    int c = 1;
    for (int i = 0; i < w->num_children; i++)
        c += count_widgets(w->children[i]);
    return c;
}

/* Count knobs specifically */
static int count_knobs(const oxs_ui_widget_t *w)
{
    int c = (w->type == OXS_UI_KNOB) ? 1 : 0;
    for (int i = 0; i < w->num_children; i++)
        c += count_knobs(w->children[i]);
    return c;
}

/* Collect all param_ids bound in layout */
static void collect_param_ids(const oxs_ui_widget_t *w, int *ids, int *count, int max)
{
    if (w->param_id >= 0 && *count < max) ids[(*count)++] = w->param_id;
    if (w->type == OXS_UI_ENVELOPE) {
        if (w->env_attack_id >= 0 && *count < max)  ids[(*count)++] = w->env_attack_id;
        if (w->env_decay_id >= 0 && *count < max)   ids[(*count)++] = w->env_decay_id;
        if (w->env_sustain_id >= 0 && *count < max)  ids[(*count)++] = w->env_sustain_id;
        if (w->env_release_id >= 0 && *count < max)  ids[(*count)++] = w->env_release_id;
    }
    for (int i = 0; i < w->num_children; i++)
        collect_param_ids(w->children[i], ids, count, max);
}

static void test_layout_builds(void)
{
    TEST("layout builds without crashing");
    const oxs_ui_layout_t *l = oxs_ui_build_layout();
    ASSERT(l != NULL, "layout is NULL");
    ASSERT(l->root != NULL, "root is NULL");
    ASSERT(l->total_widgets > 10, "too few widgets");
    PASS();
}

static void test_layout_has_sections(void)
{
    TEST("layout has all major sections");
    const oxs_ui_layout_t *l = oxs_ui_build_layout();
    /* Root should have children for: top, oscillator, filter, envelopes,
     * LFO, FM, wavetable, effects, keyboard */
    ASSERT(l->root->num_children >= 8, "root should have at least 8 sections");
    PASS();
}

static void test_layout_widget_count(void)
{
    TEST("layout has reasonable widget count");
    const oxs_ui_layout_t *l = oxs_ui_build_layout();
    int total = count_widgets(l->root);
    ASSERT(total >= 50, "should have at least 50 widgets");
    ASSERT(total < 300, "should have fewer than 300 widgets");

    int knobs = count_knobs(l->root);
    ASSERT(knobs >= 30, "should have at least 30 knobs");
    PASS();
}

static void test_layout_params_valid(void)
{
    TEST("all layout param IDs exist in registry");
    const oxs_ui_layout_t *l = oxs_ui_build_layout();
    ASSERT(oxs_ui_validate_layout(l), "layout validation failed");
    PASS();
}

static void test_no_duplicate_bindings(void)
{
    TEST("no duplicate param bindings (warning only)");
    const oxs_ui_layout_t *l = oxs_ui_build_layout();

    int ids[512];
    int count = 0;
    collect_param_ids(l->root, ids, &count, 512);

    /* Check for duplicates — warn but don't fail
     * (some params like envelope ADSR appear in both knobs and envelope display) */
    int dups = 0;
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (ids[i] == ids[j]) dups++;
        }
    }
    /* Envelope params are intentionally duplicated (knob + display) */
    ASSERT(dups < 20, "too many duplicate param bindings");
    PASS();
}

int main(void)
{
    printf("0xSYNTH UI Layout Tests\n");
    printf("========================\n\n");

    test_layout_builds();
    test_layout_has_sections();
    test_layout_widget_count();
    test_layout_params_valid();
    test_no_duplicate_bindings();

    printf("\n========================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
