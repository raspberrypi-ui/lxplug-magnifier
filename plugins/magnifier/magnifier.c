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

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "plugin.h"

#define MAG_PROG "magnifier"

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
} MagnifierPlugin;


#define ICON_BUTTON_TRIM 4

static void set_icon (LXPanel *p, GtkWidget *image, const char *icon, int size)
{
    GdkPixbuf *pixbuf;
    if (size == 0) size = panel_get_icon_size (p) - ICON_BUTTON_TRIM;
    if (gtk_icon_theme_has_icon (panel_get_icon_theme (p), icon))
    {
        GtkIconInfo *info = gtk_icon_theme_lookup_icon (panel_get_icon_theme (p), icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);
        pixbuf = gtk_icon_info_load_icon (info, NULL);
        gtk_icon_info_free (info);
        if (pixbuf != NULL)
        {
            gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
            g_object_unref (pixbuf);
            return;
        }
    }
    else
    {
        char path[256];
        sprintf (path, "%s/images/%s.png", PACKAGE_DATA_DIR, icon);
        pixbuf = gdk_pixbuf_new_from_file_at_scale (path, size, size, TRUE, NULL);
        if (pixbuf != NULL)
        {
            gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
            g_object_unref (pixbuf);
        }
    }
}


void run_magnifier (MagnifierPlugin *mag)
{
    // create the command line arguments
    char *args[16];
    int arg = 0;
    args[arg++] = strdup (MAG_PROG);

    if (mag->shape)
    {
        args[arg++] = strdup ("-r");
        args[arg++] = g_strdup_printf ("%d", mag->width);
        args[arg++] = g_strdup_printf ("%d", mag->height);
    }
    else
    {
        args[arg++] = strdup ("-c");
        args[arg++] = g_strdup_printf ("%d", mag->width);
    }

    args[arg++] = strdup ("-z");
    args[arg++] = g_strdup_printf ("%d", mag->zoom);

    if (mag->statwin)
    {
        args[arg++] = strdup ("-s");
        if (mag->x < 0) mag->x = 0;
        if (mag->y < 0) mag->y = 0;
        args[arg++] = g_strdup_printf ("%d", mag->x);
        args[arg++] = g_strdup_printf ("%d", mag->y);
    }

    if (mag->followf) args[arg++] = strdup ("-m");
    if (mag->followt) args[arg++] = strdup ("-t");
    if (mag->filter) args[arg++] = strdup ("-f");
    args[arg] = NULL;

    // launch the magnifier with the argument array
    execv ("/usr/bin/" MAG_PROG, args);

    // free the array
    arg = 0;
    while (args[arg]) g_free (args[arg++]);
}


/* Handler for configure_event on drawing area. */
static void mag_configuration_changed (LXPanel *panel, GtkWidget *p)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (p);
}


/* Handler for menu button click */
static gboolean mag_button_press_event (GtkWidget *widget, GdkEventButton *event, LXPanel *panel)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (widget);
    int status;

#ifdef ENABLE_NLS
    textdomain (GETTEXT_PACKAGE);
#endif
    /* Launch or kill the magnifier application on left-click */
    if (event->button == 1)
    {
        if (mag->pid == -1)
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mag->plugin), TRUE);
            mag->pid = fork ();

            if (mag->pid == 0)
            {
                // new child process
                run_magnifier (mag);
                exit (0);
            }
        }
        else
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mag->plugin), FALSE);
            kill (mag->pid, SIGTERM);
            mag->pid = -1;
        }
        return TRUE;
    }
    else return FALSE;
}

/* Plugin destructor. */
static void mag_destructor (gpointer user_data)
{
    MagnifierPlugin *mag = (MagnifierPlugin *) user_data;

    /* Deallocate memory. */
    g_free (mag);
}


#define READ_VAL(name,var,low,high,def) if (config_setting_lookup_int (settings, name, &val) && val >= low && val <= high) var = val; else var = def;

/* Plugin constructor. */
static GtkWidget *mag_constructor (LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    MagnifierPlugin *mag = g_new0 (MagnifierPlugin, 1);
    int val;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    READ_VAL ("Shape", mag->shape, 0, 1, 0);
    READ_VAL ("Zoom", mag->zoom, 2, 16, 2);
    READ_VAL ("Width", mag->width, 100, 600, 350);
    READ_VAL ("Height", mag->height, 100, 600, 350);
    READ_VAL ("StaticWin", mag->statwin, 0, 1, 0);
    READ_VAL ("FollowFocus", mag->followf, 0, 1, 0);
    READ_VAL ("FollowText", mag->followt, 0, 1, 0);
    READ_VAL ("UseFilter", mag->filter, 0, 1, 0);
    READ_VAL ("StatX", mag->x, 0, 2000, 0);
    READ_VAL ("StatY", mag->y, 0, 2000, 0);

    /* Create icon */
    mag->tray_icon = gtk_image_new ();
    set_icon (panel, mag->tray_icon, "system-search", 0);
    gtk_widget_set_tooltip_text (mag->tray_icon, _("Show virtual magnifier"));
    gtk_widget_set_visible (mag->tray_icon, TRUE);

    /* Allocate top level widget and set into Plugin widget pointer. */
    mag->panel = panel;
    mag->plugin = gtk_toggle_button_new ();
    gtk_button_set_relief (GTK_BUTTON (mag->plugin), GTK_RELIEF_NONE);
    g_signal_connect (mag->plugin, "button-press-event", G_CALLBACK (mag_button_press_event), NULL);
    mag->settings = settings;
    lxpanel_plugin_set_data (mag->plugin, mag, mag_destructor);
    gtk_widget_add_events (mag->plugin, GDK_BUTTON_PRESS_MASK);

    /* Allocate icon as a child of top level */
    gtk_container_add (GTK_CONTAINER (mag->plugin), mag->tray_icon);

    mag->pid = -1;

    return mag->plugin;
}

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

    if (mag->pid != -1)
    {
        kill (mag->pid, SIGTERM);
        mag->pid = fork ();
        if (mag->pid == 0)
        {
            // new child process
            run_magnifier (mag);
            exit (0);
        }
    }
}

static gboolean mag_control_msg (GtkWidget *plugin, const char *cmd)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (plugin);

    if (!strncmp (cmd, "pos", 3))
    {
        // get the location of the topmost window, which is the loupe
        Window root, nullwd, *children;
        int nwins, null;
        Display *dsp = XOpenDisplay (NULL);
        int scr = DefaultScreen (dsp);
        Window rootwin = RootWindow (dsp, scr);
        XQueryTree (dsp, rootwin, &root, &nullwd, &children, &nwins);
        XGetGeometry (dsp, children[nwins - 1], &root, &mag->x, &mag->y, &null, &null, &null, &null);
        config_group_set_int (mag->settings, "StatX", mag->x);
        config_group_set_int (mag->settings, "StatY", mag->y);
        lxpanel_config_save (mag->panel);
        return TRUE;
    }

    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *mag_configure (LXPanel *panel, GtkWidget *p)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (p);
    
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

FM_DEFINE_MODULE(lxpanel_gtk, magnifier)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Magnifier"),
    .config = mag_configure,
    .description = N_("Virtual magnifying glass"),
    .new_instance = mag_constructor,
    .reconfigure = mag_configuration_changed,
    .control = mag_control_msg,
};
