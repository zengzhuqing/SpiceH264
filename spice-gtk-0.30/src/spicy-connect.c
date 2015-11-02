/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010-2015 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "spice-common.h"
#include "spicy-connect.h"

typedef struct
{
    gboolean connecting;
    GMainLoop *loop;
    SpiceSession *session;
} ConnectionInfo;

static struct {
    const char *text;
    const char *prop;
    GtkWidget *entry;
} connect_entries[] = {
    { .text = "Hostname",   .prop = "host"      },
    { .text = "Port",       .prop = "port"      },
    { .text = "TLS Port",   .prop = "tls-port"  },
};

static gboolean can_connect(void)
{
    if ((gtk_entry_get_text_length(GTK_ENTRY(connect_entries[0].entry)) > 0) &&
        ((gtk_entry_get_text_length(GTK_ENTRY(connect_entries[1].entry)) > 0) ||
         (gtk_entry_get_text_length(GTK_ENTRY(connect_entries[2].entry)) > 0)))
        return TRUE;

    return FALSE;
}

static void set_connection_info(SpiceSession *session)
{
    const gchar *txt;
    int i;

    for (i = 0; i < SPICE_N_ELEMENTS(connect_entries); i++) {
        txt = gtk_entry_get_text(GTK_ENTRY(connect_entries[i].entry));
        g_object_set(session, connect_entries[i].prop, txt, NULL);
    }
}

static gboolean close_cb(gpointer data)
{
    ConnectionInfo *info = data;
    info->connecting = FALSE;
    if (g_main_loop_is_running(info->loop))
        g_main_loop_quit(info->loop);

    return TRUE;
}

static void entry_changed_cb(GtkEditable* entry, gpointer data)
{
    GtkButton *connect_button = data;
    gtk_widget_set_sensitive(GTK_WIDGET(connect_button), can_connect());
}

static gboolean entry_focus_in_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    GtkRecentChooser *recent = GTK_RECENT_CHOOSER(data);
    gtk_recent_chooser_unselect_all(recent);
    return TRUE;
}

static gboolean key_pressed_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gboolean tst;
    if (event->type == GDK_KEY_PRESS) {
        switch (event->key.keyval) {
            case GDK_KEY_Escape:
                g_signal_emit_by_name(GTK_WIDGET(data), "delete-event", NULL, &tst);
                return TRUE;
            default:
                return FALSE;
        }
    }

    return FALSE;
}

static void recent_selection_changed_dialog_cb(GtkRecentChooser *chooser, gpointer data)
{
    GtkRecentInfo *info;
    gchar *txt = NULL;
    const gchar *uri;
    SpiceSession *session = data;
    int i;

    info = gtk_recent_chooser_get_current_item(chooser);
    if (info == NULL)
        return;

    uri = gtk_recent_info_get_uri(info);
    g_return_if_fail(uri != NULL);

    g_object_set(session, "uri", uri, NULL);

    for (i = 0; i < SPICE_N_ELEMENTS(connect_entries); i++) {
        g_object_get(session, connect_entries[i].prop, &txt, NULL);
        gtk_entry_set_text(GTK_ENTRY(connect_entries[i].entry), txt ? txt : "");
        g_free(txt);
    }

    gtk_recent_info_unref(info);
}

static void connect_cb(gpointer data)
{
    ConnectionInfo *info = data;
    if (can_connect())
    {
        info->connecting = TRUE;
        set_connection_info(info->session);
        if (g_main_loop_is_running(info->loop))
            g_main_loop_quit(info->loop);
    }
}

