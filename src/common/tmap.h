/* tmap.h - A red-black tree map implementation.
 *
 * Copyright (C) 1998-2005 Oskar Liljeblad
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef COMMON_TMAP_H
#define COMMON_TMAP_H

#include <stdbool.h>	/* Gnulib/C99/POSIX */
#include "comparison.h"

typedef struct _TMap TMap;
typedef struct _TMapIterator TMapIterator;

struct _TMapIterator {
    bool (*has_next)(TMapIterator *it);
    void *(*next)(TMapIterator *it);

    /* Private data follow */
    void *p0;
    void *p1;
};

#define tmap_is_empty(m) ((tmap_size(m) == 0)
TMap *tmap_new(void);
void tmap_free(TMap *map);
void tmap_set_compare_fn(TMap *map, comparison_fn_t comparator);
size_t tmap_size(TMap *map);
bool tmap_contains_key(TMap *map, const void *key);
void *tmap_first_key(TMap *map);
void *tmap_last_key(TMap *map);
void *tmap_first_value(TMap *map);
void *tmap_last_value(TMap *map);
void *tmap_get(TMap *map, const void *key);
void *tmap_put(TMap *map, void *key, void *value);
void *tmap_remove(TMap *map, const void *key);
void tmap_iterator(TMap *map, TMapIterator *it);
bool tmap_iterator_partial(TMap *map, TMapIterator *it, const void *match, comparison_fn_t comparator);
void tmap_clear(TMap *map);
void tmap_foreach_key(TMap *map, void (*iterator)());
void tmap_foreach_value(TMap *map, void (*iterator)());

#ifdef ENABLE_TMAP_TESTING
#include <stdio.h>
void tmap_dump(TMap *map, FILE *out);
bool tmap_verify(TMap *map);
#endif

#endif
