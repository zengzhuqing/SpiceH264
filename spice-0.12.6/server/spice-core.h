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

#ifndef SPICE_CORE_H_
#define SPICE_CORE_H_

#if !defined(SPICE_H_INSIDE) && !defined(SPICE_SERVER_INTERNAL)
#error "Only spice.h can be included directly."
#endif

#include <stdint.h>
#include <sys/socket.h>
#include <spice/qxl_dev.h>
#include <spice/vd_agent.h>
#include <spice/macros.h>

#ifdef SPICE_SERVER_INTERNAL
#undef SPICE_GNUC_DEPRECATED
#define SPICE_GNUC_DEPRECATED
#endif

/* interface base type */

typedef struct SpiceBaseInterface SpiceBaseInterface;
typedef struct SpiceBaseInstance SpiceBaseInstance;

struct SpiceBaseInterface {
    const char *type;
    const char *description;
    uint32_t major_version;
    uint32_t minor_version;
};
struct SpiceBaseInstance {
    const SpiceBaseInterface *sif;
};

/* core interface */

#define SPICE_INTERFACE_CORE "core"
#define SPICE_INTERFACE_CORE_MAJOR 1
#define SPICE_INTERFACE_CORE_MINOR 3
typedef struct SpiceCoreInterface SpiceCoreInterface;

#define SPICE_WATCH_EVENT_READ  (1 << 0)
#define SPICE_WATCH_EVENT_WRITE (1 << 1)

#define SPICE_CHANNEL_EVENT_CONNECTED     1
#define SPICE_CHANNEL_EVENT_INITIALIZED   2
#define SPICE_CHANNEL_EVENT_DISCONNECTED  3

#define SPICE_CHANNEL_EVENT_FLAG_TLS      (1 << 0)
#define SPICE_CHANNEL_EVENT_FLAG_ADDR_EXT (1 << 1)

typedef struct SpiceWatch SpiceWatch;
typedef void (*SpiceWatchFunc)(int fd, int event, void *opaque);

typedef struct SpiceTimer SpiceTimer;
typedef void (*SpiceTimerFunc)(void *opaque);

typedef struct SpiceChannelEventInfo {
    int connection_id;
    int type;
    int id;
    int flags;
    /* deprecated, can't hold ipv6 addresses, kept for backward compatibility */
    struct sockaddr laddr SPICE_GNUC_DEPRECATED;
    struct sockaddr paddr SPICE_GNUC_DEPRECATED;
    socklen_t llen SPICE_GNUC_DEPRECATED;
    socklen_t plen SPICE_GNUC_DEPRECATED;
    /* should be used if (flags & SPICE_CHANNEL_EVENT_FLAG_ADDR_EXT) */
    struct sockaddr_storage laddr_ext;
    struct sockaddr_storage paddr_ext;
    socklen_t llen_ext, plen_ext;
} SpiceChannelEventInfo;

struct SpiceCoreInterface {
    SpiceBaseInterface base;

    SpiceTimer *(*timer_add)(SpiceTimerFunc func, void *opaque);
    void (*timer_start)(SpiceTimer *timer, uint32_t ms);
    void (*timer_cancel)(SpiceTimer *timer);
    void (*timer_remove)(SpiceTimer *timer);

    SpiceWatch *(*watch_add)(int fd, int event_mask, SpiceWatchFunc func, void *opaque);
    void (*watch_update_mask)(SpiceWatch *watch, int event_mask);
    void (*watch_remove)(SpiceWatch *watch);

    void (*channel_event)(int event, SpiceChannelEventInfo *info);
};


#endif /* SPICE_CORE_H_ */
