/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009,2010 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <inttypes.h>
#include <zlib.h>
#include <pthread.h>
#include "reds.h"
#include "red_worker.h"
#include "red_common.h"
#include "red_memslots.h"
#include "red_parse_qxl.h"
#include "red_replay_qxl.h"
#include <glib.h>

typedef enum {
    REPLAY_OK = 0,
    REPLAY_EOF,
} replay_t;

struct SpiceReplay {
    FILE *fd;
    int eof;
    int counter;
    bool created_primary;

    GArray *id_map; // record id -> replay id
    GArray *id_map_inv; // replay id -> record id
    GArray *id_free; // free list
    int nsurfaces;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static int replay_fread(SpiceReplay *replay, uint8_t *buf, size_t size)
{
    if (replay->eof) {
        return 0;
    }
    if (feof(replay->fd)) {
        replay->eof = 1;
        return 0;
    }
    return fread(buf, size, 1, replay->fd);
}

__attribute__((format(scanf, 2, 3)))
static replay_t replay_fscanf(SpiceReplay *replay, const char *fmt, ...)
{
    va_list ap;
    int ret;

    if (replay->eof) {
        return REPLAY_EOF;
    }
    if (feof(replay->fd)) {
        replay->eof = 1;
        return REPLAY_EOF;
    }
    va_start(ap, fmt);
    ret = vfscanf(replay->fd, fmt, ap);
    va_end(ap);
    if (ret == EOF) {
        replay->eof = 1;
    }
    return replay->eof ? REPLAY_EOF : REPLAY_OK;
}

static uint32_t replay_id_get(SpiceReplay *replay, uint32_t id)
{
    uint32_t newid = 0;

    /* TODO: this should be avoided, perhaps in recording? */
    if (id == -1)
        return id;

    pthread_mutex_lock(&replay->mutex);
    if (replay->id_map->len <= id) {
        spice_warn_if_reached();
    } else {
        newid = g_array_index(replay->id_map, uint32_t, id);
    }
    pthread_mutex_unlock(&replay->mutex);

    return newid;
}

static uint32_t replay_id_new(SpiceReplay *replay, uint32_t id)
{
    uint32_t new_id;
    uint32_t *map;

    pthread_mutex_lock(&replay->mutex);
    while (1) {
        if (replay->id_free->len > 0) {
            new_id = g_array_index(replay->id_free, uint32_t, 0);
            g_array_remove_index_fast(replay->id_free, 0);
        } else {
            new_id = replay->id_map_inv->len;
        }

        if (new_id < replay->nsurfaces)
            break;
        pthread_cond_wait(&replay->cond, &replay->mutex);
    }

    if (replay->id_map->len <= id)
        g_array_set_size(replay->id_map, id + 1);
    if (replay->id_map_inv->len <= new_id)
        g_array_set_size(replay->id_map_inv, new_id + 1);

    map = &g_array_index(replay->id_map, uint32_t, id);
    *map = new_id;
    map = &g_array_index(replay->id_map_inv, uint32_t, new_id);
    *map = id;
    pthread_mutex_unlock(&replay->mutex);

    spice_debug("%u -> %u (map %u, inv %u)", id, new_id,
                replay->id_map->len, replay->id_map_inv->len);

    return new_id;
}

static void replay_id_free(SpiceReplay *replay, uint32_t id)
{
    uint32_t old_id;
    uint32_t *map;

    pthread_mutex_lock(&replay->mutex);
    map = &g_array_index(replay->id_map_inv, uint32_t, id);
    old_id = *map;
    *map = -1;

    if (old_id != -1) {
        map = &g_array_index(replay->id_map, uint32_t, old_id);
        if (*map == id)
            *map = -1;

        g_array_append_val(replay->id_free, id);
    }
    pthread_cond_signal(&replay->cond);
    pthread_mutex_unlock(&replay->mutex);
}


#if 0
static void hexdump(uint8_t *hex, uint8_t bytes)
{
    int i;

    for (i = 0; i < bytes; i++) {
        if (0 == i % 16) {
            fprintf(stderr, "%lx: ", (size_t)hex+i);
        }
        if (0 == i % 4) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, " %02x", hex[i]);
        if (15 == i % 16) {
            fprintf(stderr, "\n");
        }
    }
}
#endif

static replay_t read_binary(SpiceReplay *replay, const char *prefix, size_t *size, uint8_t
                            **buf, size_t base_size)
{
    char template[1024];
    int with_zlib = -1;
    int zlib_size;
    uint8_t *zlib_buffer;
    z_stream strm;

    snprintf(template, sizeof(template), "binary %%d %s %%ld:", prefix);
    if (replay_fscanf(replay, template, &with_zlib, size) == REPLAY_EOF)
        return REPLAY_EOF;

    if (*buf == NULL) {
        *buf = malloc(*size + base_size);
        if (*buf == NULL) {
            spice_error("allocation error for %ld", *size);
            exit(1);
        }
    }
#if 0
    {
        int num_read = fread(*buf + base_size, *size, 1, fd);
        spice_error("num_read = %d", num_read);
        hexdump(*buf + base_size, *size);
    }
#else
    spice_return_val_if_fail(with_zlib != -1, REPLAY_EOF);
    if (with_zlib) {
        int ret;

        replay_fscanf(replay, "%d:", &zlib_size);
        zlib_buffer = malloc(zlib_size);
        replay_fread(replay, zlib_buffer, zlib_size);
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = zlib_size;
        strm.next_in = zlib_buffer;
        strm.avail_out = *size;
        strm.next_out = *buf + base_size;
        if ((ret = inflateInit(&strm)) != Z_OK) {
            spice_error("inflateInit failed");
            exit(1);
        }
        if ((ret = inflate(&strm, Z_NO_FLUSH)) != Z_STREAM_END) {
            spice_error("inflate error %d (disc: %ld)", ret, *size - strm.total_out);
            if (ret == Z_DATA_ERROR) {
                /* last operation may be wrong. since we do the recording
                 * in red_worker, when there is a shutdown from the vcpu/io thread
                 * it seems it may kill the red_worker thread (so a chunk is
                 * left hanging and the rest of the message is never written).
                 * Let it pass */
                return REPLAY_EOF;
            }
            if (ret != Z_OK) {
                spice_warn_if_reached();
            }
        }
        (void)inflateEnd(&strm);
        free(zlib_buffer); // TODO - avoid repeat alloc/dealloc by keeping last
    } else {
        replay_fread(replay, *buf + base_size, *size);
    }
#endif
    replay_fscanf(replay, "\n");
    return REPLAY_OK;
}

