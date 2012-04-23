/* variables.c - Variable management for interactive qmv/qcp.
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
#include <string.h> 	    	    /* gnulib (C89) */
#include <stdio.h>  	    	    /* C89 */
#include <stdbool.h>	    	    /* gnulib (POSIX) */
#include <gettext.h> 	    	    /* gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) gettext(s)
#include "quotearg.h"	    	    /* gnulib */
#include "xalloc.h"		    /* gnulib */
#include "common/string-utils.h"
#include "qcmd.h"

static inline bool
parse_boolean(const char *value, bool *bool_value)
{
    if (strcmp(value, "0") == 0
		|| strcasecmp(value, "false") == 0
		|| strcasecmp(value, "no") == 0
		|| strcasecmp(value, "off") == 0) {
	*bool_value = false;
	return true;
    }
    if (strcmp(value, "1") == 0
		|| strcasecmp(value, "true") == 0
		|| strcasecmp(value, "yes") == 0
		|| strcasecmp(value, "on") == 0) {
	*bool_value = true;
	return true;
    }

    printf(_("invalid boolean value `%s' - should be either `yes' or `no'\n"), quotearg(value));
    return false;
}

void
set_command(char **args)
{
    if (args[1] == NULL) {
	show_command(args);
	return;
    }
    if (args[2] == NULL) {
	printf(_("missing value argument\n"));
	return;
    }

    if (strcmp(args[1], "simulate") == 0) {
        bool value;
        if (parse_boolean(args[2], &value))
    	    simulate = value;
    }
    else if (strcmp(args[1], "editor") == 0) {
    	editor_program = xstrdup(args[2]);
    }
    else if (strcmp(args[1], "format") == 0) {
    	EditFormat *new_format = find_edit_format_by_name(args[2]);
	if (new_format == NULL)
	    printf(_("no such edit format `%s'\n"), quotearg(args[2]));
	else
	    format = new_format;
    }
    else if (strcmp(args[1], "tempfile") == 0) {
	printf(_("tempfile variable is read-only\n"));
    }
    else if (strcmp(args[1], "options") == 0) {
    	format->parse_options(args[2]);
    }
    else if (strcmp(args[1], "ls") == 0) {
        ls_program = xstrdup(args[2]);
    }
}

static void
show_variable(char *name)
{
    if (strcmp(name, "simulate") == 0) {
	printf("%s=%s\n", name, simulate ? "true" : "false");
    }
    else if (strcmp(name, "editor") == 0) {
	printf("%s=%s\n", name, editor_program);
    }
    else if (strcmp(name, "tempfile") == 0) {
	printf("%s=%s\n", name, edit_filename);
    }
    else if (strcmp(name, "options") == 0) {
	printf(_("Write-only variable\n"));
    }
    else if (strcmp(name, "ls") == 0) {
        printf("%s=%s\n", name, ls_program);
    }
}

void
show_command(char **args)
{
    if (args[1] == NULL) {
	show_variable("simulate");
	show_variable("editor");
	show_variable("tempfile");
    } else {
	show_variable(args[1]);
    }
}

char *
variable_generator(const char *text, int state)
{
    static int c;
    static char *variables[] = {
	"simulate",
	"editor",
	"format",
	"tempfile",
	"options"
    };

    if (state == 0)
	c = 0;

    while (c < sizeof(variables)/sizeof(*variables)) {
    	char *name = variables[c];

	c++;
	if (starts_with(name, text))
    	    return xstrdup(name);
    }

    return NULL;
}
