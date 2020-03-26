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


/* Handler for configure_event on drawing area. */
static void mag_configuration_changed (LXPanel *panel, GtkWidget *p)
{
    MagnifierPlugin *mag = lxpanel_plugin_get_data (p);
}

void run_magnifier (MagnifierPlugin *mag)
{
    // create the command line arguments
    char *args[12];
    int arg = 0;
    args[arg++] = strdup ("mouseloupe");

    if (mag->shape == 0)
    {
        args[arg++] = strdup ("-c");
        args[arg++] = g_strdup_printf ("%d", mag->width);
    }
    else
    {
        args[arg++] = strdup ("-r");
        args[arg++] = g_strdup_printf ("%d", mag->width);
        args[arg++] = g_strdup_printf ("%d", mag->height);
    }

    args[arg++] = strdup ("-z");
    args[arg++] = g_strdup_printf ("%d", mag->zoom);

    if (mag->statwin) args[arg++] = strdup ("-s");
    if (mag->followf) args[arg++] = strdup ("-m");
    if (mag->followt) args[arg++] = strdup ("-t");
    if (mag->filter) args[arg++] = strdup ("-f");
    args[arg] = NULL;

    execv ("/usr/bin/mouseloupe", args);
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

/* Plugin constructor. */
static GtkWidget *mag_constructor (LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    MagnifierPlugin *mag = g_new0 (MagnifierPlugin, 1);

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    /* Create icon */
    mag->tray_icon = gtk_image_new ();
    set_icon (panel, mag->tray_icon, "system-search", 0);
    gtk_widget_set_tooltip_text (mag->tray_icon, _("Show virtual magnifier"));
    gtk_widget_set_visible (mag->tray_icon, TRUE);

    /* Allocate top level widget and set into Plugin widget pointer. */
    mag->panel = panel;
    mag->plugin = gtk_button_new ();
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
};
