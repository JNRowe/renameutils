/* display.c - Display plans.
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
#include <stdlib.h> 	    	/* C89 */
#include <string.h> 	    	/* gnulib (C89) */
#include <gettext.h> 	    	/* gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) (s)
#include "quotearg.h"	    	/* gnulib */
#include "common/error.h"
#include "qcmd.h"

void
display_names(FileSpec *spec)
{
    printf(_("%s -> %s\n"),
    	    quotearg_n(0, spec->old_name),
	    quotearg_n(1, spec->new_name));
}

static void
print_error_status(FileSpec *spec)
{
    switch (spec->status) {
    case STATUS_OLD_MISSING:
	puts(_("  Source file is missing, cannot rename/copy"));
	break;
    case STATUS_DUPLICATE:
	//if (spec->next_spec == NULL)
	puts(_("  Destination file names are the same, cannot rename/copy"));
	break;
    case STATUS_NEW_EXISTS:
	puts(_("  Destination file exists, cannot rename/copy"));
	break;
    default:
	internal_error(_("invalid status (%d) after order resolution\n"));
	break;
    }
}

static void
print_ok_status(FileSpec *spec)
{
    switch (spec->status) {
    case STATUS_CIRCULAR:
	//if (spec->next_spec == NULL)
	puts(_("  These renames were created due to circular renaming"));
	break;
    case STATUS_APPLY:
    	if (strcmp(program, "qmv") == 0)
	    puts(_("  Regular rename"));
	else
	    puts(_("  Regular copy"));
	break;
    default:
	internal_error(_("invalid status (%d) after order resolution\n"));
	break;
    }
}

void
display_plan(ApplyPlan *plan)
{
    LListIterator it;
    FileSpecStatus last_status = STATUS_UNCHECKED;
    FileSpec *last_spec = NULL;

    if (!llist_is_empty(plan->error)) {
	printf(_("Plan contains errors.\n\n"));
    } else if (llist_is_empty(plan->ok)) {
	printf(_("Plan is empty (no changes made).\n"));
    } else {
	printf(_("Plan is valid.\n\n"));
    }

    for (llist_iterator(plan->error, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (last_spec != NULL && last_status != spec->status)
	    print_error_status(spec);
	display_names(spec);
	last_status = spec->status;
	last_spec = spec;
    }
    if (last_spec != NULL)
	print_error_status(last_spec);

    if (!llist_is_empty(plan->error) && !llist_is_empty(plan->ok))
    	puts("");

    last_spec = NULL;
    for (llist_iterator(plan->ok, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (last_spec != NULL && last_status != spec->status)
	    print_ok_status(spec);
	display_names(spec);
	last_status = spec->status;
	last_spec = spec;
    }
    if (last_spec != NULL)
	print_ok_status(last_spec);

    puts("");
}
