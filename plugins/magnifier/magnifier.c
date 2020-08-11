/*
 * Magnifier plugin for LXPanel
 *
 * Copyright for relevant code as for LXPanel
 *
 */

/*
Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib/gi18n.h>

#include "plugin.h"

#define MAG_PROG "/usr/bin/mage"

#define BOUNDS(var,min,max) if (var < min) var = min; if (var > max) var = max;
#define READ_VAL(name,var,low,high,def) if (config_setting_lookup_int (settings, name, &val) && val >= low && val <= high) var = val; else var = def;
#define ADD_ARG(...) args[arg++] = g_strdup_printf (__VA_ARGS__)

/* Private context for plugin */

typedef struct
{
    GtkWidget *plugin;              /* Back pointer to the widget */
    LXPanel *panel;                 /* Back pointer to panel */
    GtkWidget *tray_icon;           /* Displayed image */
    config_setting_t *settings;     /* Plugin settings */
    int pid;                        /* PID for magnifier executable */
    int shape;
    int width;
    int height;
    int zoom;
    int x;
    int y;
    gboolean statwin;
    gboolean followf;
    gboolean followt;
    gboolean filter;
    gboolean restart;
} MagnifierPlugin;

static void run_magnifier (MagnifierPlugin *mag);

static void magnifier_closed_cb (GPid pid, gint status, gpointer user_data)
{
    MagnifierPlugin *mag = (MagnifierPlugin *) user_data;

    mag->pid = -1;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mag->plugin), FALSE);
    g_spawn_close_pid (pid);
    if (mag->restart)
    {
        mag->restart = FALSE;
        run_magnifier (mag);
    }
}

static void run_magnifier (MagnifierPlugin *mag)
{
    // create the command line argument array
    char *args[16];
    int arg = 0;

    ADD_ARG (MAG_PROG);

    if (mag->shape)
    {
        ADD_ARG ("-r");
        ADD_ARG ("%d", mag->width);
        ADD_ARG ("%d", mag->height);
    }
    else
    {
        ADD_ARG ("-c");
        ADD_ARG ("%d", mag->width);
    }

    ADD_ARG ("-z");
    ADD_ARG ("%d", mag->zoom);

    if (mag->statwin)
    {
        if (mag->x < 0) mag->x = 0;
        if (mag->y < 0) mag->y = 0;
        ADD_ARG ("-s");
        ADD_ARG ("%d", mag->x);
        ADD_ARG ("%d", mag->y);
    }

    if (mag->followf) ADD_ARG ("-m");
    if (mag->followt) ADD_ARG ("-t");
    if (mag->filter) ADD_ARG ("-f");
    args[arg] = NULL;

    // launch the magnifier with the argument array
    g_spawn_async (NULL, args, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &mag->pid, NULL);
    g_child_watch_add (mag->pid, magnifier_closed_cb, mag);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mag->plugin), TRUE);
}

static void toggle_magnifier (MagnifierPlugin *mag)
{
    if (mag->pid == -1) run_magnifier (mag);
    else kill (mag->pid, SIGTERM);
}

/* Handler for configure_event on drawing area. */
static void mag_configuration_changed (LXPanel *panel, GtkWidget *p)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (p);
    lxpanel_plugin_set_taskbar_icon (panel, mag->tray_icon, "system-search");
}

/* Handler for menu button click */
static gboolean mag_button_press_event (GtkWidget *widget, GdkEventButton *event, LXPanel *panel)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (widget);

#ifdef ENABLE_NLS
    textdomain (GETTEXT_PACKAGE);
#endif

    /* Launch or kill the magnifier application on left-click */
    if (event->button == 1)
    {
        toggle_magnifier (mag);
        return TRUE;
    }
    else return FALSE;
}

/* Handler for mouse scroll */
static void mag_mouse_scrolled (GtkScale *scale, GdkEventScroll *evt, MagnifierPlugin *mag)
{
    if (mag->pid == -1) return;

    if (evt->direction == GDK_SCROLL_UP || evt->direction == GDK_SCROLL_LEFT)
    {
        if (mag->zoom < 16) mag->zoom += 1;
    }
    else
    {
        if (mag->zoom > 2) mag->zoom -= 1;
    }
    config_group_set_int (mag->settings, "Zoom", mag->zoom);
    lxpanel_config_save (mag->panel);
    mag->restart = TRUE;
    kill (mag->pid, SIGTERM);
}

/* Handler for control message from panel */
static gboolean mag_control_msg (GtkWidget *plugin, const char *cmd)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (plugin);

    if (!strncmp (cmd, "pos", 3))
    {
        // get the location of the topmost window, which is the loupe
        Window rootwin, root, nullwd, *children;
        int nwins, null, scr;
        Display *dsp;

        dsp = XOpenDisplay (NULL);
        scr = DefaultScreen (dsp);
        rootwin = RootWindow (dsp, scr);
        XQueryTree (dsp, rootwin, &root, &nullwd, &children, &nwins);
        XGetGeometry (dsp, children[nwins - 1], &root, &mag->x, &mag->y, &null, &null, &null, &null);
        config_group_set_int (mag->settings, "StatX", mag->x);
        config_group_set_int (mag->settings, "StatY", mag->y);
        lxpanel_config_save (mag->panel);
        return TRUE;
    }

    if (!strncmp (cmd, "toggle", 6))
    {
        toggle_magnifier (mag);
        return TRUE;
    }

    return FALSE;
}

