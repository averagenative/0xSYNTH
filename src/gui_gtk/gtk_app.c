/*
 * 0xSYNTH GTK 4 Application Implementation
 *
 * Walks the UI layout tree and creates GTK widgets.
 * Custom knobs, envelope displays, and meters use GtkDrawingArea + Cairo.
 */

#include "gtk_app.h"
#include "../ui/ui_types.h"
#include "../engine/types.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── App Context ────────────────────────────────────────────────────────── */

typedef struct {
    oxs_synth_t    *synth;
    GtkApplication *app;
    GtkWidget      *window;
    guint           timer_id;  /* periodic UI refresh timer */
} OxsGtkApp;

static OxsGtkApp g_app;

/* ─── Custom Knob Widget ─────────────────────────────────────────────────── */

typedef struct {
    GtkDrawingArea *area;
    oxs_synth_t    *synth;
    int32_t         param_id;
    float           min, max;
    char            label[48];
    bool            dragging;
    double          drag_start_y;
    float           drag_start_val;
} OxsKnob;

static void knob_draw(GtkDrawingArea *area, cairo_t *cr,
                      int width, int height, gpointer data)
{
    (void)area;
    OxsKnob *k = (OxsKnob *)data;
    float val = oxs_synth_get_param(k->synth, (uint32_t)k->param_id);
    float normalized = (val - k->min) / (k->max - k->min);
    if (normalized < 0) normalized = 0;
    if (normalized > 1) normalized = 1;

    double cx = width / 2.0;
    double cy = height / 2.0 - 6;
    double radius = (width < height ? width : height) / 2.0 - 8;
    if (radius < 8) radius = 8;

    /* Background arc (track) */
    double start_angle = 0.75 * M_PI;
    double end_angle = 2.25 * M_PI;

    cairo_set_line_width(cr, 3.0);
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_arc(cr, cx, cy, radius, start_angle, end_angle);
    cairo_stroke(cr);

    /* Value arc */
    double val_angle = start_angle + normalized * (end_angle - start_angle);
    cairo_set_source_rgb(cr, 0.2, 0.7, 0.9);
    cairo_arc(cr, cx, cy, radius, start_angle, val_angle);
    cairo_stroke(cr);

    /* Indicator dot */
    double dot_x = cx + radius * cos(val_angle);
    double dot_y = cy + radius * sin(val_angle);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_arc(cr, dot_x, dot_y, 3, 0, 2 * M_PI);
    cairo_fill(cr);

    /* Label */
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_set_font_size(cr, 9);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, k->label, &ext);
    cairo_move_to(cr, cx - ext.width / 2, height - 2);
    cairo_show_text(cr, k->label);

    /* Value text */
    char val_str[16];
    if (k->max - k->min > 100)
        snprintf(val_str, sizeof(val_str), "%.0f", val);
    else
        snprintf(val_str, sizeof(val_str), "%.2f", val);
    cairo_text_extents(cr, val_str, &ext);
    cairo_move_to(cr, cx - ext.width / 2, cy + 4);
    cairo_show_text(cr, val_str);
}

static void knob_drag_begin(GtkGestureDrag *gesture, double x, double y,
                            gpointer data)
{
    (void)gesture; (void)x;
    OxsKnob *k = (OxsKnob *)data;
    k->dragging = true;
    k->drag_start_y = y;
    k->drag_start_val = oxs_synth_get_param(k->synth, (uint32_t)k->param_id);
}

static void knob_drag_update(GtkGestureDrag *gesture, double offset_x,
                             double offset_y, gpointer data)
{
    (void)gesture; (void)offset_x;
    OxsKnob *k = (OxsKnob *)data;
    if (!k->dragging) return;

    /* Dragging up increases value, down decreases */
    float range = k->max - k->min;
    float delta = (float)(-offset_y / 150.0) * range;
    float new_val = k->drag_start_val + delta;
    if (new_val < k->min) new_val = k->min;
    if (new_val > k->max) new_val = k->max;

    oxs_synth_set_param(k->synth, (uint32_t)k->param_id, new_val);
    gtk_widget_queue_draw(GTK_WIDGET(k->area));
}

static void knob_drag_end(GtkGestureDrag *gesture, double x, double y,
                          gpointer data)
{
    (void)gesture; (void)x; (void)y;
    OxsKnob *k = (OxsKnob *)data;
    k->dragging = false;
}

static GtkWidget *create_knob_widget(oxs_synth_t *synth, int32_t param_id,
                                     const char *label)
{
    OxsKnob *k = g_new0(OxsKnob, 1);
    k->synth = synth;
    k->param_id = param_id;
    strncpy(k->label, label, sizeof(k->label) - 1);

    oxs_param_info_t info;
    if (oxs_synth_param_info(synth, (uint32_t)param_id, &info)) {
        k->min = info.min;
        k->max = info.max;
    } else {
        k->min = 0; k->max = 1;
    }

    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), 60);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), 72);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), knob_draw, k, g_free);
    k->area = GTK_DRAWING_AREA(area);

    /* Drag gesture for value changes */
    GtkGesture *drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin", G_CALLBACK(knob_drag_begin), k);
    g_signal_connect(drag, "drag-update", G_CALLBACK(knob_drag_update), k);
    g_signal_connect(drag, "drag-end", G_CALLBACK(knob_drag_end), k);
    gtk_widget_add_controller(area, GTK_EVENT_CONTROLLER(drag));

    return area;
}

