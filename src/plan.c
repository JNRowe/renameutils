/* plan.c - Checking renames and resolving rename order.
 *
 * Copyright (C) 2001, 2002, 2004, 2005, 2007, 2008 Oskar Liljeblad
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdlib.h> 	    		/* C89 */
#include <string.h> 	    		/* gnulib (C89) */
#include <stdbool.h>	    		/* gnulib (POSIX) */
#include <gettext.h> 	    		/* gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) (s)
#include "xalloc.h"			/* gnulib */
#include "xvasprintf.h"			/* gnulib */
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "common/common.h"
#include "common/error.h"
#include "common/llist.h"
#include "common/hmap.h"
#include "qcmd.h"

/* Find renames that have the same destination filename.
 * Mark these as STATUS_DUPLICATE.
 */
static void
duplicate_scan(LList *renames, ApplyPlan *plan)
{
    HMap *map = hmap_new();
    LListIterator it;

    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);

    	if (spec->status == STATUS_UNCHECKED) {
	    FileSpec *spec2;

	    spec2 = hmap_put(map, spec->new_name, spec);
	    if (spec2 != NULL) {
		if (spec2->status == STATUS_UNCHECKED) {
		    spec2->status = STATUS_DUPLICATE;
	    	    llist_add(plan->error, spec2);
		}
		spec->status = STATUS_DUPLICATE;
		llist_add(plan->error, spec);
    	    }
	}
    }

    hmap_free(map);
}

/* Sort renames topologically, so that their destination name
 * won't exist when they are to be renamed.
 */
static void
topologic_sort(LList *renames, ApplyPlan *plan)
{
    LList *pending = llist_new();
    HMap *available = hmap_new();
    LListIterator it;
    bool changed;

    /* Find all renames where destination file does not exist. */
    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status == STATUS_UNCHECKED) {
	    char *fullname = cat_files(work_directory, spec->new_name);
	    if (!file_exists(fullname)) {
		hmap_put(available, spec->old_name, spec);
		spec->status = STATUS_APPLY;
		llist_add(plan->ok, spec);
	    } else {
		llist_add(pending, spec);
	    }
	    free(fullname);
	}
    }

    /* Topologic sorting */
    do {
	changed = false;
	for (llist_iterator(pending, &it); it.has_next(&it); ) {
	    FileSpec *spec = it.next(&it);
	    FileSpec *depend = hmap_get(available, spec->new_name);
	    if (depend != NULL) {
		it.remove(&it);
		spec->prev_spec = depend;
		depend->next_spec = spec;
		hmap_put(available, spec->old_name, spec);
		hmap_remove(available, spec->new_name);
		spec->status = STATUS_APPLY;
		llist_add(plan->ok, spec);
		changed = true;
	    }
	}
    } while (changed);

    llist_free(pending);
    hmap_free(available);
}

/* Find all renames where the old file is missing.
 * There is no guarantee that the old file will go
 * missing later (say, a microsecond after we
 * did the file_exists check), but it is probably
 * of some guidance to the user.
 *
 * XXX: Technically, there are other checks similar
 * to this one. For example, maybe the new file
 * is on a different filesystem? Or the new file
 * is a subdirectory of the old file (assuming the
 * old file is a directory)?
 */
static void
old_missing_scan(LList *renames, ApplyPlan *plan)
{
    LListIterator it;

    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status == STATUS_UNCHECKED) {
	    char *fullname = cat_files(work_directory, spec->old_name);
	    if (!file_exists(fullname)) {
		spec->status = STATUS_OLD_MISSING;
		llist_add(plan->error, spec);
	    }
	    free(fullname);
	}
    }
}

/* For each spec, if destination is found among sources
 * then that spec is an error.
 * This is only useful in qcp mode.
 */
static void
new_exists_scan(LList *renames, ApplyPlan *plan)
{
    HMap *map;
    LListIterator it;

    map = hmap_new();

    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status == STATUS_UNCHECKED)
	    hmap_put(map, spec->old_name, spec);
    }

    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
    	if (spec->status == STATUS_UNCHECKED) {
	    FileSpec *s2;
	    s2 = hmap_get(map, spec->new_name);
	    if (s2 != NULL) {
		spec->status = STATUS_NEW_EXISTS;
		llist_add(plan->error, spec);
	    }
	}
    }

    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
    	if (spec->status == STATUS_UNCHECKED) {
	    char *fullname = cat_files(work_directory, spec->new_name);
	    if (file_exists(fullname)) {
	    	spec->status = STATUS_NEW_EXISTS;
	    	llist_add(plan->error, spec);
	    }
	    free(fullname);
	}
    }

    hmap_free(map);
}

/* Remaining pending files are either cross-referenced
 * (e.g. x->y, y->x), or can't be renamed because the
 * destination file exists.
 */
