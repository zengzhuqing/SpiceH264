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
#ifndef __WIN32_UTIL_H__
#define __WIN32_UTIL_H__

#include <gio/gio.h>
#include "namedpipe.h"

G_BEGIN_DECLS

gboolean        spice_win32_set_low_integrity (void* handle, GError **error);
SpiceNamedPipe* spice_win32_user_pipe_new (gchar *name, GError **error);

G_END_DECLS

#endif /* __WIN32_UTIL_H__ */
