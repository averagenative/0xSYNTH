/*
 * 0xSYNTH GTK 4 Application
 *
 * Creates the main window from the UI layout tree.
 * Binds widgets to synth params via synth_api.h.
 */

#ifndef OXS_GTK_APP_H
#define OXS_GTK_APP_H

#include <gtk/gtk.h>
#include "../api/synth_api.h"

/* Launch the GTK 4 application.
 * This blocks until the window is closed. */
int oxs_gtk_run(oxs_synth_t *synth, int argc, char *argv[]);

#endif /* OXS_GTK_APP_H */