/* ─── Envelope Display ───────────────────────────────────────────────────── */

typedef struct {
    oxs_synth_t *synth;
    int32_t a_id, d_id, s_id, r_id;
} OxsEnvDisplay;

static void env_draw(GtkDrawingArea *area, cairo_t *cr,
                     int width, int height, gpointer data)
{
    (void)area;
    OxsEnvDisplay *e = (OxsEnvDisplay *)data;

    float a = oxs_synth_get_param(e->synth, (uint32_t)e->a_id);
    float d = oxs_synth_get_param(e->synth, (uint32_t)e->d_id);
    float s = oxs_synth_get_param(e->synth, (uint32_t)e->s_id);
    float r = oxs_synth_get_param(e->synth, (uint32_t)e->r_id);

    /* Normalize times to fit width */
    float total = a + d + 0.3f + r; /* sustain hold = 0.3s for display */
    if (total < 0.01f) total = 0.01f;

    double pad = 4;
    double w = width - 2 * pad;
    double h = height - 2 * pad;

    double x_a = pad + (a / total) * w;
    double x_d = x_a + (d / total) * w;
    double x_s = x_d + (0.3f / total) * w;
    double x_r = x_s + (r / total) * w;
    double y_top = pad;
    double y_sus = pad + (1.0 - s) * h;
    double y_bot = pad + h;

    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.2, 0.7, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, pad, y_bot);
    cairo_line_to(cr, x_a, y_top);      /* attack */
    cairo_line_to(cr, x_d, y_sus);      /* decay */
    cairo_line_to(cr, x_s, y_sus);      /* sustain hold */
    cairo_line_to(cr, x_r, y_bot);      /* release */
    cairo_stroke(cr);
}

static GtkWidget *create_envelope_widget(oxs_synth_t *synth,
                                         int32_t a, int32_t d,
                                         int32_t s, int32_t r)
{
    OxsEnvDisplay *e = g_new0(OxsEnvDisplay, 1);
    e->synth = synth;
    e->a_id = a; e->d_id = d; e->s_id = s; e->r_id = r;

    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), 120);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), 60);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), env_draw, e, g_free);
    return area;
}

/* ─── Level Meter ────────────────────────────────────────────────────────── */

typedef struct {
    oxs_synth_t *synth;
    float peak_l, peak_r;
} OxsMeter;

static void meter_draw(GtkDrawingArea *area, cairo_t *cr,
                       int width, int height, gpointer data)
{
    (void)area;
    OxsMeter *m = (OxsMeter *)data;

    /* Drain output events to get latest peaks */
    oxs_output_event_t ev;
    while (oxs_synth_pop_output_event(m->synth, &ev)) {
        m->peak_l = ev.peak_l;
        m->peak_r = ev.peak_r;
    }

    /* Decay */
    m->peak_l *= 0.95f;
    m->peak_r *= 0.95f;

    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    double bar_w = (width - 6) / 2.0;
    double max_h = height - 4;

    /* Left meter */
    double h_l = m->peak_l * max_h;
    if (h_l > max_h) h_l = max_h;
    cairo_set_source_rgb(cr, 0.1, 0.8, 0.3);
    cairo_rectangle(cr, 2, height - 2 - h_l, bar_w, h_l);
    cairo_fill(cr);

    /* Right meter */
    double h_r = m->peak_r * max_h;
    if (h_r > max_h) h_r = max_h;
    cairo_rectangle(cr, 4 + bar_w, height - 2 - h_r, bar_w, h_r);
    cairo_fill(cr);
}

/* ─── Signal Callbacks ───────────────────────────────────────────────────── */

static void dropdown_changed_cb(GObject *obj, GParamSpec *pspec, gpointer data)
{
    (void)pspec; (void)data;
    int pid = GPOINTER_TO_INT(g_object_get_data(obj, "param_id"));
    oxs_synth_t *s = g_object_get_data(obj, "synth");
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
    oxs_synth_set_param(s, (uint32_t)pid, (float)sel);
}

static void toggle_changed_cb(GtkToggleButton *btn, gpointer data)
{
    (void)data;
    int pid = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "param_id"));
    oxs_synth_t *s = g_object_get_data(G_OBJECT(btn), "synth");
    float val = gtk_toggle_button_get_active(btn) ? 1.0f : 0.0f;
    oxs_synth_set_param(s, (uint32_t)pid, val);
}

/* ─── Layout Tree Walker ─────────────────────────────────────────────────── */

static GtkWidget *build_widget(const oxs_ui_widget_t *w, oxs_synth_t *synth);

