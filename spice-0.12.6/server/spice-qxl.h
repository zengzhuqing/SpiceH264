/*
 *  Copyright (C) 2014 Red Hat, Inc.
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

#ifndef SPICE_QXL_H_
#define SPICE_QXL_H_

#if !defined(SPICE_H_INSIDE) && !defined(SPICE_SERVER_INTERNAL)
#error "Only spice.h can be included directly."
#endif

#include "spice-core.h"

/* qxl interface */

#define SPICE_INTERFACE_QXL "qxl"
#define SPICE_INTERFACE_QXL_MAJOR 3
#define SPICE_INTERFACE_QXL_MINOR 3

typedef struct QXLInterface QXLInterface;
typedef struct QXLInstance QXLInstance;
typedef struct QXLState QXLState;
typedef struct QXLWorker QXLWorker;
typedef struct QXLDevMemSlot QXLDevMemSlot;
typedef struct QXLDevSurfaceCreate QXLDevSurfaceCreate;

struct QXLWorker {
    uint32_t minor_version;
    uint32_t major_version;
    /* These calls are deprecated. Please use the spice_qxl_* calls instead */
    void (*wakeup)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*oom)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*start)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*stop)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*update_area)(QXLWorker *qxl_worker, uint32_t surface_id,
                       struct QXLRect *area, struct QXLRect *dirty_rects,
                       uint32_t num_dirty_rects, uint32_t clear_dirty_region) SPICE_GNUC_DEPRECATED;
    void (*add_memslot)(QXLWorker *worker, QXLDevMemSlot *slot) SPICE_GNUC_DEPRECATED;
    void (*del_memslot)(QXLWorker *worker, uint32_t slot_group_id, uint32_t slot_id) SPICE_GNUC_DEPRECATED;
    void (*reset_memslots)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*destroy_surfaces)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*destroy_primary_surface)(QXLWorker *worker, uint32_t surface_id) SPICE_GNUC_DEPRECATED;
    void (*create_primary_surface)(QXLWorker *worker, uint32_t surface_id,
                                   QXLDevSurfaceCreate *surface) SPICE_GNUC_DEPRECATED;
    void (*reset_image_cache)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*reset_cursor)(QXLWorker *worker) SPICE_GNUC_DEPRECATED;
    void (*destroy_surface_wait)(QXLWorker *worker, uint32_t surface_id) SPICE_GNUC_DEPRECATED;
    void (*loadvm_commands)(QXLWorker *worker, struct QXLCommandExt *ext, uint32_t count) SPICE_GNUC_DEPRECATED;
};

void spice_qxl_wakeup(QXLInstance *instance);
void spice_qxl_oom(QXLInstance *instance);
/* deprecated since 0.11.2, spice_server_vm_start replaces it */
void spice_qxl_start(QXLInstance *instance) SPICE_GNUC_DEPRECATED;
/* deprecated since 0.11.2 spice_server_vm_stop replaces it */
void spice_qxl_stop(QXLInstance *instance) SPICE_GNUC_DEPRECATED;
void spice_qxl_update_area(QXLInstance *instance, uint32_t surface_id,
                   struct QXLRect *area, struct QXLRect *dirty_rects,
                   uint32_t num_dirty_rects, uint32_t clear_dirty_region);
void spice_qxl_add_memslot(QXLInstance *instance, QXLDevMemSlot *slot);
void spice_qxl_del_memslot(QXLInstance *instance, uint32_t slot_group_id, uint32_t slot_id);
void spice_qxl_reset_memslots(QXLInstance *instance);
void spice_qxl_destroy_surfaces(QXLInstance *instance);
void spice_qxl_destroy_primary_surface(QXLInstance *instance, uint32_t surface_id);
void spice_qxl_create_primary_surface(QXLInstance *instance, uint32_t surface_id,
                               QXLDevSurfaceCreate *surface);
void spice_qxl_reset_image_cache(QXLInstance *instance);
void spice_qxl_reset_cursor(QXLInstance *instance);
void spice_qxl_destroy_surface_wait(QXLInstance *instance, uint32_t surface_id);
void spice_qxl_loadvm_commands(QXLInstance *instance, struct QXLCommandExt *ext, uint32_t count);
/* async versions of commands. when complete spice calls async_complete */
void spice_qxl_update_area_async(QXLInstance *instance, uint32_t surface_id, QXLRect *qxl_area,
                                 uint32_t clear_dirty_region, uint64_t cookie);
