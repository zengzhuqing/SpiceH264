/* Copyright (C) 2011 Red Hat, Inc. */

/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdint.h>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#endif

#include "spice-controller.h"

SpiceCtrlController *ctrl = NULL;
SpiceCtrlForeignMenu *menu = NULL;
GMainLoop *loop = NULL;

void signaled (GObject *gobject, const gchar *signal_name)
{
    g_message ("signaled: %s", signal_name);
}

void notified (GObject *gobject, GParamSpec *pspec,
               gpointer user_data)
{
    GValue value = { 0, };
    GValue strvalue = { 0, };

    g_return_if_fail (gobject != NULL);
    g_return_if_fail (pspec != NULL);

    g_value_init (&value, pspec->value_type);
    g_value_init (&strvalue, G_TYPE_STRING);
    g_object_get_property (gobject, pspec->name, &value);

    if (pspec->value_type == G_TYPE_STRV) {
      gchar** p = (gchar **)g_value_get_boxed (&value);
      g_message ("notify::%s == ", pspec->name);
      while (*p)
        g_message ("%s", *p++);
    } else if (G_TYPE_IS_OBJECT(pspec->value_type)) {
      GObject *o = g_value_get_object (&value);
      g_message ("notify::%s == %s", pspec->name, o ? G_OBJECT_TYPE_NAME (o) : "null");
    } else {
      g_value_transform (&value, &strvalue);
      g_message ("notify::%s  = %s", pspec->name, g_value_get_string (&strvalue));
    }

    g_value_unset (&value);
    g_value_unset (&strvalue);
}

void connect_signals (gpointer obj)
{
    guint i, n_ids = 0;
    guint *ids = NULL;
    GType type = G_OBJECT_TYPE (obj);

    ids = g_signal_list_ids (type, &n_ids);
    for (i = 0; i < n_ids; i++) {
        const gchar *name = g_signal_name (ids[i]);
        g_signal_connect (obj, name, G_CALLBACK (signaled), (gpointer)name);
    }
}

int main (int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2,36,0)
    g_type_init ();
#endif
    loop = g_main_loop_new (NULL, FALSE);

    if (argc > 1 && g_str_equal(argv[1], "--menu")) {
        menu = spice_ctrl_foreign_menu_new ();
        g_signal_connect (menu, "notify", G_CALLBACK (notified), NULL);
        connect_signals (menu);

        spice_ctrl_foreign_menu_listen (menu, NULL, NULL, NULL);
    } else {
        ctrl = spice_ctrl_controller_new ();
        g_signal_connect (ctrl, "notify", G_CALLBACK (notified), NULL);
        connect_signals (ctrl);

        spice_ctrl_controller_listen (ctrl, NULL, NULL, NULL);
    }

    g_main_loop_run (loop);

    if (ctrl != NULL)
        g_object_unref (ctrl);
    if (menu != NULL)
        g_object_unref (menu);

    return 0;
}

