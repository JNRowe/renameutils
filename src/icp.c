/* icp.c - Interactive cp(1).
 *
 * Copyright (C) 2001-2005 Oskar Liljeblad
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
#if HAVE_UNISTD_H
#include <unistd.h> 	    	    /* POSIX */
#endif
#include <stdlib.h> 	    	    /* C89 */
#include <signal.h> 	    	    /* C89 */
#include <stdio.h>  	    	    /* C89 */
#include <errno.h>  	    	    /* C89 */
#include <locale.h> 	    	    /* C89 */
#include <stdbool.h>	    	    /* Gnulib (POSIX) */
#include <progname.h>	    	    /* Gnulib */
#include <quotearg.h>	    	    /* Gnulib */
#include <yesno.h>  	    	    /* Gnulib */
#include <version-etc.h>    	    /* Gnulib */
#include <gettext.h>	    	    /* Gnulib (gettext) */
#define _(String) gettext(String)
#define N_(String) (String)
#include <readline/readline.h>	    /* Readline */
#include <readline/history.h>	    /* Readline */
#include "xalloc.h"
#include "common/error.h"
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "common/common.h"

#define PROGRAM "icp"

static char *sourcefile = NULL;
static char *first_text = NULL;
const char version_etc_copyright[] = N_("Copyright (C) 2001-2004 Oskar Liljeblad");

static int
insert_first_text(void)
{
    return rl_insert_text(first_text);
}

static void
int_signal_handler(int signal)
{
    puts("");
    warn(_("no changes made"));
    exit(0);
}

static void
display_imv_help(void)
{
    printf(_("Usage: %s [OPTION]... FILE\n\
Copy a file by editing the destination name using GNU readline.\n\
All options except the options listed below are passed to cp(1).\n\
\n\
      --help                 display this help and exit\n\
      --version              output version information and exit\n\
\n\
Report bugs to <%s>.\n"), program_name, PACKAGE_BUGREPORT);
}

int
main(int argc, char **argv)
{
    struct sigaction action;
    char *newname;
    bool gotend = false;
    int c;
    int len;

    set_program_name(argv[0]);

    if (setlocale (LC_ALL, "") == NULL)
    	die(_("invalid locale"));
    if (bindtextdomain (PACKAGE, LOCALEDIR) == NULL)
    	die_errno(NULL);
    if (textdomain (PACKAGE) == NULL)
    	die_errno(NULL);

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
	display_imv_help();
	exit(0);
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
	/*version_etc_copyright = N_("Copyright (C) 2001-2004 Oskar Liljeblad");*/
	version_etc(stdout, PROGRAM, PACKAGE, VERSION, "Oskar Liljeblad", NULL);
	exit(0);
    }

    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = int_signal_handler;
    action.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &action, NULL) < 0)
	die_errno(NULL);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGQUIT, &action, NULL) < 0)
	die_errno(NULL);

    for (c = 1; c < argc; c++) {
	if (!gotend && strcmp(argv[c], "--") == 0) {
	    gotend = true;
	} else if (gotend || argv[c][0] != '-') {
	    if (sourcefile != NULL)
		die(_("too many arguments"));
	    sourcefile = argv[c];
	}
    }

    if (sourcefile == NULL)
	die(_("missing file argument"));

    len = strlen(sourcefile);
    if (sourcefile[len-1] == '/')
	sourcefile[len-1] = '\0';

    first_text = xstrdup(sourcefile);
    do {
	add_history(sourcefile);
	rl_startup_hook = insert_first_text;
	newname = readline("> ");

	if (newname != NULL
		&& strcmp(newname, "") != 0
		&& strcmp(newname, sourcefile) != 0) {
	    char **args;

	    if (file_exists(newname)) {
		printf(_("%s: overwrite `%s'? "), program_name, quotearg(newname));
		fflush(stdout);
		if (!yesno()) {
		    free(first_text);
		    first_text = newname;
		    continue;
		}
	    }

	    args = xmalloc(sizeof(char *) * (argc + (gotend ? 2 : 3)));
	    memcpy(args, argv, argc * sizeof(char *));

	    if (gotend) {
		args[argc] = newname;
		args[argc+1] = NULL;
	    } else {
		args[argc] = "--";
		args[argc+1] = newname;
		args[argc+2] = NULL;
	    }

	    args[0] = "cp";
	    if (execvp(args[0], args) < 0)
	    	die_errno(_("cannot execute %s"), args[0]);
	    die(_("cannot execute %s"), args[0]);
	}
    } while (false);

    if (newname == NULL)
	puts("");

    warn(_("no changes made"));
    exit(0);
}