static size_t red_replay_data_chunks(SpiceReplay *replay, const char *prefix,
                                     uint8_t **mem, size_t base_size)
{
    size_t data_size;
    int count_chunks;
    size_t next_data_size;
    QXLDataChunk *cur;

    replay_fscanf(replay, "data_chunks %d %ld\n", &count_chunks, &data_size);
    if (base_size == 0) {
        base_size = sizeof(QXLDataChunk);
    }

    if (read_binary(replay, prefix, &next_data_size, mem, base_size) == REPLAY_EOF) {
        return 0;
    }
    cur = (QXLDataChunk*)(*mem + base_size - sizeof(QXLDataChunk));
    cur->data_size = next_data_size;
    data_size = cur->data_size;
    cur->next_chunk = cur->prev_chunk = 0;
    while (count_chunks-- > 0) {
        if (read_binary(replay, prefix, &next_data_size, (uint8_t**)&cur->next_chunk,
            sizeof(QXLDataChunk)) == REPLAY_EOF) {
            return 0;
        }
        data_size += next_data_size;
        ((QXLDataChunk*)cur->next_chunk)->prev_chunk = (QXLPHYSICAL)cur;
        cur = (QXLDataChunk*)cur->next_chunk;
        cur->data_size = next_data_size;
        cur->next_chunk = 0;
    }

    return data_size;
}

static void red_replay_data_chunks_free(SpiceReplay *replay, void *data, size_t base_size)
{
    QXLDataChunk *cur = (QXLDataChunk *)((uint8_t*)data +
        (base_size ? base_size - sizeof(QXLDataChunk) : 0));

    cur = (QXLDataChunk *)cur->next_chunk;
    while (cur) {
        QXLDataChunk *next = (QXLDataChunk *)cur->next_chunk;
        free(cur);
        cur = next;
    }

    free(data);
}

static void red_replay_point_ptr(SpiceReplay *replay, QXLPoint *qxl)
{
    replay_fscanf(replay, "point %d %d\n", &qxl->x, &qxl->y);
}

static void red_replay_rect_ptr(SpiceReplay *replay, const char *prefix, QXLRect *qxl)
{
    char template[1024];

    snprintf(template, sizeof(template), "rect %s %%d %%d %%d %%d\n", prefix);
    replay_fscanf(replay, template, &qxl->top, &qxl->left, &qxl->bottom, &qxl->right);
}

static QXLPath *red_replay_path(SpiceReplay *replay)
{
    QXLPath *qxl = NULL;
    size_t data_size;

    data_size = red_replay_data_chunks(replay, "path", (uint8_t**)&qxl, sizeof(QXLPath));
    qxl->data_size = data_size;
    return qxl;
}

static void red_replay_path_free(SpiceReplay *replay, QXLPHYSICAL p)
{
    QXLPath *qxl = (QXLPath *)p;

    red_replay_data_chunks_free(replay, qxl, sizeof(*qxl));
}

static QXLClipRects *red_replay_clip_rects(SpiceReplay *replay)
{
    QXLClipRects *qxl = NULL;
    int num_rects;

    replay_fscanf(replay, "num_rects %d\n", &num_rects);
    red_replay_data_chunks(replay, "clip_rects", (uint8_t**)&qxl, sizeof(QXLClipRects));
    qxl->num_rects = num_rects;
    return qxl;
}

static void red_replay_clip_rects_free(SpiceReplay *replay, QXLClipRects *qxl)
{
    red_replay_data_chunks_free(replay, qxl, sizeof(*qxl));
}

static uint8_t *red_replay_image_data_flat(SpiceReplay *replay, size_t *size)
{
    uint8_t *data = NULL;

    read_binary(replay, "image_data_flat", size, &data, 0);
    return data;
}