void spice_qxl_add_memslot_async(QXLInstance *instance, QXLDevMemSlot *slot, uint64_t cookie);
void spice_qxl_destroy_surfaces_async(QXLInstance *instance, uint64_t cookie);
void spice_qxl_destroy_primary_surface_async(QXLInstance *instance, uint32_t surface_id, uint64_t cookie);
void spice_qxl_create_primary_surface_async(QXLInstance *instance, uint32_t surface_id,
                                QXLDevSurfaceCreate *surface, uint64_t cookie);
void spice_qxl_destroy_surface_async(QXLInstance *instance, uint32_t surface_id, uint64_t cookie);
/* suspend and resolution change on windows drivers */
void spice_qxl_flush_surfaces_async(QXLInstance *instance, uint64_t cookie);
/* since spice 0.12.0 */
void spice_qxl_monitors_config_async(QXLInstance *instance, QXLPHYSICAL monitors_config,
                                     int group_id, uint64_t cookie);
/* since spice 0.12.3 */
void spice_qxl_driver_unload(QXLInstance *instance);
/* since spice 0.12.6 */
void spice_qxl_set_max_monitors(QXLInstance *instance,
                                unsigned int max_monitors);

typedef struct QXLDrawArea {
    uint8_t *buf;
    uint32_t size;
    uint8_t *line_0;
    uint32_t width;
    uint32_t heigth;
    int stride;
} QXLDrawArea;

typedef struct QXLDevInfo {
    uint32_t x_res;
    uint32_t y_res;
    uint32_t bits;
    uint32_t use_hardware_cursor;

    QXLDrawArea draw_area;

    uint32_t ram_size;
} QXLDevInfo;

typedef struct QXLDevInitInfo {
    uint32_t num_memslots_groups;
    uint32_t num_memslots;
    uint8_t memslot_gen_bits;
    uint8_t memslot_id_bits;
    uint32_t qxl_ram_size;
    uint8_t internal_groupslot_id;
    uint32_t n_surfaces;
} QXLDevInitInfo;

struct QXLDevMemSlot {
    uint32_t slot_group_id;
    uint32_t slot_id;
    uint32_t generation;
    unsigned long virt_start;
    unsigned long virt_end;
    uint64_t addr_delta;
    uint32_t qxl_ram_size;
};

struct QXLDevSurfaceCreate {
    uint32_t width;
    uint32_t height;
    int32_t stride;
    uint32_t format;
    uint32_t position;
    uint32_t mouse_mode;
    uint32_t flags;
    uint32_t type;
    uint64_t mem;
    uint32_t group_id;
};

struct QXLInterface {
    SpiceBaseInterface base;

    void (*attache_worker)(QXLInstance *qin, QXLWorker *qxl_worker);
    void (*set_compression_level)(QXLInstance *qin, int level);
    void (*set_mm_time)(QXLInstance *qin, uint32_t mm_time) SPICE_GNUC_DEPRECATED;

    void (*get_init_info)(QXLInstance *qin, QXLDevInitInfo *info);
    int (*get_command)(QXLInstance *qin, struct QXLCommandExt *cmd);
    int (*req_cmd_notification)(QXLInstance *qin);
    void (*release_resource)(QXLInstance *qin, struct QXLReleaseInfoExt release_info);
    int (*get_cursor_command)(QXLInstance *qin, struct QXLCommandExt *cmd);
    int (*req_cursor_notification)(QXLInstance *qin);
    void (*notify_update)(QXLInstance *qin, uint32_t update_id);
    int (*flush_resources)(QXLInstance *qin);
    void (*async_complete)(QXLInstance *qin, uint64_t cookie);
    void (*update_area_complete)(QXLInstance *qin, uint32_t surface_id,
                                 struct QXLRect *updated_rects,
                                 uint32_t num_updated_rects);
    void (*set_client_capabilities)(QXLInstance *qin,
                                    uint8_t client_present,
                                    uint8_t caps[58]);
    /* returns 1 if the interface is supported, 0 otherwise.
     * if monitors_config is NULL nothing is done except reporting the
     * return code. */
    int (*client_monitors_config)(QXLInstance *qin,
                                  VDAgentMonitorsConfig *monitors_config);
};

struct QXLInstance {
    SpiceBaseInstance  base;
    int                id;
    QXLState           *st;
};

#endif /* SPICE_QXL_H_ */