gboolean spicy_connect_dialog(SpiceSession *session)
{
    GtkWidget *connect_button, *cancel_button, *label;
    GtkBox *main_box, *recent_box, *button_box;
    GtkWindow *window;
    GtkTable *table;
    int i;

    ConnectionInfo info = {
        FALSE,
        NULL,
        session
    };

    /* Create the widgets */
    window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(window, "Connect to SPICE");
    gtk_window_set_resizable(window, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);

    main_box = GTK_BOX(gtk_vbox_new(FALSE, 0));
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(main_box));

    table = GTK_TABLE(gtk_table_new(3, 2, 0));
    gtk_box_pack_start(main_box, GTK_WIDGET(table), FALSE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
    gtk_table_set_row_spacings(table, 5);
    gtk_table_set_col_spacings(table, 5);

    for (i = 0; i < SPICE_N_ELEMENTS(connect_entries); i++) {
        gchar *txt;
        label = gtk_label_new(connect_entries[i].text);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_table_attach_defaults(table, label, 0, 1, i, i+1);
        connect_entries[i].entry = GTK_WIDGET(gtk_entry_new());
        gtk_table_attach_defaults(table, connect_entries[i].entry, 1, 2, i, i+1);
        g_object_get(session, connect_entries[i].prop, &txt, NULL);
        SPICE_DEBUG("%s: #%i [%s]: \"%s\"",
                __FUNCTION__, i, connect_entries[i].prop, txt);
        if (txt) {
            gtk_entry_set_text(GTK_ENTRY(connect_entries[i].entry), txt);
            g_free(txt);
        }
    }

    recent_box = GTK_BOX(gtk_vbox_new(FALSE, 0));
    gtk_box_pack_start(main_box, GTK_WIDGET(recent_box), TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(recent_box), 5);

    label = gtk_label_new("Recent connections:");
    gtk_box_pack_start(recent_box, label, FALSE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    button_box = GTK_BOX(gtk_hbutton_box_new());
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(button_box, 5);
    gtk_container_set_border_width(GTK_CONTAINER(button_box), 5);
    connect_button = gtk_button_new_with_label("Connect");
    cancel_button = gtk_button_new_with_label("Cancel");
    gtk_box_pack_start(button_box, cancel_button, FALSE, TRUE, 0);
    gtk_box_pack_start(button_box, connect_button, FALSE, TRUE, 1);

    gtk_box_pack_start(main_box, GTK_WIDGET(button_box), FALSE, TRUE, 0);

    gtk_widget_set_sensitive(GTK_WIDGET(connect_button), can_connect());

    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(key_pressed_cb), window);
    g_signal_connect_swapped(window, "delete-event",
                             G_CALLBACK(close_cb), &info);
    g_signal_connect_swapped(connect_button, "clicked",
                             G_CALLBACK(connect_cb), &info);
    g_signal_connect_swapped(cancel_button, "clicked",
                             G_CALLBACK(close_cb), &info);

    GtkRecentFilter *rfilter;
    GtkWidget *recent;

    recent = GTK_WIDGET(gtk_recent_chooser_widget_new());
    gtk_recent_chooser_set_show_icons(GTK_RECENT_CHOOSER(recent), FALSE);
    gtk_box_pack_start(recent_box, recent, TRUE, TRUE, 0);

    rfilter = gtk_recent_filter_new();
    gtk_recent_filter_add_mime_type(rfilter, "application/x-spice");
    gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recent), rfilter);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(recent), FALSE);
    g_signal_connect(recent, "selection-changed",
                     G_CALLBACK(recent_selection_changed_dialog_cb), session);
    g_signal_connect_swapped(recent, "item-activated",
                             G_CALLBACK(connect_cb), &info);

    for (i = 0; i < SPICE_N_ELEMENTS(connect_entries); i++) {
        g_signal_connect_swapped(connect_entries[i].entry, "activate",
                                 G_CALLBACK(connect_cb), &info);
        g_signal_connect(connect_entries[i].entry, "changed",
                         G_CALLBACK(entry_changed_cb), connect_button);
        g_signal_connect(connect_entries[i].entry, "focus-in-event",
                         G_CALLBACK(entry_focus_in_cb), recent);
    }

    /* show and wait for response */
    gtk_widget_show_all(GTK_WIDGET(window));

    info.loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(info.loop);

    gtk_widget_destroy(GTK_WIDGET(window));

    return info.connecting;
}