static QXLImage *red_replay_image(SpiceReplay *replay, uint32_t flags)
{
    QXLImage* qxl = NULL;
    size_t bitmap_size, size;
    uint8_t qxl_flags;
    int temp;
    int has_palette;
    int has_image;

    replay_fscanf(replay, "image %d\n", &has_image);
    if (!has_image) {
        return NULL;
    }

    qxl = (QXLImage*)malloc(sizeof(QXLImage));
    replay_fscanf(replay, "descriptor.id %ld\n", &qxl->descriptor.id);
    replay_fscanf(replay, "descriptor.type %d\n", &temp); qxl->descriptor.type = temp;
    replay_fscanf(replay, "descriptor.flags %d\n", &temp); qxl->descriptor.flags = temp;
    replay_fscanf(replay, "descriptor.width %d\n", &qxl->descriptor.width);
    replay_fscanf(replay, "descriptor.height %d\n", &qxl->descriptor.height);

    switch (qxl->descriptor.type) {
    case SPICE_IMAGE_TYPE_BITMAP:
        replay_fscanf(replay, "bitmap.format %d\n", &temp); qxl->bitmap.format = temp;
        replay_fscanf(replay, "bitmap.flags %d\n", &temp); qxl->bitmap.flags = temp;
        replay_fscanf(replay, "bitmap.x %d\n", &qxl->bitmap.x);
        replay_fscanf(replay, "bitmap.y %d\n", &qxl->bitmap.y);
        replay_fscanf(replay, "bitmap.stride %d\n", &qxl->bitmap.stride);
        qxl_flags = qxl->bitmap.flags;
        replay_fscanf(replay, "has_palette %d\n", &has_palette);
        if (has_palette) {
            QXLPalette *qp;
            int i, num_ents;

            replay_fscanf(replay, "qp.num_ents %d\n", &num_ents);
            qp = malloc(sizeof(QXLPalette) + num_ents * sizeof(qp->ents[0]));
            qp->num_ents = num_ents;
            qxl->bitmap.palette = (QXLPHYSICAL)qp;
            replay_fscanf(replay, "unique %ld\n", &qp->unique);
            for (i = 0; i < num_ents; i++) {
                replay_fscanf(replay, "ents %d\n", &qp->ents[i]);
            }
        } else {
            qxl->bitmap.palette = 0;
        }
        bitmap_size = qxl->bitmap.y * abs(qxl->bitmap.stride);
        qxl->bitmap.data = 0;
        if (qxl_flags & QXL_BITMAP_DIRECT) {
            qxl->bitmap.data = (QXLPHYSICAL)red_replay_image_data_flat(replay, &bitmap_size);
        } else {
            size = red_replay_data_chunks(replay, "bitmap.data", (uint8_t**)&qxl->bitmap.data, 0);
            if (size != bitmap_size) {
                spice_printerr("bad image, %ld != %ld", size, bitmap_size);
                return NULL;
            }
        }
        break;
    case SPICE_IMAGE_TYPE_SURFACE:
        replay_fscanf(replay, "surface_image.surface_id %d\n", &qxl->surface_image.surface_id);
        qxl->surface_image.surface_id = replay_id_get(replay, qxl->surface_image.surface_id);
        break;
    case SPICE_IMAGE_TYPE_QUIC:
        // TODO - make this much nicer (precompute size and allocs, store them during
        // record, then reread into them. and use MPEG-4).
        replay_fscanf(replay, "quic.data_size %d\n", &qxl->quic.data_size);
        qxl = realloc(qxl, sizeof(QXLImageDescriptor) + sizeof(QXLQUICData) +
                      qxl->quic.data_size);
        size = red_replay_data_chunks(replay, "quic.data", (uint8_t**)&qxl->quic.data, 0);
        spice_assert(size == qxl->quic.data_size);
        break;
    default:
        spice_warn_if_reached();
    }
    return qxl;
}

static void red_replay_image_free(SpiceReplay *replay, QXLPHYSICAL p, uint32_t flags)
{
    QXLImage *qxl = (QXLImage *)p;
    if (!qxl)
        return;

    switch (qxl->descriptor.type) {
    case SPICE_IMAGE_TYPE_BITMAP:
        free((void*)qxl->bitmap.palette);
        if (qxl->bitmap.flags & QXL_BITMAP_DIRECT) {
            free((void*)qxl->bitmap.data);
        } else {
            red_replay_data_chunks_free(replay, (void*)qxl->bitmap.data, 0);
        }
        break;
    case SPICE_IMAGE_TYPE_SURFACE:
        break;
    case SPICE_IMAGE_TYPE_QUIC:
        red_replay_data_chunks_free(replay, qxl, 0);
        break;
    default:
        spice_warn_if_reached();
    }

    free(qxl);
}

static void red_replay_brush_ptr(SpiceReplay *replay, QXLBrush *qxl, uint32_t flags)
{
    replay_fscanf(replay, "type %d\n", &qxl->type);
    switch (qxl->type) {
    case SPICE_BRUSH_TYPE_SOLID:
        replay_fscanf(replay, "u.color %d\n", &qxl->u.color);
        break;
    case SPICE_BRUSH_TYPE_PATTERN:
        qxl->u.pattern.pat = (QXLPHYSICAL)red_replay_image(replay, flags);
        red_replay_point_ptr(replay, &qxl->u.pattern.pos);
        break;
    }
}

static void red_replay_brush_free(SpiceReplay *replay, QXLBrush *qxl, uint32_t flags)
{
    switch (qxl->type) {
    case SPICE_BRUSH_TYPE_PATTERN:
        red_replay_image_free(replay, qxl->u.pattern.pat, flags);
        break;
    }
}

static void red_replay_qmask_ptr(SpiceReplay *replay, QXLQMask *qxl, uint32_t flags)
{
    int temp;

    replay_fscanf(replay, "flags %d\n", &temp); qxl->flags = temp;
    red_replay_point_ptr(replay, &qxl->pos);
    qxl->bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
}

static void red_replay_qmask_free(SpiceReplay *replay, QXLQMask *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->bitmap, flags);
}

