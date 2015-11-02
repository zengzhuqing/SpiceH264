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
#ifndef __SPICE_FOREIGN_MENU_LISTENER_H__
#define __SPICE_FOREIGN_MENU_LISTENER_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define SPICE_FOREIGN_MENU_LISTENER_ERROR spice_foreign_menu_listener_error_quark ()
GQuark spice_foreign_menu_listener_error_quark (void);

typedef enum
{
    SPICE_FOREIGN_MENU_LISTENER_ERROR_VALUE /* incorrect value */
} SpiceForeignMenuListenerError;


GObject* spice_foreign_menu_listener_new (const gchar *address, GError **error);

void spice_foreign_menu_listener_accept_async (GObject *listener,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data);

GIOStream* spice_foreign_menu_listener_accept_finish (GObject *listener,
                                                    GAsyncResult *result,
                                                    GObject **source_object,
                                                    GError **error);
G_END_DECLS

#endif /* __SPICE_FOREIGN_MENU_LISTENER_H__ */
