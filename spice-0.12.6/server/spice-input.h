/*
 *  Copyright (C) 2009-2014 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SPICE_INPUT_H_
#define SPICE_INPUT_H_

#if !defined(SPICE_H_INSIDE) && !defined(SPICE_SERVER_INTERNAL)
#error "Only spice.h can be included directly."
#endif

#include "spice-core.h"

/* input interfaces */

#define SPICE_INTERFACE_KEYBOARD "keyboard"
#define SPICE_INTERFACE_KEYBOARD_MAJOR 1
#define SPICE_INTERFACE_KEYBOARD_MINOR 1
typedef struct SpiceKbdInterface SpiceKbdInterface;
typedef struct SpiceKbdInstance SpiceKbdInstance;
typedef struct SpiceKbdState SpiceKbdState;

struct SpiceKbdInterface {
    SpiceBaseInterface base;

    void (*push_scan_freg)(SpiceKbdInstance *sin, uint8_t frag);
    uint8_t (*get_leds)(SpiceKbdInstance *sin);
};

struct SpiceKbdInstance {
    SpiceBaseInstance base;
    SpiceKbdState     *st;
};

int spice_server_kbd_leds(SpiceKbdInstance *sin, int leds);

#define SPICE_INTERFACE_MOUSE "mouse"
#define SPICE_INTERFACE_MOUSE_MAJOR 1
#define SPICE_INTERFACE_MOUSE_MINOR 1
typedef struct SpiceMouseInterface SpiceMouseInterface;
typedef struct SpiceMouseInstance SpiceMouseInstance;
typedef struct SpiceMouseState SpiceMouseState;

struct SpiceMouseInterface {
    SpiceBaseInterface base;

    void (*motion)(SpiceMouseInstance *sin, int dx, int dy, int dz,
                   uint32_t buttons_state);
    void (*buttons)(SpiceMouseInstance *sin, uint32_t buttons_state);
};

struct SpiceMouseInstance {
    SpiceBaseInstance base;
    SpiceMouseState   *st;
};

#define SPICE_INTERFACE_TABLET "tablet"
#define SPICE_INTERFACE_TABLET_MAJOR 1
#define SPICE_INTERFACE_TABLET_MINOR 1
typedef struct SpiceTabletInterface SpiceTabletInterface;
typedef struct SpiceTabletInstance SpiceTabletInstance;
typedef struct SpiceTabletState SpiceTabletState;

struct SpiceTabletInterface {
    SpiceBaseInterface base;

    void (*set_logical_size)(SpiceTabletInstance* tablet, int width, int height);
    void (*position)(SpiceTabletInstance* tablet, int x, int y, uint32_t buttons_state);
    void (*wheel)(SpiceTabletInstance* tablet, int wheel_moution, uint32_t buttons_state);
    void (*buttons)(SpiceTabletInstance* tablet, uint32_t buttons_state);
};

struct SpiceTabletInstance {
    SpiceBaseInstance base;
    SpiceTabletState  *st;
};

#endif /* SPICE_INPUT_H_ */
