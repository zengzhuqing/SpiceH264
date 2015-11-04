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

#ifndef SPICE_CHAR_H_
#define SPICE_CHAR_H_

#if !defined(SPICE_H_INSIDE) && !defined(SPICE_SERVER_INTERNAL)
#error "Only spice.h can be included directly."
#endif

#include "spice-core.h"

/* char device interfaces */

#define SPICE_INTERFACE_CHAR_DEVICE "char_device"
#define SPICE_INTERFACE_CHAR_DEVICE_MAJOR 1
#define SPICE_INTERFACE_CHAR_DEVICE_MINOR 3
typedef struct SpiceCharDeviceInterface SpiceCharDeviceInterface;
typedef struct SpiceCharDeviceInstance SpiceCharDeviceInstance;
typedef struct SpiceCharDeviceState SpiceCharDeviceState;

typedef enum {
    SPICE_CHAR_DEVICE_NOTIFY_WRITABLE = 1 << 0,
} spice_char_device_flags;

struct SpiceCharDeviceInterface {
    SpiceBaseInterface base;

    void (*state)(SpiceCharDeviceInstance *sin, int connected);
    int (*write)(SpiceCharDeviceInstance *sin, const uint8_t *buf, int len);
    int (*read)(SpiceCharDeviceInstance *sin, uint8_t *buf, int len);
    void (*event)(SpiceCharDeviceInstance *sin, uint8_t event);
    spice_char_device_flags flags;
};

struct SpiceCharDeviceInstance {
    SpiceBaseInstance base;
    const char* subtype;
    SpiceCharDeviceState *st;
    const char* portname;
};

void spice_server_char_device_wakeup(SpiceCharDeviceInstance *sin);
void spice_server_port_event(SpiceCharDeviceInstance *char_device, uint8_t event);
const char** spice_server_char_device_recognized_subtypes(void);

#endif /* SPICE_CHAR_H_ */
