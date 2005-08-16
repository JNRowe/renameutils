/* deurlname.c - Remove URL-encoded characters from file names
 *
 * Copyright (C) 2001, 2002, 2004, 2005 Oskar Liljeblad
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
#include <gettext.h> 	    	    /* Gnulib (gettext) */
#define _(String) gettext(String)
#define N_(String) (String)
#include <stdio.h>  	    	    /* C89 */
#include <string.h> 	    	    /* C89 */
#include <stdlib.h> 	    	    /* C89 */
#include <ctype.h>  	    	    /* C89 */
#include <locale.h> 	    	    /* C89 */
#include <progname.h>	    	    /* gnulib */
#include <version-etc.h>	    /* gnulib */
#include <quotearg.h>	    	    /* gnulib */
#include <getopt.h> 	    	    /* gnulib (POSIX) */
#include <stdbool.h>	    	    /* gnulib (POSIX) */
#include "common/error.h"
#include "common/strbuf.h"

#define PROGRAM "deurlname"

const char version_etc_copyright[] = "Copyright (C) 2001, 2002, 2004, 2005 Oskar Liljeblad";

enum {
  VERSION_OPT = 256,
  HELP_OPT
};

static struct option option_table[] = {
    { "verbose", no_argument, NULL, 'v' },
    { "version", no_argument, NULL, VERSION_OPT },
    { "help",    no_argument, NULL, HELP_OPT    },
    { 0 }
};

static inline int
hexvalue(char c)
{
    if (c >= '0' && c <= '9')
    	return c-'0';
    if (c >= 'A' && c <= 'F')
    	return c-'A'+10;
    return c-'a'+10;
}

int
main(int argc, char **argv)
{
    StrBuf *newname;
    bool verbose = false;
    int c;
    int rc = 0;

    set_program_name(argv[0]);

    if (setlocale (LC_ALL, "") == NULL)
    	die(_("invalid locale"));
    if (bindtextdomain (PACKAGE, LOCALEDIR) == NULL)
    	die_errno(NULL);
    if (textdomain (PACKAGE) == NULL)
    	die_errno(NULL);

    while (true) {
    	c = getopt_long (argc, argv, "v", option_table, NULL);
        if (c == -1)
            break;

        switch (c) {
        case '?':
            exit(1);
        case 'v': /* --verbose */
            verbose = true;
            break;
        case VERSION_OPT:
	    version_etc(stdout, PROGRAM, PACKAGE, VERSION, "Oskar Liljeblad", NULL);
  	    exit(0);
        case HELP_OPT:
	    printf(_("Usage: %s [OPTION]... [FILE]...\n\
Remove URL-encoded characters from file names.\n\
\n\
Options:\n\
  -v, --verbose              display every rename done\n\
      --help                 display this help and exit\n\
      --version              output version information and exit\n\
\n\
The encoded slash character (%%2f) is left untouched in file names.\n"), program_name);
            printf(_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
  	    exit(0);
        }
    }

    if (optind >= argc)
    	die(_("missing file argument"));

    newname = strbuf_new();
    for (c = optind; c < argc; c++) {
    	char *name;
	char *basename;
	int d;

    	name = argv[c];
    	strbuf_clear(newname);
	basename = strrchr(name, '/');
	d = 0;
	if (basename != NULL) {
	    strbuf_append_substring(newname, name, 0, basename-name+1);
	    d = basename-name+1;
	}
	for (; name[d] != '\0'; d++) {
    	    if (name[d] == '%' && isxdigit(name[d+1]) && isxdigit(name[d+2])) {
	    	char newch = (hexvalue(name[d+1]) << 4) | hexvalue(name[d+2]);
	    	if (newch != '\0' && newch != '/') {
		    strbuf_append_char(newname, newch);
	    	    d += 2;
	    	    continue;
                }
	    }
	    strbuf_append_char(newname, name[d]);
	}

	if (strcmp(name, strbuf_buffer(newname)) != 0) {
	    if (verbose)
	        printf(_("%s => "), quotearg(name));
		printf(_("%s\n"), quotearg(strbuf_buffer(newname)));
	    if (rename(name, strbuf_buffer(newname)) < 0) {
	        warn_errno(_("cannot rename `%s'"), quotearg(name));
                rc = 1;
            }
	}
    }

    strbuf_free(newname);
    exit(rc);
}