static void red_replay_fill_ptr(SpiceReplay *replay, QXLFill *qxl, uint32_t flags)
{
    int temp;

    red_replay_brush_ptr(replay, &qxl->brush, flags);
    replay_fscanf(replay, "rop_descriptor %d\n", &temp); qxl->rop_descriptor = temp;
    red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_fill_free(SpiceReplay *replay, QXLFill *qxl, uint32_t flags)
{
    red_replay_brush_free(replay, &qxl->brush, flags);
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_opaque_ptr(SpiceReplay *replay, QXLOpaque *qxl, uint32_t flags)
{
    int temp;

    qxl->src_bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
    red_replay_rect_ptr(replay, "src_area", &qxl->src_area);
    red_replay_brush_ptr(replay, &qxl->brush, flags);
    replay_fscanf(replay, "rop_descriptor %d\n", &temp); qxl->rop_descriptor = temp;
    replay_fscanf(replay, "scale_mode %d\n", &temp); qxl->scale_mode = temp;
    red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_opaque_free(SpiceReplay *replay, QXLOpaque *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->src_bitmap, flags);
    red_replay_brush_free(replay, &qxl->brush, flags);
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_copy_ptr(SpiceReplay *replay, QXLCopy *qxl, uint32_t flags)
{
    int temp;

   qxl->src_bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
   red_replay_rect_ptr(replay, "src_area", &qxl->src_area);
   replay_fscanf(replay, "rop_descriptor %d\n", &temp); qxl->rop_descriptor = temp;
   replay_fscanf(replay, "scale_mode %d\n", &temp); qxl->scale_mode = temp;
   red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_copy_free(SpiceReplay *replay, QXLCopy *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->src_bitmap, flags);
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_blend_ptr(SpiceReplay *replay, QXLBlend *qxl, uint32_t flags)
{
    int temp;

   qxl->src_bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
   red_replay_rect_ptr(replay, "src_area", &qxl->src_area);
   replay_fscanf(replay, "rop_descriptor %d\n", &temp); qxl->rop_descriptor = temp;
   replay_fscanf(replay, "scale_mode %d\n", &temp); qxl->scale_mode = temp;
   red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_blend_free(SpiceReplay *replay, QXLBlend *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->src_bitmap, flags);
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_transparent_ptr(SpiceReplay *replay, QXLTransparent *qxl, uint32_t flags)
{
   qxl->src_bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
   red_replay_rect_ptr(replay, "src_area", &qxl->src_area);
   replay_fscanf(replay, "src_color %d\n", &qxl->src_color);
   replay_fscanf(replay, "true_color %d\n", &qxl->true_color);
}

static void red_replay_transparent_free(SpiceReplay *replay, QXLTransparent *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->src_bitmap, flags);
}

static void red_replay_alpha_blend_ptr(SpiceReplay *replay, QXLAlphaBlend *qxl, uint32_t flags)
{
    int temp;

    replay_fscanf(replay, "alpha_flags %d\n", &temp); qxl->alpha_flags = temp;
    replay_fscanf(replay, "alpha %d\n", &temp); qxl->alpha = temp;
    qxl->src_bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
    red_replay_rect_ptr(replay, "src_area", &qxl->src_area);
}

static void red_replay_alpha_blend_free(SpiceReplay *replay, QXLAlphaBlend *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->src_bitmap, flags);
}

static void red_replay_alpha_blend_ptr_compat(SpiceReplay *replay, QXLCompatAlphaBlend *qxl, uint32_t flags)
{
    int temp;

    replay_fscanf(replay, "alpha %d\n", &temp); qxl->alpha = temp;
    qxl->src_bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
    red_replay_rect_ptr(replay, "src_area", &qxl->src_area);
}

static void red_replay_rop3_ptr(SpiceReplay *replay, QXLRop3 *qxl, uint32_t flags)
{
    int temp;

    qxl->src_bitmap = (QXLPHYSICAL)red_replay_image(replay, flags);
    red_replay_rect_ptr(replay, "src_area", &qxl->src_area);
    red_replay_brush_ptr(replay, &qxl->brush, flags);
    replay_fscanf(replay, "rop3 %d\n", &temp); qxl->rop3 = temp;
    replay_fscanf(replay, "scale_mode %d\n", &temp); qxl->scale_mode = temp;
    red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_rop3_free(SpiceReplay *replay, QXLRop3 *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->src_bitmap, flags);
    red_replay_brush_free(replay, &qxl->brush, flags);
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_stroke_ptr(SpiceReplay *replay, QXLStroke *qxl, uint32_t flags)
{
    int temp;

    qxl->path = (QXLPHYSICAL)red_replay_path(replay);
    replay_fscanf(replay, "attr.flags %d\n", &temp); qxl->attr.flags = temp;
    if (qxl->attr.flags & SPICE_LINE_FLAGS_STYLED) {
        size_t size;

        replay_fscanf(replay, "attr.style_nseg %d\n", &temp); qxl->attr.style_nseg = temp;
        read_binary(replay, "style", &size, (uint8_t**)&qxl->attr.style, 0);
    }
    red_replay_brush_ptr(replay, &qxl->brush, flags);
    replay_fscanf(replay, "fore_mode %d\n", &temp); qxl->fore_mode = temp;
    replay_fscanf(replay, "back_mode %d\n", &temp); qxl->back_mode = temp;
}

static void red_replay_stroke_free(SpiceReplay *replay, QXLStroke *qxl, uint32_t flags)
{
    red_replay_path_free(replay, qxl->path);
    if (qxl->attr.flags & SPICE_LINE_FLAGS_STYLED) {
        free((void*)qxl->attr.style);
    }
    red_replay_brush_free(replay, &qxl->brush, flags);
}

static QXLString *red_replay_string(SpiceReplay *replay)
{
    int temp;
    uint32_t data_size;
    uint16_t length;
    uint16_t flags;
    size_t chunk_size;
    QXLString *qxl = NULL;

    replay_fscanf(replay, "data_size %d\n", &data_size);
    replay_fscanf(replay, "length %d\n", &temp); length = temp;
    replay_fscanf(replay, "flags %d\n", &temp); flags = temp;
    chunk_size = red_replay_data_chunks(replay, "string", (uint8_t**)&qxl, sizeof(QXLString));
    qxl->data_size = data_size;
    qxl->length = length;
    qxl->flags = flags;
    spice_assert(chunk_size == qxl->data_size);
    return qxl;
}

static void red_replay_string_free(SpiceReplay *replay, QXLString *qxl)
{
    red_replay_data_chunks_free(replay, qxl, sizeof(*qxl));
}

static void red_replay_text_ptr(SpiceReplay *replay, QXLText *qxl, uint32_t flags)
{
    int temp;

   qxl->str = (QXLPHYSICAL)red_replay_string(replay);
   red_replay_rect_ptr(replay, "back_area", &qxl->back_area);
   red_replay_brush_ptr(replay, &qxl->fore_brush, flags);
   red_replay_brush_ptr(replay, &qxl->back_brush, flags);
   replay_fscanf(replay, "fore_mode %d\n", &temp); qxl->fore_mode = temp;
   replay_fscanf(replay, "back_mode %d\n", &temp); qxl->back_mode = temp;
}

static void red_replay_text_free(SpiceReplay *replay, QXLText *qxl, uint32_t flags)
{
    red_replay_string_free(replay, (QXLString *)qxl->str);
    red_replay_brush_free(replay, &qxl->fore_brush, flags);
    red_replay_brush_free(replay, &qxl->back_brush, flags);
}

static void red_replay_whiteness_ptr(SpiceReplay *replay, QXLWhiteness *qxl, uint32_t flags)
{
    red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_whiteness_free(SpiceReplay *replay, QXLWhiteness *qxl, uint32_t flags)
{
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_blackness_ptr(SpiceReplay *replay, QXLBlackness *qxl, uint32_t flags)
{
    red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_blackness_free(SpiceReplay *replay, QXLBlackness *qxl, uint32_t flags)
{
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_invers_ptr(SpiceReplay *replay, QXLInvers *qxl, uint32_t flags)
{
    red_replay_qmask_ptr(replay, &qxl->mask, flags);
}

static void red_replay_invers_free(SpiceReplay *replay, QXLInvers *qxl, uint32_t flags)
{
    red_replay_qmask_free(replay, &qxl->mask, flags);
}

static void red_replay_clip_ptr(SpiceReplay *replay, QXLClip *qxl)
{
    replay_fscanf(replay, "type %d\n", &qxl->type);
    switch (qxl->type) {
    case SPICE_CLIP_TYPE_RECTS:
        qxl->data = (QXLPHYSICAL)red_replay_clip_rects(replay);
        break;
    }
}

static void red_replay_clip_free(SpiceReplay *replay, QXLClip *qxl)
{
    switch (qxl->type) {
    case SPICE_CLIP_TYPE_RECTS:
        red_replay_clip_rects_free(replay, (QXLClipRects*)qxl->data);
        break;
    }
}

static uint8_t *red_replay_transform(SpiceReplay *replay)
{
    uint8_t *data = NULL;
    size_t size;

    read_binary(replay, "transform", &size, &data, 0);
    spice_warn_if_fail(size == sizeof(SpiceTransform));

    return data;
}

static void red_replay_composite_ptr(SpiceReplay *replay, QXLComposite *qxl, uint32_t flags)
{
    int enabled;

    replay_fscanf(replay, "flags %d\n", &qxl->flags);
    qxl->src = (QXLPHYSICAL)red_replay_image(replay, flags);

    replay_fscanf(replay, "src_transform %d\n", &enabled);
    qxl->src_transform = enabled ? (QXLPHYSICAL)red_replay_transform(replay) : 0;

    replay_fscanf(replay, "mask %d\n", &enabled);
    qxl->mask = enabled ? (QXLPHYSICAL)red_replay_image(replay, flags) : 0;

    replay_fscanf(replay, "mask_transform %d\n", &enabled);
    qxl->mask_transform = enabled ? (QXLPHYSICAL)red_replay_transform(replay) : 0;

    replay_fscanf(replay, "src_origin %" SCNi16 " %" SCNi16 "\n", &qxl->src_origin.x, &qxl->src_origin.y);
    replay_fscanf(replay, "mask_origin %" SCNi16 " %" SCNi16 "\n", &qxl->mask_origin.x, &qxl->mask_origin.y);
}

static void red_replay_composite_free(SpiceReplay *replay, QXLComposite *qxl, uint32_t flags)
{
    red_replay_image_free(replay, qxl->src, flags);
    free((void*)qxl->src_transform);
    red_replay_image_free(replay, qxl->mask, flags);
    free((void*)qxl->mask_transform);

}

static QXLDrawable *red_replay_native_drawable(SpiceReplay *replay, uint32_t flags)
{
    QXLDrawable *qxl = malloc(sizeof(QXLDrawable)); // TODO - this is too large usually
    int i;
    int temp;

    red_replay_rect_ptr(replay, "bbox", &qxl->bbox);
    red_replay_clip_ptr(replay, &qxl->clip);
    replay_fscanf(replay, "effect %d\n", &temp); qxl->effect = temp;
    replay_fscanf(replay, "mm_time %d\n", &qxl->mm_time);
    replay_fscanf(replay, "self_bitmap %d\n", &temp); qxl->self_bitmap = temp;
    red_replay_rect_ptr(replay, "self_bitmap_area", &qxl->self_bitmap_area);
    replay_fscanf(replay, "surface_id %d\n", &qxl->surface_id);
    qxl->surface_id = replay_id_get(replay, qxl->surface_id);

    for (i = 0; i < 3; i++) {
        replay_fscanf(replay, "surfaces_dest %d\n", &qxl->surfaces_dest[i]);
        qxl->surfaces_dest[i] = replay_id_get(replay, qxl->surfaces_dest[i]);
        red_replay_rect_ptr(replay, "surfaces_rects", &qxl->surfaces_rects[i]);
    }

    replay_fscanf(replay, "type %d\n", &temp); qxl->type = temp;
    switch (qxl->type) {
    case QXL_DRAW_ALPHA_BLEND:
        red_replay_alpha_blend_ptr(replay, &qxl->u.alpha_blend, flags);
        break;
    case QXL_DRAW_BLACKNESS:
        red_replay_blackness_ptr(replay, &qxl->u.blackness, flags);
        break;
    case QXL_DRAW_BLEND:
        red_replay_blend_ptr(replay, &qxl->u.blend, flags);
        break;
    case QXL_DRAW_COPY:
        red_replay_copy_ptr(replay, &qxl->u.copy, flags);
        break;
    case QXL_COPY_BITS:
        red_replay_point_ptr(replay, &qxl->u.copy_bits.src_pos);
        break;
    case QXL_DRAW_FILL:
        red_replay_fill_ptr(replay, &qxl->u.fill, flags);
        break;
    case QXL_DRAW_OPAQUE:
        red_replay_opaque_ptr(replay, &qxl->u.opaque, flags);
        break;
    case QXL_DRAW_INVERS:
        red_replay_invers_ptr(replay, &qxl->u.invers, flags);
        break;
    case QXL_DRAW_NOP:
        break;
    case QXL_DRAW_ROP3:
        red_replay_rop3_ptr(replay, &qxl->u.rop3, flags);
        break;
    case QXL_DRAW_STROKE:
        red_replay_stroke_ptr(replay, &qxl->u.stroke, flags);
        break;
    case QXL_DRAW_TEXT:
        red_replay_text_ptr(replay, &qxl->u.text, flags);
        break;
    case QXL_DRAW_TRANSPARENT:
        red_replay_transparent_ptr(replay, &qxl->u.transparent, flags);
        break;
    case QXL_DRAW_WHITENESS:
        red_replay_whiteness_ptr(replay, &qxl->u.whiteness, flags);
        break;
    case QXL_DRAW_COMPOSITE:
        red_replay_composite_ptr(replay, &qxl->u.composite, flags);
        break;
    default:
        spice_warn_if_reached();
        break;
    };
    return qxl;
}

static void red_replay_native_drawable_free(SpiceReplay *replay, QXLDrawable *qxl, uint32_t flags)
{
    red_replay_clip_free(replay, &qxl->clip);

    switch (qxl->type) {
    case QXL_DRAW_ALPHA_BLEND:
        red_replay_alpha_blend_free(replay, &qxl->u.alpha_blend, flags);
        break;
    case QXL_DRAW_BLACKNESS:
        red_replay_blackness_free(replay, &qxl->u.blackness, flags);
        break;
    case QXL_DRAW_BLEND:
        red_replay_blend_free(replay, &qxl->u.blend, flags);
        break;
    case QXL_DRAW_COPY:
        red_replay_copy_free(replay, &qxl->u.copy, flags);
        break;
    case QXL_COPY_BITS:
        break;
    case QXL_DRAW_FILL:
        red_replay_fill_free(replay, &qxl->u.fill, flags);
        break;
    case QXL_DRAW_OPAQUE:
        red_replay_opaque_free(replay, &qxl->u.opaque, flags);
        break;
    case QXL_DRAW_INVERS:
        red_replay_invers_free(replay, &qxl->u.invers, flags);
        break;
    case QXL_DRAW_NOP:
        break;
    case QXL_DRAW_ROP3:
        red_replay_rop3_free(replay, &qxl->u.rop3, flags);
        break;
    case QXL_DRAW_STROKE:
        red_replay_stroke_free(replay, &qxl->u.stroke, flags);
        break;
    case QXL_DRAW_TEXT:
        red_replay_text_free(replay, &qxl->u.text, flags);
        break;
    case QXL_DRAW_TRANSPARENT:
        red_replay_transparent_free(replay, &qxl->u.transparent, flags);
        break;
    case QXL_DRAW_WHITENESS:
        red_replay_whiteness_free(replay, &qxl->u.whiteness, flags);
        break;
    case QXL_DRAW_COMPOSITE:
        red_replay_composite_free(replay, &qxl->u.composite, flags);
        break;
    default:
        spice_warn_if_reached();
        break;
    };

    free(qxl);
}

static QXLCompatDrawable *red_replay_compat_drawable(SpiceReplay *replay, uint32_t flags)
{
    int temp;
    QXLCompatDrawable *qxl = malloc(sizeof(QXLCompatDrawable)); // TODO - too large usually

    red_replay_rect_ptr(replay, "bbox", &qxl->bbox);
    red_replay_clip_ptr(replay, &qxl->clip);
    replay_fscanf(replay, "effect %d\n", &temp); qxl->effect = temp;
    replay_fscanf(replay, "mm_time %d\n", &qxl->mm_time);

    replay_fscanf(replay, "bitmap_offset %d\n", &temp); qxl->bitmap_offset = temp;
    red_replay_rect_ptr(replay, "bitmap_area", &qxl->bitmap_area);

    replay_fscanf(replay, "type %d\n", &temp); qxl->type = temp;
    switch (qxl->type) {
    case QXL_DRAW_ALPHA_BLEND:
        red_replay_alpha_blend_ptr_compat(replay, &qxl->u.alpha_blend, flags);
        break;
    case QXL_DRAW_BLACKNESS:
        red_replay_blackness_ptr(replay, &qxl->u.blackness, flags);
        break;
    case QXL_DRAW_BLEND:
        red_replay_blend_ptr(replay, &qxl->u.blend, flags);
        break;
    case QXL_DRAW_COPY:
        red_replay_copy_ptr(replay, &qxl->u.copy, flags);
        break;
    case QXL_COPY_BITS:
        red_replay_point_ptr(replay, &qxl->u.copy_bits.src_pos);
        break;
    case QXL_DRAW_FILL:
        red_replay_fill_ptr(replay, &qxl->u.fill, flags);
        break;
    case QXL_DRAW_OPAQUE:
        red_replay_opaque_ptr(replay, &qxl->u.opaque, flags);
        break;
    case QXL_DRAW_INVERS:
        red_replay_invers_ptr(replay, &qxl->u.invers, flags);
        break;
    case QXL_DRAW_NOP:
        break;
    case QXL_DRAW_ROP3:
        red_replay_rop3_ptr(replay, &qxl->u.rop3, flags);
        break;
    case QXL_DRAW_STROKE:
        red_replay_stroke_ptr(replay, &qxl->u.stroke, flags);
        break;
    case QXL_DRAW_TEXT:
        red_replay_text_ptr(replay, &qxl->u.text, flags);
        break;
    case QXL_DRAW_TRANSPARENT:
        red_replay_transparent_ptr(replay, &qxl->u.transparent, flags);
        break;
    case QXL_DRAW_WHITENESS:
        red_replay_whiteness_ptr(replay, &qxl->u.whiteness, flags);
        break;
    default:
        spice_error("%s: unknown type %d", __FUNCTION__, qxl->type);
        break;
    };
    return qxl;
}

static QXLPHYSICAL red_replay_drawable(SpiceReplay *replay, uint32_t flags)
{
    if (replay->eof) {
        return 0;
    }
    replay_fscanf(replay, "drawable\n");
    if (flags & QXL_COMMAND_FLAG_COMPAT) {
        return (QXLPHYSICAL)red_replay_compat_drawable(replay, flags);
    } else {
        return (QXLPHYSICAL)red_replay_native_drawable(replay, flags);
    }
}

static QXLUpdateCmd *red_replay_update_cmd(SpiceReplay *replay)
{
    QXLUpdateCmd *qxl = malloc(sizeof(QXLUpdateCmd));

    replay_fscanf(replay, "update\n");
    red_replay_rect_ptr(replay, "area", &qxl->area);
    replay_fscanf(replay, "update_id %d\n", &qxl->update_id);
    replay_fscanf(replay, "surface_id %d\n", &qxl->surface_id);
    qxl->surface_id = replay_id_get(replay, qxl->surface_id);

    return qxl;
}

static QXLMessage *red_replay_message(SpiceReplay *replay)
{
    QXLMessage *qxl;
    size_t size;

    read_binary(replay, "message", &size, (uint8_t**)&qxl, sizeof(QXLMessage));
    return qxl;
}

static QXLSurfaceCmd *red_replay_surface_cmd(SpiceReplay *replay)
{
    size_t size;
    size_t read_size;
    int temp;
    QXLSurfaceCmd *qxl = calloc(1, sizeof(QXLSurfaceCmd));

    replay_fscanf(replay, "surface_cmd\n");
    replay_fscanf(replay, "surface_id %d\n", &qxl->surface_id);
    replay_fscanf(replay, "type %d\n", &temp); qxl->type = temp;
    replay_fscanf(replay, "flags %d\n", &qxl->flags);

    switch (qxl->type) {
    case QXL_SURFACE_CMD_CREATE:
        replay_fscanf(replay, "u.surface_create.format %d\n", &qxl->u.surface_create.format);
        replay_fscanf(replay, "u.surface_create.width %d\n", &qxl->u.surface_create.width);
        replay_fscanf(replay, "u.surface_create.height %d\n", &qxl->u.surface_create.height);
        replay_fscanf(replay, "u.surface_create.stride %d\n", &qxl->u.surface_create.stride);
        size = qxl->u.surface_create.height * abs(qxl->u.surface_create.stride);
        if ((qxl->flags & QXL_SURF_FLAG_KEEP_DATA) != 0) {
            read_binary(replay, "data", &read_size, (uint8_t**)&qxl->u.surface_create.data, 0);
            if (read_size != size) {
                spice_printerr("mismatch %ld != %ld", size, read_size);
            }
        } else {
            qxl->u.surface_create.data = (QXLPHYSICAL)malloc(size);
        }
        qxl->surface_id = replay_id_new(replay, qxl->surface_id);
        break;
    case QXL_SURFACE_CMD_DESTROY:
        qxl->u.surface_create.data = (QXLPHYSICAL)NULL;
        qxl->surface_id = replay_id_get(replay, qxl->surface_id);
    }
    return qxl;
}

static void red_replay_surface_cmd_free(SpiceReplay *replay, QXLSurfaceCmd *qxl)
{
    if (qxl->type == QXL_SURFACE_CMD_DESTROY) {
        replay_id_free(replay, qxl->surface_id);
    }

    free((void*)qxl->u.surface_create.data);
    free(qxl);
}

static void replay_handle_create_primary(QXLWorker *worker, SpiceReplay *replay)
{
    QXLDevSurfaceCreate surface = { 0, };
    size_t size;
    uint8_t *mem = NULL;

    if (replay->created_primary) {
        spice_printerr(
            "WARNING: %d: original recording event not preceded by a destroy primary",
            replay->counter);
        worker->destroy_primary_surface(worker, 0);
    }
    replay->created_primary = TRUE;

    replay_fscanf(replay, "%d %d %d %d\n", &surface.width, &surface.height,
        &surface.stride, &surface.format);
    replay_fscanf(replay, "%d %d %d %d\n", &surface.position, &surface.mouse_mode,
        &surface.flags, &surface.type);
    read_binary(replay, "data", &size, &mem, 0);
    surface.group_id = 0;
    surface.mem = (QXLPHYSICAL)mem;
    worker->create_primary_surface(worker, 0, &surface);
}

static void replay_handle_dev_input(QXLWorker *worker, SpiceReplay *replay,
                                    RedWorkerMessage message)
{
    switch (message) {
    case RED_WORKER_MESSAGE_CREATE_PRIMARY_SURFACE:
    case RED_WORKER_MESSAGE_CREATE_PRIMARY_SURFACE_ASYNC:
        replay_handle_create_primary(worker, replay);
        break;
    case RED_WORKER_MESSAGE_DESTROY_PRIMARY_SURFACE:
        replay->created_primary = FALSE;
        worker->destroy_primary_surface(worker, 0);
        break;
    case RED_WORKER_MESSAGE_DESTROY_SURFACES:
        replay->created_primary = FALSE;
        worker->destroy_surfaces(worker);
        break;
    case RED_WORKER_MESSAGE_UPDATE:
        // XXX do anything? we record the correct bitmaps already.
    case RED_WORKER_MESSAGE_DISPLAY_CONNECT:
        // we want to ignore this one - it is sent on client connection, we
        // shall have our own clients
    case RED_WORKER_MESSAGE_WAKEUP:
        // safe to ignore
        break;
    default:
        spice_debug("unhandled %d\n", message);
    }
}

/*
 * NOTE: This reads from a saved file and performs all io actions, calling the
 * dispatcher, until it sees a command, at which point it returns it via the
 * last parameter [ext_cmd]. Hence you cannot call this from the worker thread
 * since it will block reading from the dispatcher pipe.
 */
SPICE_GNUC_VISIBLE QXLCommandExt* spice_replay_next_cmd(SpiceReplay *replay,
                                                         QXLWorker *worker)
{
    QXLCommandExt* cmd;
    uint64_t timestamp;
    int type;
    int what = -1;
    int counter;

    while (what != 0) {
        replay_fscanf(replay, "event %d %d %d %ld\n", &counter,
                            &what, &type, &timestamp);
        if (replay->eof) {
            return NULL;
        }
        if (what == 1) {
            replay_handle_dev_input(worker, replay, type);
        }
    }
    cmd = g_slice_new(QXLCommandExt);
    cmd->cmd.type = type;
    cmd->group_id = 0;
    spice_debug("command %ld, %d\r", timestamp, cmd->cmd.type);
    switch (cmd->cmd.type) {
    case QXL_CMD_DRAW:
        cmd->flags = 0;
        cmd->cmd.data = red_replay_drawable(replay, cmd->flags);
        break;
    case QXL_CMD_UPDATE:
        cmd->cmd.data = (QXLPHYSICAL)red_replay_update_cmd(replay);
        break;
    case QXL_CMD_MESSAGE:
        cmd->cmd.data = (QXLPHYSICAL)red_replay_message(replay);
        break;
    case QXL_CMD_SURFACE:
        cmd->cmd.data = (QXLPHYSICAL)red_replay_surface_cmd(replay);
        break;
    }

    QXLReleaseInfo *info;
    switch (cmd->cmd.type) {
    case QXL_CMD_DRAW:
    case QXL_CMD_UPDATE:
    case QXL_CMD_SURFACE:
        info = (QXLReleaseInfo *)cmd->cmd.data;
        info->id = (uint64_t)cmd;
    }

    replay->counter++;

    return cmd;
}

SPICE_GNUC_VISIBLE void spice_replay_free_cmd(SpiceReplay *replay, QXLCommandExt *cmd)
{
    spice_return_if_fail(replay);
    spice_return_if_fail(cmd);

    switch (cmd->cmd.type) {
    case QXL_CMD_DRAW: {
        // FIXME: compat flag must be save somewhere...
        spice_return_if_fail(cmd->flags == 0);
        QXLDrawable *qxl = (QXLDrawable *)cmd->cmd.data;
        red_replay_native_drawable_free(replay, qxl, cmd->flags);
        break;
    }
    case QXL_CMD_UPDATE: {
        QXLUpdateCmd *qxl = (QXLUpdateCmd *)cmd->cmd.data;
        free(qxl);
        break;
    }
    case QXL_CMD_SURFACE: {
        QXLSurfaceCmd *qxl = (QXLSurfaceCmd *)cmd->cmd.data;
        red_replay_surface_cmd_free(replay, qxl);
        break;
    }
    default:
        break;
    }

    g_slice_free(QXLCommandExt, cmd);
}

/* caller is incharge of closing the replay when done and releasing the SpiceReplay
 * memory */
SPICE_GNUC_VISIBLE
SpiceReplay *spice_replay_new(FILE *file, int nsurfaces)
{
    unsigned int version = 0;
    SpiceReplay *replay;

    spice_return_val_if_fail(file != NULL, NULL);

    if (fscanf(file, "SPICE_REPLAY %u\n", &version) == 1) {
        if (version > 1) {
            spice_warning("Replay file version unsupported");
            return NULL;
        }
    }

    replay = malloc(sizeof(SpiceReplay));

    replay->eof = 0;
    replay->fd = file;
    replay->created_primary = FALSE;
    pthread_mutex_init(&replay->mutex, NULL);
    pthread_cond_init(&replay->cond, NULL);
    replay->id_map = g_array_new(FALSE, FALSE, sizeof(uint32_t));
    replay->id_map_inv = g_array_new(FALSE, FALSE, sizeof(uint32_t));
    replay->id_free = g_array_new(FALSE, FALSE, sizeof(uint32_t));
    replay->nsurfaces = nsurfaces;

    /* reserve id 0 */
    replay_id_new(replay, 0);

    return replay;
}

SPICE_GNUC_VISIBLE void spice_replay_free(SpiceReplay *replay)
{
    spice_return_if_fail(replay != NULL);

    pthread_mutex_destroy(&replay->mutex);
    pthread_cond_destroy(&replay->cond);
    g_array_free(replay->id_map, TRUE);
    g_array_free(replay->id_map_inv, TRUE);
    g_array_free(replay->id_free, TRUE);
    fclose(replay->fd);
    free(replay);
}
