/* destination-only.c - "destination-only" format.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>		/* Gnulib (POSIX) */
#include <errno.h>  	 	/* C89 */
#include <stdlib.h> 	   	/* C89 */
#include <stdio.h>  	   	/* C89 */
#include <gettext.h> 		/* Gnulib (gettext) */
#define _(String) gettext(String)
#include "quotearg.h"		/* Gnulib */
#include "common/common.h"
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "common/error.h"
#include "common/llist.h"
#include "xalloc.h"		/* Gnulib */
#include "qcmd.h"

static bool parse_options(char *options);
static char *option_generator(const char *text, int state);
static void output(FILE *out, LList *files);
static bool input(FILE *in, LList *files);

static bool separate = false;
static char *option_array[] = {
    "help",
    "separate",
    NULL
};
enum {
    HELP_OPT,
    SEPARATE_OPT,
};

#define NO_ARGUMENT(name) \
    if (value != NULL) { \
	warn(_("suboption `%s' for --option does not take an argument"), #name); \
	reset_options(); \
	return false; \
    }

EditFormat destination_only_format = {
    "destination-only",
    "do",
    parse_options,
    option_generator,
    output,
    input
};

static void
reset_options()
{
    separate = false;
}

static bool
parse_options(char *options)
{
    reset_options();

    while (*options != '\0') {
	char *value;
	int c = getsubopt(&options, option_array, &value);

	if (c == -1) {
	    warn(_("unknown suboption for --option: %s"), quotearg(value));
	    reset_options();
	    return false;
	}

	switch (c) {
	case HELP_OPT:
	    printf(_("\
Available options for `dual-column' format:\n\
\n\
  separate          put a blank line between all renames\n"));
  	    return false;
	case SEPARATE_OPT:
	    NO_ARGUMENT(separate);
	    separate = true;
	    break;
	}
    }

    return true;
}

static char *
option_generator(const char *text, int state)
{
    static int c;
    if (state == 0)
	c = 0;
    while (option_array[c] != NULL) {
	char *name = option_array[c];
	c++;
	if (starts_with(name, text))
	    return xstrdup(name);
    }
    return NULL;
}

static void
output(FILE *out, LList *files)
{
    int c = 0;
    LListIterator it;

    for (llist_iterator(files, &it); it.has_next(&it); c++) {
	FileSpec *spec = it.next(&it);
	char *file;

	file = quote_output_file(spec->new_name);
	fprintf(out, "%s\n", file);
	if (separate && it.has_next(&it))
	    fprintf(out, "\n");
	free(file);
    }
}

static bool
input(FILE *in, LList *files)
{
    int c = 0;
    LListIterator it;

    for (llist_iterator(files, &it); it.has_next(&it); c++) {
	FileSpec *spec = it.next(&it);
	char *file_new;
	char *tmp;
		
	file_new = read_line(in);
	if (file_new == NULL) {
	    if (ferror(in))
		warn_errno(_("cannot read %s"), quotearg(edit_filename));
	    else
		warn(_("premature end of %s"), quotearg(edit_filename));
	    /* XXX: revert changes */
	    return false;
	}

	if (separate && it.has_next(&it)) {
	    char *empty_line;

	    empty_line = read_line(in);
	    if (empty_line == NULL) {
		if (ferror(in))
		    warn_errno(NULL);
		else
		    warn(_("premature end of %s"), quotearg(edit_filename));
		/* XXX: revert changes */
		return false;
	    }
	    chomp(empty_line);
	    if (empty_line[0] != '\0') {
		warn(_("expected empty line in file, got `%s'\n"), quotearg(empty_line));
		/* XXX: revert changes */
		return false;
	    }
	    free(empty_line);
	}

	chomp(file_new);

	tmp = dequote_output_file(file_new);
	free(file_new);
	file_new = tmp;

	if (file_new[0] == '\0') {
	    warn(_("new filename for %s is empty - not updating name"), quotearg(spec->old_name));
	    /* Don't return */
	} else {
	    spec->new_name = file_new;
	}
    }

    return true;
}
