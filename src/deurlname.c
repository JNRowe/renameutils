/* deurlname.c - Remove URL-encoded characters from file names
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
#include <stdio.h>  	    	    /* C89 */
#include <string.h> 	    	    /* gnulib (C89) */
#include <stdlib.h> 	    	    /* C89 */
#include <ctype.h>  	    	    /* C89 */
#include <locale.h> 	    	    /* gnulib (POSIX) */
#include <getopt.h> 	    	    /* gnulib (POSIX) */
#include <stdbool.h>	    	    /* gnulib (POSIX) */
#include <gettext.h> 	    	    /* gnulib (gettext) */
#define _(String) gettext(String)
#define N_(String) (String)
#include "progname.h"	    	    /* gnulib */
#include "version-etc.h"	    /* gnulib */
#include "quotearg.h"	    	    /* gnulib */
#include "configmake.h"		    /* gnulib */
#include "common/error.h"
#include "common/strbuf.h"

#define PROGRAM "deurlname"

const char version_etc_copyright[] = 
    "Copyright (C) 2001, 2002, 2004, 2005, 2007, 2008 Oskar Liljeblad";

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
hexvalue (char c)
{
    if (c >= '0' && c <= '9')
    	return c-'0';
    if (c >= 'A' && c <= 'F')
    	return c-'A'+10;
    return c-'a'+10;
}

int
main (int argc, char **argv)
{
    StrBuf *newnamebuf;
    bool verbose = false;
    int c;
    int rc = EXIT_SUCCESS;

    set_program_name(argv[0]);

    if (setlocale(LC_ALL, "") == NULL)
    	warn(_("cannot set locale: %s\n"), errstr);
#ifdef ENABLE_NLS
    if (bindtextdomain(PACKAGE, LOCALEDIR) == NULL)
        warn(_("cannot bind message domain: %s\n"), errstr);
    if (textdomain(PACKAGE) == NULL)
    	warn(_("cannot set message domain: %s\n"), errstr);
#endif

    while (true) {
    	c = getopt_long (argc, argv, "v", option_table, NULL);
        if (c == -1)
            break;

        switch (c) {
        case '?':
            exit(EXIT_FAILURE);
        case 'v': /* --verbose */
            verbose = true;
            break;
        case VERSION_OPT:
	    version_etc(stdout, PROGRAM, PACKAGE, VERSION, "Oskar Liljeblad", NULL);
  	    exit(EXIT_SUCCESS);
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
  	    exit(EXIT_SUCCESS);
        }
    }

    if (optind >= argc)
    	die(_("missing file argument"));

    newnamebuf = strbuf_new();
    for (c = optind; c < argc; c++) {
    	char *name;
	char *basename;
	char *newname;
	int d;

    	name = argv[c];
    	strbuf_clear(newnamebuf);
	basename = strrchr(name, '/');
	d = 0;
	if (basename != NULL) {
	    strbuf_append_substring(newnamebuf, name, 0, basename-name+1);
	    d = basename-name+1;
	}
	for (; name[d] != '\0'; d++) {
    	    if (name[d] == '%' && isxdigit(name[d+1]) && isxdigit(name[d+2])) {
	    	char newch = (hexvalue(name[d+1]) << 4) | hexvalue(name[d+2]);
	    	if (newch != '\0' && newch != '/') {
		    strbuf_append_char(newnamebuf, newch);
	    	    d += 2;
	    	    continue;
                }
	    }
	    strbuf_append_char(newnamebuf, name[d]);
	}

        newname = strbuf_buffer(newnamebuf);
	if (strcmp(name, newname) != 0) {
	    if (verbose)
	        printf(_("%s => %s\n"), quotearg_n(0, name), quotearg_n(1, newname));
	    if (rename(name, newname) < 0) {
	        warn(_("cannot rename `%s' to `%s': %s"), quotearg_n(0, name), quotearg_n(1, newname), errstr);
                rc = EXIT_FAILURE;
            }
	}
    }

    strbuf_free(newnamebuf);
    exit(rc);
}
