/* dual-column.c - The "dual-column" edit format.
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

/* NOTE:
 * There will be major havoc if the indicators contain \n.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>  	    	    /* C89 */
#include <string.h> 	    	    /* C89 */
#include <stdlib.h> 	    	    /* C89 */
#include <stdio.h>  	    	    /* C89 */
#include <stdbool.h>	    	    /* Gnulib (POSIX) */
#include <gettext.h> 	    	    /* Gnulib (gettext) */
#define _(String) gettext(String)
#include <quotearg.h>	    	    /* Gnulib */
#include "common/common.h"
#include "common/strbuf.h"
#include "common/string-utils.h"
#include "common/io-utils.h"
#include "common/intutil.h"
#include "common/tab-utils.h"
#include "common/error.h"
#include "minmax.h"
#include "xalloc.h"			/* Gnulib */
#include "qcmd.h"

static void reset_options();
static bool parse_options(char *options);
static char *option_generator(const char *text, int state);
static char *quote_first_last(char *old_name);
static void output(FILE *out, LList *files);
static inline void free_unread_lines();
static void unread_line(char *line);
static char *read_unread_line(FILE *in);
static bool input(FILE *in, LList *files);
static inline void swap_if_swap(char **a, char **b);
static bool process_dual_line(FileSpec *spec, char *line1, char *line2);
static bool process_single_line(FileSpec *spec, char *line);

static LList *unread_lines = NULL;	//FIXME
static int real_width;

/* Variables representing option */
static uint16_t width = 40;
static uint16_t tabsize = 8;
static bool swap = false;
static bool use_spaces = false;
static bool separate = false;
static bool autowidth = true;
static char *indicator1 = "";
static char *indicator2 = "";

EditFormat dual_column_format = {
    "dual-column",
    "dc",
    parse_options,
    option_generator,
    output,
    input
};

static char *option_array[] = {
    "help",
    "swap",
    "separate",
    "spaces",
    "tabsize",
    "width",
    "autowidth",
    "indicator1",
    "indicator2",
    NULL
};

enum {
    HELP_OPT,
    SWAP_OPT,
    SEPARATE_OPT,
    SPACES_OPT,
    TABSIZE_OPT,
    WIDTH_OPT,
    AUTOWIDTH_OPT,
    INDICATOR1_OPT,
    INDICATOR2_OPT,
};

#define NO_ARGUMENT() \
    if (value != NULL) { \
	warn(_("suboption `%s' for --option does not take an argument"), option_array[c]); \
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
    use_spaces = false;
    tabsize = 8;
    width = 40;
    autowidth = true;
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
Available options for `dual-column' format:\n\
\n\
  swap              swap location of old and new names when editing\n\
  separate          put a blank line between all renames\n\
  tabsize=SIZE      width of tabs in the edit file\n\
  spaces            indent with spaces only instead of tabs and spaces\n\
  width=WIDTH       column index which the second file name starts at\n\
  autowidth         enable automatic calculation of the width (default)\n\
  indicator1=TEXT   text to put before the first file name\n\
  indicator2=TEXT   text to put before the second file name\n\
\n\
Note that if a width is specified before autowidth, that width is used\n\
as a minimum width that the calculated width will never go below.\n"));
	    return false;
	case SWAP_OPT:
	    NO_ARGUMENT();
	    swap = true;
	    break;
	case SEPARATE_OPT:
	    NO_ARGUMENT();
	    separate = true;
	    break;
	case SPACES_OPT:
	    NO_ARGUMENT();
	    use_spaces = true;
	    break;
	case TABSIZE_OPT:
	    REQUIRE_ARGUMENT();
	    if (!parse_uint16(value, &tabsize)) {
		warn(_("invalid number `%s' for tabsize suboption"), quotearg(value));
		reset_options();
		return false;
	    }
	    break;
	case WIDTH_OPT:
	    REQUIRE_ARGUMENT();
	    if (!parse_uint16(value, &width)) {
		warn(_("invalid number `%s' for width suboption"), quotearg(value));
		reset_options();
		return false;
	    }
	    autowidth = false;
	    break;
	case AUTOWIDTH_OPT:
	    NO_ARGUMENT();
	    autowidth = true;
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

