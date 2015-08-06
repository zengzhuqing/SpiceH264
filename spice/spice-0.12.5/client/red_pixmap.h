/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009 Red Hat, Inc.

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

#ifndef _H_RED_PIXMAP
#define _H_RED_PIXMAP

#include "red_drawable.h"
#include "utils.h"

class RedPixmap: public RedDrawable {
public:
    RedPixmap(int width, int height, Format format, bool top_bottom);
    virtual ~RedPixmap();

    virtual SpicePoint get_size() { SpicePoint pt = {_width, _height}; return pt;}

    int get_width() { return _width;}
    int get_height() { return _height;}
    int get_stride() { return _stride;}
    uint8_t* get_data() { return _data;}
    bool is_big_endian_bits();
    virtual RedDrawable::Format get_format() { return _format; }

    bool equal(const RedPixmap &other, const SpiceRect &rect) const {
        spice_debug("l:%d r:%d t:%d b:%d", rect.left, rect.right, rect.top, rect.bottom);
        for (int x = rect.left; x < rect.right; ++x)
            for (int y = rect.top; y < rect.bottom; ++y) {
                for (int i = 0; i < 3; i++) { // ignore alpha
                    int p = x * 4 + y * _stride + i;
                    if (other._data[p] != _data[p]) {
                        spice_printerr("equal fails at (+%d+%d) +%d+%d:%d in %dx%d",
                                       rect.left, rect.top, x-rect.left, y-rect.top, i,
                                       _width-rect.left, _height-rect.top);
                        if (getenv("DIFFBP"))
                            SPICE_BREAKPOINT();
                        return false;
                    }
                }
            }
        return true;
    }

protected:
    Format _format;
    int _width;
    int _height;
    int _stride;
    bool _top_bottom;
    uint8_t* _data;
};

#endif
