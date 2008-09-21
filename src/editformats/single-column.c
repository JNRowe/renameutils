/* single-column.c - "single-column" format.
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
#include <errno.h>  	    	    /* C89 */
#include <stdlib.h> 	    	    /* C89 */
#include <stdio.h>  	    	    /* C89 */
#include <stdbool.h>	    	    /* Gnulib (POSIX) */
#include <gettext.h> 	    	    /* Gnulib (gettext) */
#include <string.h>		    /* C89 */
#define _(String) gettext(String)
#include <quotearg.h>	    	    /* Gnulib */
#include "common/common.h"
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "common/error.h"
#include "xalloc.h"			/* Gnulib */
#include "common/llist.h"
#include "qcmd.h"

static bool parse_options(char *options);
static char *option_generator(const char *text, int state);
static void output(FILE *out, LList *files);
static bool input(FILE *in, LList *files);

static bool swap = false;
static bool separate = false;
static char *indicator1 = "";
static char *indicator2 = "";

EditFormat single_column_format = {
    "single-column",
    "sc",
    parse_options,
    option_generator,
    output,
    input,
};

static char *option_array[] = {
    "help",
    "swap",
    "separate",
    "indicator1",
    "indicator2",
    NULL
};
enum {
    HELP_OPT,
    SWAP_OPT,
    SEPARATE_OPT,
    INDICATOR1_OPT,
    INDICATOR2_OPT
};

#define NO_ARGUMENT(name) \
    if (value != NULL) { \
	warn(_("suboption `%s' for --option does not take an argument"), #name); \
	reset_options(); \
	return false; \
    }
#define REQUIRE_ARGUMENT() \
    if (value == NULL) { \
	warn(_("missing argument for --option suboption `%s'"), option_array[c]); \
	reset_options(); \
	return false; \
    }

static void
reset_options()
{
    swap = false;
    separate = false;
    indicator1 = "";
    indicator2 = "";
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
Available options for `single-column' format:\n\
\n\
  swap              swap location of old and new names when editing\n\
  separate          put a blank line between all renames\n\
  indicator1=TEXT   text to put before the first file name\n\
  indicator2=TEXT   text to put before the second file name\n"));
	    return false;
	case SWAP_OPT:
	    NO_ARGUMENT(swap);
	    swap = true;
	    break;
	case SEPARATE_OPT:
	    NO_ARGUMENT(separate);
	    separate = true;
	    break;
	case INDICATOR1_OPT:
	    REQUIRE_ARGUMENT();
	    indicator1 = value;
	    break;
	case INDICATOR2_OPT:
	    REQUIRE_ARGUMENT();
	    indicator2 = value;
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
    LListIterator it;

    for (llist_iterator(files, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	char *file1;
	char *file2;

	file1 = quote_output_file(spec->old_name);
	file2 = quote_output_file(spec->new_name);
	fprintf(out, "%s%s\n", indicator1, file1);
	fprintf(out, "%s%s\n", indicator2, file2);
	if (separate && it.has_next(&it))
	    fprintf(out, "\n");
	free(file2);
	free(file1);
    }
}

static bool
input(FILE *in, LList *files)
{
    LListIterator it;
    int c = 0;

    for (llist_iterator(files, &it); it.has_next(&it); c++) {
	FileSpec *spec = it.next(&it);
	char *file_old;
	char *file_new;
	char *tmp;

	file_old = read_line(in);
	file_new = read_line(in);
	if (file_old == NULL || file_new == NULL) {
	    if (ferror(in))
		warn_errno(NULL);
	    else
		warn(_("premature end of %s"), quotearg(edit_filename));
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
		return false;
	    }
	    chomp(empty_line);
	    if (empty_line[0] != '\0') {
		warn(_("expected empty line in file, got `%s'\n"), quotearg(empty_line));
		return false;
	    }
	    free(empty_line);
	}

	chomp(file_old);
	chomp(file_new);

	if (!starts_with(file_old, indicator1)) {
	    warn(_("first name for %s did not start with the indicator"), quotearg(spec->old_name));
	    return false;
	}
	tmp = dequote_output_file(file_old + strlen(indicator1));
	free(file_old);
	file_old = tmp;
	if (!starts_with(file_new, indicator2)) {
	    warn(_("second name for %s did not start with the indicator"), quotearg(spec->old_name));
	    return false;
	}
	tmp = dequote_output_file(file_new + strlen(indicator2));
	free(file_new);
	file_new = tmp;

	if (swap) {
	    tmp = file_old;
	    file_old = file_new;
	    file_new = tmp;
	}

	if (strcmp(file_old, spec->old_name) != 0) {
	    warn(_("old filename for %s has been changed"), quotearg(spec->old_name));
	    /* Don't return */
	} else if (file_new[0] == '\0') {
	    warn(_("new filename is empty for %s"), quotearg(spec->old_name));
	    /* Don't return */
	} else {
	    spec->new_name = file_new;
	}
    }

    return true;
}
