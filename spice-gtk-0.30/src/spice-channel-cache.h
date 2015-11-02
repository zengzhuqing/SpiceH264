/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#ifndef SPICE_CHANNEL_CACHE_H_
# define SPICE_CHANNEL_CACHE_H_

#include <inttypes.h> /* For PRIx64 */
#include "common/mem.h"
#include "common/ring.h"

G_BEGIN_DECLS

typedef struct display_cache_item {
    guint64                     id;
    gboolean                    lossy;
    guint32                     ref_count;
} display_cache_item;

typedef struct display_cache {
    GHashTable  *table;
    gboolean    ref_counted;
}display_cache;

static inline display_cache_item* cache_item_new(guint64 id, gboolean lossy)
{
    display_cache_item *self = g_slice_new(display_cache_item);
    self->id = id;
    self->lossy = lossy;
    self->ref_count = 1;
    return self;
}

static inline void cache_item_free(display_cache_item *self)
{
    g_slice_free(display_cache_item, self);
}

static inline display_cache* cache_new(GDestroyNotify value_destroy)
{
    display_cache * self = g_slice_new(display_cache);
    self->table = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                                       (GDestroyNotify) cache_item_free,
                                       value_destroy);
    self->ref_counted = FALSE;
    return self;
}

static inline display_cache * cache_image_new(GDestroyNotify value_destroy)
{
    display_cache * self = cache_new(value_destroy);
    self->ref_counted = TRUE;
    return self;
};

static inline gpointer cache_find(display_cache *cache, uint64_t id)
{
    return g_hash_table_lookup(cache->table, &id);
}

static inline gpointer cache_find_lossy(display_cache *cache, uint64_t id, gboolean *lossy)
{
    gpointer value;
    display_cache_item *item;

    if (!g_hash_table_lookup_extended(cache->table, &id, (gpointer*)&item, &value))
        return NULL;

    *lossy = item->lossy;

    return value;
}

static inline void cache_add_lossy(display_cache *cache, uint64_t id,
                                   gpointer value, gboolean lossy)
{
    display_cache_item *item = cache_item_new(id, lossy);
    display_cache_item *current_item;
    gpointer            current_image;

    //If image is currently in the table add its reference count before replacing it
    if(cache->ref_counted) {
        if(g_hash_table_lookup_extended(cache->table, &id, (gpointer*) &current_item,
                                        (gpointer*) &current_image)) {
            item->ref_count = current_item->ref_count + 1;
        }
    }
    g_hash_table_replace(cache->table, item, value);
}

static inline void cache_add(display_cache *cache, uint64_t id, gpointer value)
{
    cache_add_lossy(cache, id, value, FALSE);
}

static inline gboolean cache_remove(display_cache *cache, uint64_t id)
{
    display_cache_item * item;
    gpointer value;

    if( g_hash_table_lookup_extended(cache->table, &id, (gpointer*) &item, &value)) {
        --item->ref_count;
        if(!cache->ref_counted || item->ref_count == 0 ) {
            return g_hash_table_remove(cache->table, &id);
        }
    }
    else {
        return FALSE;
    }
    return TRUE;
}

static inline void cache_clear(display_cache *cache)
{
    g_hash_table_remove_all(cache->table);
}

static inline void cache_unref(display_cache *cache)
{
    g_hash_table_unref(cache->table);
}

G_END_DECLS

#endif // SPICE_CHANNEL_CACHE_H_