static GtkWidget *build_group(const oxs_ui_widget_t *w, oxs_synth_t *synth)
{
    GtkWidget *frame = gtk_frame_new(w->label);
    GtkWidget *box;
    if (w->direction == OXS_UI_HORIZONTAL)
        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    else
        box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    gtk_widget_set_margin_start(box, 4);
    gtk_widget_set_margin_end(box, 4);
    gtk_widget_set_margin_top(box, 2);
    gtk_widget_set_margin_bottom(box, 2);

    for (int i = 0; i < w->num_children; i++) {
        GtkWidget *child = build_widget(w->children[i], synth);
        if (child) gtk_box_append(GTK_BOX(box), child);
    }

    gtk_frame_set_child(GTK_FRAME(frame), box);
    return frame;
}

static GtkWidget *build_widget(const oxs_ui_widget_t *w, oxs_synth_t *synth)
{
    switch (w->type) {
    case OXS_UI_GROUP:
        return build_group(w, synth);

    case OXS_UI_KNOB:
        return create_knob_widget(synth, w->param_id, w->label);

    case OXS_UI_DROPDOWN: {
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *label = gtk_label_new(w->label);
        gtk_box_append(GTK_BOX(box), label);

        /* Use GtkDropDown with string list */
        const char *strings[OXS_UI_MAX_OPTIONS + 1];
        for (int i = 0; i < w->num_options; i++)
            strings[i] = w->options[i].label;
        strings[w->num_options] = NULL;

        GtkWidget *dd = gtk_drop_down_new_from_strings(strings);
        int cur = (int)oxs_synth_get_param(synth, (uint32_t)w->param_id);
        if (cur >= 0 && cur < w->num_options)
            gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), (guint)cur);

        /* Store param_id as widget data for change callback */
        g_object_set_data(G_OBJECT(dd), "param_id",
                          GINT_TO_POINTER(w->param_id));
        g_object_set_data(G_OBJECT(dd), "synth", synth);
        g_signal_connect(dd, "notify::selected",
            G_CALLBACK(dropdown_changed_cb), NULL);

        gtk_box_append(GTK_BOX(box), dd);
        return box;
    }

    case OXS_UI_TOGGLE: {
        GtkWidget *btn = gtk_toggle_button_new_with_label(w->label);
        bool cur = oxs_synth_get_param(synth, (uint32_t)w->param_id) > 0.5f;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), cur);

        g_object_set_data(G_OBJECT(btn), "param_id",
                          GINT_TO_POINTER(w->param_id));
        g_object_set_data(G_OBJECT(btn), "synth", synth);
        g_signal_connect(btn, "toggled",
            G_CALLBACK(toggle_changed_cb), NULL);
        return btn;
    }

    case OXS_UI_LABEL:
        return gtk_label_new(w->label);

    case OXS_UI_ENVELOPE:
        return create_envelope_widget(synth,
            w->env_attack_id, w->env_decay_id,
            w->env_sustain_id, w->env_release_id);

    case OXS_UI_METER: {
        OxsMeter *m = g_new0(OxsMeter, 1);
        m->synth = synth;
        GtkWidget *area = gtk_drawing_area_new();
        gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), 30);
        gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), 80);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), meter_draw, m, g_free);
        return area;
    }

    case OXS_UI_WAVEFORM:
    case OXS_UI_PRESET_BROWSER:
    case OXS_UI_KEYBOARD:
        /* Placeholder for now */
        return gtk_label_new(w->label);

    default:
        return gtk_label_new("?");
    }
}

/* ─── Timer for UI refresh ───────────────────────────────────────────────── */

static gboolean ui_refresh(gpointer data)
{
    (void)data;
    /* Queue redraw on all drawing areas — GTK handles invalidation */
    if (g_app.window && gtk_widget_get_visible(g_app.window)) {
        gtk_widget_queue_draw(g_app.window);
    }
    return G_SOURCE_CONTINUE;
}

/* ─── App Activate ───────────────────────────────────────────────────────── */

static void on_activate(GtkApplication *app, gpointer data)
{
    (void)data;

    /* Build layout tree */
    const oxs_ui_layout_t *layout = oxs_ui_build_layout();

    /* Create main window */
    g_app.window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_app.window), "0xSYNTH");
    gtk_window_set_default_size(GTK_WINDOW(g_app.window), 900, 700);

    /* Dark theme */
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, NULL);

    /* Scrollable container for the layout */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* Build widget tree from layout */
    GtkWidget *root_widget = build_widget(layout->root, g_app.synth);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), root_widget);
    gtk_window_set_child(GTK_WINDOW(g_app.window), scroll);

    /* Start UI refresh timer (30fps) */
    g_app.timer_id = g_timeout_add(33, ui_refresh, NULL);

    gtk_window_present(GTK_WINDOW(g_app.window));
}

/* ─── Public API ─────────────────────────────────────────────────────────── */

int oxs_gtk_run(oxs_synth_t *synth, int argc, char *argv[])
{
    g_app.synth = synth;
    g_app.app = gtk_application_new("com.0xsynth.standalone",
                                     G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(g_app.app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(g_app.app), argc, argv);

    if (g_app.timer_id) g_source_remove(g_app.timer_id);
    g_object_unref(g_app.app);

    return status;
}