static char *
quote_first_last(char *old_name)
{
    StrBuf *new_name;
    int c;
    int d;

    d = tab_index(old_name, real_width, tabsize);
    for (c = 0; c < d; c++) {
	if (old_name[c] != ' ' && old_name[c] != '\t')
	    break;
    }

    new_name = strbuf_new();
    if (real_width != 0 && c == d)
	strbuf_append_char(new_name, '\\');
    strbuf_append(new_name, old_name);
    if (real_width != 0 && old_name[strlen(old_name)-1] == ' ')
	strbuf_insert_char(new_name, strbuf_length(new_name)-1, '\\');

    free(old_name);
    return strbuf_free_to_string(new_name);
}

static void
output(FILE *out, LList *files)
{
    LListIterator it;
    LList *tmp = llist_new();
    int ilen = strlen(indicator1);
    char *old_name;
    char *new_name;

    real_width = width;

    /* Prepare list of old and new names */
    for (llist_iterator(files, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);

	old_name = quote_output_file(spec->old_name);
	old_name = quote_first_last(old_name);
	new_name = quote_output_file(spec->new_name);
	llist_add_last(tmp, old_name);
	llist_add_last(tmp, new_name);

	if (autowidth)
	    real_width = MAX(real_width, strlen(swap ? new_name : old_name)+1);
    }

    while (!llist_is_empty(tmp)) {
	old_name = llist_remove_first(tmp);
	new_name = llist_remove_first(tmp);

	if (strlen(old_name) >= real_width+ilen) {
	    fprintf(out, "%s%s\n", indicator1, old_name);
	    tab_to(out, 0, real_width, (use_spaces ? 0 : tabsize));
	    fprintf(out, "%s%s\n", indicator2, new_name);
	} else {
	    fprintf(out, "%s%s", indicator1, old_name);
	    tab_to(out, strlen(old_name)+ilen, real_width, (use_spaces ? 0 : tabsize));
	    fprintf(out, "%s%s\n", indicator2, new_name);
	}
	if (separate && !llist_is_empty(tmp))
	    fprintf(out, "\n");

	free(old_name);
	free(new_name);
    }

    llist_free(tmp);
}

static inline void
free_unread_lines()
{
    llist_iterate(unread_lines, free);
    llist_free(unread_lines);
}

static void
unread_line(char *line)
{
    if (line != NULL)
	llist_add_first(unread_lines, line);
}

static char *
read_unread_line(FILE *in)
{
    if (!llist_is_empty(unread_lines))
	return llist_remove_first(unread_lines);
    return read_line(in);
}