static void
cross_reference_resolve(LList *renames, ApplyPlan *plan)
{
    HMap *map_old = hmap_new();
    HMap *map_new = hmap_new();
    LListIterator it;

    /* Put old names for all remaining files into the table. */
    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status == STATUS_UNCHECKED) {
	    hmap_put(map_old, spec->old_name, spec);
	    hmap_put(map_new, spec->new_name, spec);
	} else if (spec->status == STATUS_APPLY) {
	    hmap_put(map_new, spec->new_name, spec);
	}
    }

    /* Follow the renamings from new to old. */
    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *s1 = it.next(&it);
	FileSpec *s2 = s1;
	FileSpec *prev_circular = s1;
	LListIterator it2;
	int tmp_count = 1;
	char *tmp_name;
	char *fullname = NULL;

	/* Rename has already been processed. Ignore it. */
	if (s1->status != STATUS_UNCHECKED)
	    continue;

	/* Follow the renamings from new to old, until we reach the rename
	 * we started from, or a dead end (i.e. destination exists). We can
	 * be sure this is not going to loop forever, because no two
	 * renames can have the same destination name. */
	for (;;) {
	    s2 = hmap_get(map_old, s2->new_name);

	    /* Destination file already exists. */
	    if (s2 == NULL || s2->status != STATUS_UNCHECKED) {
		s1->next_spec = NULL;
		break;
	    }
	    /* We have found a circular rename-set. */
	    if (s2 == s1) {
		prev_circular->next_spec = NULL;
		break;
	    }
	    prev_circular->next_spec = s2;
	    prev_circular = s2;
	}

	/* This was not a cross-reference. The destination already
	 * existed. */
	if (s1->next_spec == NULL) {
	    s1->status = STATUS_NEW_EXISTS;
	    llist_add(plan->error, s1);
	    continue;
	}

	/* Find a temporary name for the new rename */
	do {
	    if (fullname != NULL)
		free(fullname);
	    tmp_name = xasprintf("%s-%d", s1->old_name, tmp_count++);
	    fullname = cat_files(work_directory, tmp_name);
	} while (file_exists(fullname) || hmap_contains_key(map_new, tmp_name));
	free(fullname);

	/* Add and update status of selected files (reverse order). */
	s2 = new_file_spec();
        /*llist_add(plan->free_spec, s2);*/
	llist_add_first(plan->ok, s2);
	s2->status = STATUS_CIRCULAR;
	s2->old_name = xstrdup(s1->old_name);
	s2->new_name = xstrdup(s1->new_name);
	s2->next_spec = s1->next_spec;
	s2->prev_spec = s1->prev_spec;
	s1->status = STATUS_APPLY;
	s1->next_spec = NULL;
	s1->prev_spec = NULL;
	s1 = s2;

	for (s2 = s1->next_spec; s2 != NULL; s2 = s2->next_spec) {
	    s2->status = STATUS_CIRCULAR;
	    llist_add_first(plan->ok, s2);
	}

	/* Add the extra required rename. */
	s2 = new_file_spec();
        /*llist_add(plan->free_spec, s2);*/
	llist_add_first(plan->ok, s2);
	s2->status = STATUS_CIRCULAR;
	s2->old_name = s1->old_name;
	s2->new_name = xstrdup(tmp_name);
	s1->old_name = xstrdup(tmp_name);

	/* Update the next_spec field of these circular renames */
	llist_iterator(plan->ok, &it2);
	it2.next(&it2); /* skip first - that's s2 */
	while (it.has_next(&it2)) {
	    FileSpec *spec = it.next(&it2);
	    s2->next_spec = spec;
	    spec->prev_spec = s2;
	    if (spec == s1)
		break;
	    s2 = spec;
	}
	s1->next_spec = NULL;
    }

    hmap_free(map_new);
    hmap_free(map_old);
}

/* Find all renames that have no changes.
 */
static void
no_change_scan(LList *renames, ApplyPlan *plan)
{
    LListIterator it;

    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status == STATUS_UNCHECKED) {
	    if (strcmp(spec->old_name, spec->new_name) == 0) {
		spec->status = STATUS_NO_CHANGE;
		llist_add(plan->no_change, spec);
	    }
	}
    }
}

/* Mark all with status UNCHECKED as APPLY, and
 * add them to ok list in plan. This is the last thing
 * done. Some may already be in the ok list.
 */
static void
apply_unchecked(LList *renames, ApplyPlan *plan)
{
    LListIterator it;

    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status == STATUS_UNCHECKED) {
	    spec->status = STATUS_APPLY;
	    llist_add(plan->ok, spec);
	}
    }
}

static void
previous_scan(LList *renames, ApplyPlan *plan)
{
    LListIterator it;
    
    for (llist_iterator(renames, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status == STATUS_APPLY || spec->status == STATUS_CIRCULAR)
	    llist_add(plan->ok, spec);
    	else if (spec->status == STATUS_NO_CHANGE)
	    llist_add(plan->no_change, spec);
	else if (spec->status != STATUS_UNCHECKED)
	    llist_add(plan->error, spec);

    }
}

ApplyPlan *
make_plan(LList *renames)
{
    ApplyPlan *plan;

    plan = xmalloc(sizeof(ApplyPlan));
    plan->ok = llist_new();
    plan->error = llist_new();
    plan->no_change = llist_new();
    /*plan->free_spec = llist_new();*/

    previous_scan(renames, plan);
    duplicate_scan(renames, plan);
    old_missing_scan(renames, plan);
    no_change_scan(renames, plan);
    if (strcmp(program, "qmv") == 0) {
	topologic_sort(renames, plan);
	cross_reference_resolve(renames, plan);
    } else if (strcmp(program, "qcp") == 0) {
	new_exists_scan(renames, plan);
    }
    apply_unchecked(renames, plan);

    return plan;
}

void
free_plan(ApplyPlan *plan)
{
    llist_free(plan->ok);
    llist_free(plan->error);
    llist_free(plan->no_change);
    /*llist_iterate(plan->free_spec, free_file_spec);
    llist_free(plan->free_spec);*/
    free(plan);
}