/* Handler for configuration changed message from config dialog */
static gboolean mag_apply_configuration (gpointer user_data)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data ((GtkWidget *) user_data);

    config_group_set_int (mag->settings, "Shape", mag->shape);
    config_group_set_int (mag->settings, "Width", mag->width);
    config_group_set_int (mag->settings, "Height", mag->height);
    config_group_set_int (mag->settings, "Zoom", mag->zoom);
    config_group_set_int (mag->settings, "StaticWin", mag->statwin);
    config_group_set_int (mag->settings, "FollowText", mag->followt);
    config_group_set_int (mag->settings, "FollowFocus", mag->followf);
    config_group_set_int (mag->settings, "UseFilter", mag->filter);

    // check bounds
    BOUNDS (mag->zoom, 2, 16);
    if (mag->shape == 0)
    {
        BOUNDS (mag->width, 100, 600);
    }
    else
    {
        BOUNDS (mag->width, 100, 800);
        BOUNDS (mag->height, 50, 600);
    }

    if (mag->pid != -1)
    {
        mag->restart = TRUE;
        kill (mag->pid, SIGTERM);
    }
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *mag_configure (LXPanel *panel, GtkWidget *p)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (p);

#ifdef ENABLE_NLS
    textdomain (GETTEXT_PACKAGE);
#endif

    return lxpanel_generic_config_dlg (_("Virtual Magnifier"), panel,
        mag_apply_configuration, p,
        _("Circle"), &mag->shape, CONF_TYPE_RBUTTON,
        _("Rectangle"), &mag->shape, CONF_TYPE_RBUTTON,
        _("Width"), &mag->width, CONF_TYPE_INT,
        _("Height"), &mag->height, CONF_TYPE_INT,
        _("Zoom"), &mag->zoom, CONF_TYPE_INT,
        _("Static window"), &mag->statwin, CONF_TYPE_BOOL,
        _("Follow focus"), &mag->followf, CONF_TYPE_BOOL,
        _("Follow text cursor"), &mag->followt, CONF_TYPE_BOOL,
        _("Bilinear filter"), &mag->filter, CONF_TYPE_BOOL,
        NULL);
}

/* Plugin destructor. */
static void mag_destructor (gpointer user_data)
{
    MagnifierPlugin *mag = (MagnifierPlugin *) user_data;

    /* Deallocate memory. */
    g_free (mag);
}

/* Plugin constructor. */
static GtkWidget *mag_constructor (LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    MagnifierPlugin *mag = g_new0 (MagnifierPlugin, 1);
    int val;
    mag->panel = panel;
    mag->settings = settings;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    READ_VAL ("Shape", mag->shape, 0, 1, 1);
    READ_VAL ("Zoom", mag->zoom, 2, 16, 2);
    READ_VAL ("Width", mag->width, 100, 600, 350);
    READ_VAL ("Height", mag->height, 100, 600, 350);
    READ_VAL ("StaticWin", mag->statwin, 0, 1, 0);
    READ_VAL ("FollowFocus", mag->followf, 0, 1, 0);
    READ_VAL ("FollowText", mag->followt, 0, 1, 0);
    READ_VAL ("UseFilter", mag->filter, 0, 1, 0);
    READ_VAL ("StatX", mag->x, 0, 2000, 0);
    READ_VAL ("StatY", mag->y, 0, 2000, 0);

    if (access (MAG_PROG, F_OK) != -1)
    {
        /* Allocate top level widget and set into Plugin widget pointer. */
        mag->plugin = gtk_toggle_button_new ();
        gtk_button_set_relief (GTK_BUTTON (mag->plugin), GTK_RELIEF_NONE);
        g_signal_connect (mag->plugin, "scroll-event", G_CALLBACK (mag_mouse_scrolled), mag);

        /* Allocate icon as a child of top level */
        mag->tray_icon = gtk_image_new ();
        lxpanel_plugin_set_taskbar_icon (panel, mag->tray_icon, "system-search");
        gtk_widget_set_tooltip_text (mag->tray_icon, _("Show virtual magnifier"));
        gtk_widget_set_visible (mag->tray_icon, TRUE);
        gtk_container_add (GTK_CONTAINER (mag->plugin), mag->tray_icon);

        mag->pid = -1;
        mag->restart = FALSE;
    }
    else
    {
        /* a NULL label has a width of zero; unlike an empty button... */
        mag->plugin = gtk_label_new (NULL);
    }

    lxpanel_plugin_set_data (mag->plugin, mag, mag_destructor);
    return mag->plugin;
}

FM_DEFINE_MODULE(lxpanel_gtk, magnifier)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Magnifier"),
    .description = N_("Virtual magnifying glass"),
    .new_instance = mag_constructor,
    .config = mag_configure,
    .reconfigure = mag_configuration_changed,
    .button_press_event = mag_button_press_event,
    .control = mag_control_msg,
    .gettext_package = GETTEXT_PACKAGE
};
