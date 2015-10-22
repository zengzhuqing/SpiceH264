/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

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

#include <glib.h>
#include <glib/gstdio.h>

#include "spice-controller-listener.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include "namedpipe.h"
#include "namedpipelistener.h"
#include "win32-util.h"
#endif

#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#endif

/**
 * SpiceControllerListenerError:
 * @SPICE_CONTROLLER_LISTENER_ERROR_VALUE: invalid value.
 *
 * Possible errors of controller listener related functions.
 **/

/**
 * SPICE_CONTROLLER_LISTENER_ERROR:
 *
 * The error domain of the controller listener subsystem.
 **/
GQuark
spice_controller_listener_error_quark (void)
{
  return g_quark_from_static_string ("spice-controller-listener-error");
}

GObject*
spice_controller_listener_new (const gchar *address, GError **error)
{
    GObject *listener = NULL;
    gchar *addr = NULL;

    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    addr = g_strdup (address);

#ifdef G_OS_WIN32
    if (addr == NULL)
        addr = g_strdup (g_getenv ("SPICE_XPI_NAMEDPIPE"));
    if (addr == NULL)
        addr = g_strdup_printf ("\\\\.\\pipe\\SpiceController-%" G_GUINT64_FORMAT, (guint64)GetCurrentProcessId ());
#else
    if (addr == NULL)
        addr = g_strdup (g_getenv ("SPICE_XPI_SOCKET"));
#endif
    if (addr == NULL) {
        g_set_error (error,
                     SPICE_CONTROLLER_LISTENER_ERROR,
                     SPICE_CONTROLLER_LISTENER_ERROR_VALUE,
#ifdef G_OS_WIN32
                     "Missing namedpipe address"
#else
                     "Missing socket address"
#endif
                     );
        goto end;
    }

    g_unlink (addr);

#ifdef G_OS_WIN32
    {
        SpiceNamedPipe *np;

        listener = G_OBJECT (spice_named_pipe_listener_new ());

        np = spice_win32_user_pipe_new (addr, error);
        if (!np) {
            g_object_unref (listener);
            listener = NULL;
            goto end;
        }

        spice_named_pipe_listener_add_named_pipe (SPICE_NAMED_PIPE_LISTENER (listener), np);
    }
#else
    {
        listener = G_OBJECT (g_socket_listener_new ());

        if (!g_socket_listener_add_address (G_SOCKET_LISTENER (listener),
                                            G_SOCKET_ADDRESS (g_unix_socket_address_new (addr)),
                                            G_SOCKET_TYPE_STREAM,
                                            G_SOCKET_PROTOCOL_DEFAULT,
                                            NULL,
                                            NULL,
                                            error))
            g_warning ("failed to add address");
    }
#endif

end:
    g_free (addr);
    return listener;
}

void
spice_controller_listener_accept_async (GObject *listener,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
    g_return_if_fail(G_IS_OBJECT(listener));

#ifdef G_OS_WIN32
    spice_named_pipe_listener_accept_async (SPICE_NAMED_PIPE_LISTENER (listener), cancellable, callback, user_data);
#else
    g_socket_listener_accept_async (G_SOCKET_LISTENER (listener), cancellable, callback, user_data);
#endif
}

GIOStream*
spice_controller_listener_accept_finish (GObject *listener,
                                         GAsyncResult *result,
                                         GObject **source_object,
                                         GError **error)
{
    g_return_val_if_fail(G_IS_OBJECT(listener), NULL);

#ifdef G_OS_WIN32
    SpiceNamedPipeConnection *np;
    np = spice_named_pipe_listener_accept_finish (SPICE_NAMED_PIPE_LISTENER (listener), result, source_object, error);
    if (np)
        return G_IO_STREAM (np);
#else
    GSocketConnection *socket;
    socket = g_socket_listener_accept_finish (G_SOCKET_LISTENER (listener), result, source_object, error);
    if (socket)
        return G_IO_STREAM (socket);
#endif

    return NULL;
}
