/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012-2014 Red Hat, Inc.
   Copyright © 1998-2009 VLC authors and VideoLAN

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
#include "config.h"

#include <string.h>
#include <glib/gi18n.h>

#include "glib-compat.h"

#if !GLIB_CHECK_VERSION(2,30,0)
G_DEFINE_BOXED_TYPE (GMainContext, spice_main_context, g_main_context_ref, g_main_context_unref)

#define KILOBYTE_FACTOR (G_GOFFSET_CONSTANT (1000))
#define MEGABYTE_FACTOR (KILOBYTE_FACTOR * KILOBYTE_FACTOR)
#define GIGABYTE_FACTOR (MEGABYTE_FACTOR * KILOBYTE_FACTOR)
#define TERABYTE_FACTOR (GIGABYTE_FACTOR * KILOBYTE_FACTOR)
#define PETABYTE_FACTOR (TERABYTE_FACTOR * KILOBYTE_FACTOR)
#define EXABYTE_FACTOR  (PETABYTE_FACTOR * KILOBYTE_FACTOR)

/**
 * g_format_size:
 * @size: a size in bytes
 *
 * Formats a size (for example the size of a file) into a human readable
 * string.  Sizes are rounded to the nearest size prefix (kB, MB, GB)
 * and are displayed rounded to the nearest tenth. E.g. the file size
 * 3292528 bytes will be converted into the string "3.2 MB".
 *
 * The prefix units base is 1000 (i.e. 1 kB is 1000 bytes).
 *
 * This string should be freed with g_free() when not needed any longer.
 *
 * See g_format_size_full() for more options about how the size might be
 * formatted.
 *
 * Returns: a newly-allocated formatted string containing a human readable
 *     file size
 *
 * Since: 2.30
 */
gchar *
g_format_size (guint64 size)
{
  GString *string;

  string = g_string_new (NULL);

    if (size < KILOBYTE_FACTOR)
      {
        g_string_printf (string,
                         g_dngettext(GETTEXT_PACKAGE, "%u byte", "%u bytes", (guint) size),
                         (guint) size);
      }

    else if (size < MEGABYTE_FACTOR)
      g_string_printf (string, _("%.1f kB"), (gdouble) size / (gdouble) KILOBYTE_FACTOR);

    else if (size < GIGABYTE_FACTOR)
      g_string_printf (string, _("%.1f MB"), (gdouble) size / (gdouble) MEGABYTE_FACTOR);

    else if (size < TERABYTE_FACTOR)
      g_string_printf (string, _("%.1f GB"), (gdouble) size / (gdouble) GIGABYTE_FACTOR);
    else if (size < PETABYTE_FACTOR)
      g_string_printf (string, _("%.1f TB"), (gdouble) size / (gdouble) TERABYTE_FACTOR);

    else if (size < EXABYTE_FACTOR)
      g_string_printf (string, _("%.1f PB"), (gdouble) size / (gdouble) PETABYTE_FACTOR);

    else
      g_string_printf (string, _("%.1f EB"), (gdouble) size / (gdouble) EXABYTE_FACTOR);

  return g_string_free (string, FALSE);
}

#endif


#if !GLIB_CHECK_VERSION(2,32,0)
/**
 * g_queue_free_full:
 * @queue: a pointer to a #GQueue
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #GQueue,
 * and calls the specified destroy function on every element's data.
 *
 * Since: 2.32
 */
void
g_queue_free_full (GQueue        *queue,
                  GDestroyNotify  free_func)
{
  g_queue_foreach (queue, (GFunc) free_func, NULL);
  g_queue_free (queue);
}
#endif


#ifndef HAVE_STRTOK_R
G_GNUC_INTERNAL
char *strtok_r(char *s, const char *delim, char **save_ptr)
{
    char *token;

    if (s == NULL)
        s = *save_ptr;

    /* Scan leading delimiters. */
    s += strspn (s, delim);
    if (*s == '\0')
        return NULL;

    /* Find the end of the token. */
    token = s;
    s = strpbrk (token, delim);
    if (s == NULL)
        /* This token finishes the string. */
        *save_ptr = strchr (token, '\0');
    else
    {
        /* Terminate the token and make *SAVE_PTR point past it. */
        *s = '\0';
        *save_ptr = s + 1;
    }
    return token;
}
#endif