static bool
input(FILE *in, LList *files)
{
    LListIterator it;
    char *line1;
    char *line2;

    unread_lines = llist_new();
    for (llist_iterator(files, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	int c, d;

	line1 = read_unread_line(in);
	if (line1 == NULL) {
	    if (ferror(in))
		warn_errno(_("cannot read %s"), quotearg(edit_filename));
	    else
		warn(_("premature end of %s looking for %s"), quotearg(edit_filename), quotearg(spec->old_name));
	    free_unread_lines();
	    return false;
	}

	line2 = read_unread_line(in);
	if (line2 == NULL) {
	    if (ferror(in)) {
		warn_errno(_("cannot read %s"), quotearg(edit_filename));
		free(line1);
		free_unread_lines();
		return false;
	    }
	    if (it.has_next(&it)) {
		warn(_("premature end of %s looking for %s"), quotearg(edit_filename), quotearg(spec->old_name));
		free(line1);
		free_unread_lines();
		return false;
	    }
	    unread_line(line2);
	    if (!process_single_line(spec, line1)) {
		free(line1);
		free_unread_lines();
		return false;
	    }
	    free(line1);
	} else {
	    d = tab_index(line2, real_width, tabsize);
	    /* XXX: problem if indicator starts with at least width whitespace */
	    for (c = 0; c < d; c++) {
		if (line2[c] != ' ' && line2[c] != '\t')
		    break;
	    }

	    if (c != d) {
		unread_line(line2);
		if (!process_single_line(spec, line1)) {
		    free(line1);
		    free_unread_lines();
		    return false;
		}
		free(line1);
	    } else {
		if (!process_dual_line(spec, line1, line2)) {
		    free(line1);
		    free(line2);
		    free_unread_lines();
		    return false;
		}
		free(line1);
		free(line2);
	    }
	}

	if (separate && it.has_next(&it)) {
	    char *line = read_unread_line(in);
	    chomp(line);
	    if (strlen(line) != 0) {
		warn(_("expected empty line, got %s"), quotearg(line));
		free(line);
		free_unread_lines();
		return false;
	    }
	}

	/*printf("< [%s]\n> [%s]\n", spec->old_name, spec->new_name);*/
    }

    line1 = read_unread_line(in);
    if (line1 != NULL) {
	chomp(line1);
	warn(_("expected end of file, got %s"), quotearg(line1));
	free(line1);
	free_unread_lines();
	return false;
    }
    if (ferror(in)) {
	warn_errno(_("cannot read %s"), quotearg(edit_filename));
	free_unread_lines();
	return false;
    }

    if (!llist_is_empty(unread_lines))
	internal_error(_("unread lines not empty; found %s\n"), quotearg(llist_get_first(unread_lines)));

    free_unread_lines();

    return true;
}

static inline void
swap_if_swap(char **a, char **b)
{
    if (swap) {
	char *t = *a;
	*a = *b;
	*b = t;
    }
}

static bool
process_dual_line(FileSpec *spec, char *line1, char *line2)
{
    int c;
    int d;
    char *old_name;
    char *new_name;

    if (!starts_with(line1, indicator1)) {
	warn(_("first name of %s did not start with an indicator"), quotearg(spec->old_name));
	return false;
    }

    chomp(line1);
    chomp(line2);
    old_name = dequote_output_file(line1 + strlen(indicator1));
    string_strip_trailing(old_name, " \t");

    /* Check that characters up to the width' column are whitespace */
    d = tab_index(line2, real_width, tabsize);
    for (c = 0; c < d; c++) {
	if (line2[c] != '\t' && line2[c] != ' ') {
	    warn(_("unrecognized character when space expected in %s"), quotearg(spec->old_name));
	    return false;
	}
    }
    if (!starts_with(line2+d, indicator2)) {
	warn(_("second name did not start with indicator in %s"), quotearg(spec->old_name));
	return false;
    }
    new_name = dequote_output_file(line2 + d + strlen(indicator2));

    swap_if_swap(&old_name, &new_name);
    if (strcmp(spec->old_name, old_name) != 0) {
	warn(_("old name for %s does not match, is %s"), quotearg(spec->old_name), quotearg(old_name));
	return false;
    }
    free(old_name);

    spec->new_name = new_name;
    return true;
}

static bool
process_single_line(FileSpec *spec, char *line)
{
    int c;
    int d;
    char *old_name;
    char *new_name;

    if (!starts_with(line, indicator1)) {
	warn(_("first name of %s did not start with an indicator"), quotearg(spec->old_name));
	return false;
    }

    chomp(line);
    d = tab_index(line, real_width, tabsize);
    if (line[d] == '\0') {
	warn(_("destination name missing from line with %s"), quotearg(spec->old_name));
	return false;
    }

    for (c = d-1; line[c] == ' ' || line[c] == '\t'; c--);
    if (c == d-1) {
	warn(_("expected whitespace at width-1"));
	return false;
    }
    if (line[c] == '\\')
	c++;

    line[c+1] = '\0';
    old_name = dequote_output_file(line + strlen(indicator1));

    if (!starts_with(line+d, indicator2)) {
	warn(_("second name of %s did not start with an indicator"), quotearg(spec->old_name));
	return false;
    }
    new_name = dequote_output_file(line + d + strlen(indicator2));

    swap_if_swap(&old_name, &new_name);
    if (strcmp(spec->old_name, old_name) != 0) {
	warn(_("old name for %s does not match, is %s"), quotearg(spec->old_name), quotearg(old_name));
	return false;
    }
    free(old_name);

    spec->new_name = new_name;
    return true;
}
