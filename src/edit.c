/* edit.c - Editing the file names.
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
#include <string.h> 	    	    	/* gnulib (C89) */
#include <stdlib.h> 	    	    	/* C89 */
#include <stdio.h>  	    	    	/* C89 */
#include <stdbool.h>	    	    	/* gnulib (POSIX) */
#include <gettext.h> 	    	    	/* gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) (s)
#include "quotearg.h"	    	    	/* gnulib */
#include "xalloc.h"			/* gnulib */
#include "xvasprintf.h"			/* gnulib */
#include "common/string-utils.h"
#include "common/io-utils.h"
#include "common/error.h"
#include "common/llist.h"
#include "qcmd.h"

LList *edit_file_list;
static bool accept_reedit = false;
static EditFormat *formats[] = {
    &destination_only_format,
    &dual_column_format,
    &single_column_format
};

/* Find an edit format by its long or short name.
 * Return NULL if no format was found.
 */
EditFormat *
find_edit_format_by_name(const char *name)
{
    int c;

    for (c = 0; c < sizeof(formats)/sizeof(*formats); c++) {
	if (strcmp(name, formats[c]->name) == 0
	    	|| strcmp(name, formats[c]->short_name) == 0)
	    return formats[c];
    }

    return NULL;
}

/* A readline completion generator for the various edit formats.
 */
char *
edit_format_generator(const char *text, int state)
{
    static int c;

    if (state == 0)
	c = 0;

    while (c < sizeof(formats)/sizeof(*formats)) {
	char *name = formats[c]->name;
	c++;
	if (starts_with(name, text))
	    return xstrdup(name);
    }

    return NULL;
}

bool
edit_files(bool all, bool force)
{
    LListIterator it;
    FILE *file;
    char *cmd;
    int rc;

    if (!accept_reedit || force) {
	accept_reedit = false;

	/* Determine what files we want to display to user */
	llist_clear(edit_file_list);
	if (!all) {
    	    if (plan != NULL && llist_size(plan->error) != 0)
		llist_add_all(edit_file_list, plan->error);
	} else {
    	    llist_add_all(edit_file_list, work_files);
	}

    	if (llist_is_empty(edit_file_list)) {
	    warn(_("There are no previous files to edit, use `edit all' to edit all."));
	    return false;
	}

	/* Prepare the edit file */
	file = fopen(edit_filename, "w");
	if (file == NULL) {
	    warn(_("cannot create `%s': %s"), quotearg(edit_filename), errstr);
	    llist_clear(edit_file_list);
	    return false;
	}

	/* Write file names to the output file */
	format->output(file, edit_file_list);
	if (fclose(file) != 0) {
	    warn(_("cannot close `%s': %s"), quotearg(edit_filename), errstr);
	    llist_clear(edit_file_list);
	    return false;
	}

	accept_reedit = true;
    }

    /* Start the editor */
    cmd = xasprintf("%s %s", editor_program, edit_filename);
    /* Fortunately, edit_filename usually does not contain
     * special characters and does not start with a dash. */
    rc = system(cmd);
    if (rc == -1) {
	warn(_("cannot start editor - %s"), quotearg(editor_program), errstr);
	free(cmd); /* May clobber errno */
	return false;
    }
    free(cmd); /* May clobber errno */
    if (rc != 0) {
	warn(_("editor exited with status %d"), quotearg(editor_program), rc);
	return false;
    }

    /* Open the file */
    file = fopen(edit_filename, "r");
    if (file == NULL) {
	warn(_("cannot open `%s' for reading: %s"), quotearg(edit_filename), errstr);
	return false;
    }

    /* Read the file */
    if (!format->input(file, edit_file_list))
	return false;
    if (fclose(file) != 0)
	warn(_("cannot close `%s': %s"), quotearg(edit_filename), errstr);
    accept_reedit = false;

    /* Reset status of all files. */
    for (llist_iterator(edit_file_list, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	spec->status = STATUS_UNCHECKED;
    }
    llist_clear(edit_file_list);

    return true;
}
