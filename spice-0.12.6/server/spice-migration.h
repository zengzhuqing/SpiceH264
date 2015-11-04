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

#ifndef SPICE_MIGRATION_H_
#define SPICE_MIGRATION_H_

#if !defined(SPICE_H_INSIDE) && !defined(SPICE_SERVER_INTERNAL)
#error "Only spice.h can be included directly."
#endif

#include "spice-core.h"

/* migration interface */
#define SPICE_INTERFACE_MIGRATION "migration"
#define SPICE_INTERFACE_MIGRATION_MAJOR 1
#define SPICE_INTERFACE_MIGRATION_MINOR 1
typedef struct SpiceMigrateInterface SpiceMigrateInterface;
typedef struct SpiceMigrateInstance SpiceMigrateInstance;
typedef struct SpiceMigrateState SpiceMigrateState;

struct SpiceMigrateInterface {
    SpiceBaseInterface base;
    void (*migrate_connect_complete)(SpiceMigrateInstance *sin);
    void (*migrate_end_complete)(SpiceMigrateInstance *sin);
};

struct SpiceMigrateInstance {
    SpiceBaseInstance base;
    SpiceMigrateState *st;
};

/* spice switch-host client migration */
int spice_server_migrate_info(SpiceServer *s, const char* dest,
                              int port, int secure_port,
                              const char* cert_subject);
int spice_server_migrate_switch(SpiceServer *s);

/* spice (semi-)seamless client migration */
int spice_server_migrate_connect(SpiceServer *s, const char* dest,
                                 int port, int secure_port,
                                 const char* cert_subject);
int spice_server_migrate_start(SpiceServer *s);
int spice_server_migrate_end(SpiceServer *s, int completed);

void spice_server_set_seamless_migration(SpiceServer *s, int enable);

#endif /* SPICE_MIGRATION_H_ */
