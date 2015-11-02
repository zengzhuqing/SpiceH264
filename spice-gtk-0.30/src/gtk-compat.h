/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012-2014 Red Hat, Inc.

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
#ifndef GTK_COMPAT_H
#define GTK_COMPAT_H

#include "config.h"

#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION (2, 91, 0)
#define GDK_IS_X11_DISPLAY(D) TRUE
#define gdk_window_get_display(W) gdk_drawable_get_display(GDK_DRAWABLE(W))
#endif

#if GTK_CHECK_VERSION (2, 91, 0)
static inline void gdk_drawable_get_size(GdkWindow *w, gint *ww, gint *wh)
{
    *ww = gdk_window_get_width(w);
    *wh = gdk_window_get_height(w);
}
#endif

#if !GTK_CHECK_VERSION(2, 20, 0)
static inline gboolean gtk_widget_get_realized(GtkWidget *widget)
{
    g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
    return GTK_WIDGET_REALIZED(widget);
}
#endif

#if !GTK_CHECK_VERSION (3, 0, 0)
#define cairo_rectangle_int_t GdkRectangle
#define cairo_region_t GdkRegion
#define cairo_region_create_rectangle gdk_region_rectangle
#define cairo_region_subtract_rectangle(_dest,_rect) { GdkRegion *_region = gdk_region_rectangle (_rect); gdk_region_subtract (_dest, _region); gdk_region_destroy (_region); }
#define cairo_region_destroy gdk_region_destroy

#define gdk_window_get_display(W) gdk_drawable_get_display(GDK_DRAWABLE(W))
#endif

#endif /* GTK_COMPAT_H */
