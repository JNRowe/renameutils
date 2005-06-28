/* qcmd.c - Main routines for qcmd.
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
#include <locale.h> 	    	    /* C89 */
#include <getopt.h> 	    	    
#include <stdlib.h> 	    	    /* C89 */
#include <errno.h>  	    	    /* C89 */
#include <stdbool.h>	    	    /* Gnulib (POSIX) */
#include "progname.h"	    	    /* Gnulib */
#include "quotearg.h"	    	    /* Gnulib */
#include "version-etc.h"    	    /* Gnulib */
#include "dirname.h"	    	    /* Gnulib */
#include <gettext.h>	    	    /* Gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) (s)
#include "common/io-utils.h"
#include "xalloc.h"
#include <string.h>
#include "common/string-utils.h"
#include "common/error.h"
#include "common/llist.h"
#include "qcmd.h"

char *program;
bool simulate = false;
bool verbose = false;
LList *work_files;
char *work_directory;
char *editor_program = NULL;
char *edit_filename = NULL;
EditFormat *format = &dual_column_format;
ApplyPlan *plan = NULL;
const char version_etc_copyright[] = N_("Copyright (C) 2001-2004 Oskar Liljeblad");

enum {
    SIMULATE_OPT = 1000,
    VERSION_OPT,
    HELP_OPT,
};

static struct option option_table[] = {
    { "format",       required_argument, NULL, 'f'          },
    { "options",      required_argument, NULL, 'o'          },
    { "editor",	      required_argument, NULL, 'e'          },
    { "verbose",      no_argument,       NULL, 'v'          },
    { "dummy",        no_argument,       NULL, SIMULATE_OPT },
    { "simulate",     no_argument,       NULL, SIMULATE_OPT },
    { "interactive",  no_argument,       NULL, 'i'          },
    { "version",      no_argument,       NULL, VERSION_OPT  },
    { "help",         no_argument,       NULL, HELP_OPT     },
    { 0 },
};

void
display_version(void)
{
    /*version_etc_copyright = N_("Copyright (C) 2001-2004 Oskar Liljeblad");*/
    version_etc(stdout, program, PACKAGE, VERSION, "Oskar Liljeblad", NULL);
}

void
display_help(FILE *out)
{
    fprintf(out, _("Usage: %s [OPTION]... [FILE]...\n\
Move (qmv) or copy (qmv) files quickly, editing the destination file names\n\
in a text editor.\n\
\n"), program_name);
    display_ls_help(out);
    fprintf(out, _("\n\
Format-related options:\n\
  -f, --format=FORMAT        change edit format of text file\n\
  -o, --options=OPTIONS      pass options to the edit format\n\
\n\
Other options:\n\
  -i, --interactive          start in command mode\n\
  -e, --editor=EDITOR        program to edit text file with\n\
  -v, --verbose              be more verbose\n\
      --dummy                don't copy (\"dummy\" mode)\n\
\n\
General options:\n\
      --help                 display this help and exit\n\
      --version              output version information and exit\n\
\n\
If no FILE arguments are specified, %s opens a text editor session\n\
showing all files in the current directory. With the default edit format,\n\
the files appear in a single column extending downward. An identical\n\
column appears on its right. Edit the right hand column - make desired\n\
name changes. Save the file and exit. %s will now do a rename/copy\n\
operation for any line changed.\n\
\n\
Unless the --editor option is used, the editor is chosen from the VISUAL\n\
environment variable if set, otherwise EDITOR. If none of these variables\n\
are set, the program `editor' is used.\n\
\n\
Possible values for --format are: `single-column', `dual-column',\n\
and `destination-only'. For a list of available options for each\n\
format, use --options=help.\n\
\n\
Report bugs to <%s>.\n"),
	program,
	program,
	PACKAGE_BUGREPORT);
}

static bool
noncommandmode_run(char **args)
{
    if (!list_files(args))
    	return true;
    if (llist_is_empty(work_files)) {
	warn(_("no files matched"));
	return true;
    }
    if (!edit_files(true, false))
    	return false;
    if (plan != NULL)
	free_plan(plan);
    plan = make_plan(work_files);
    if (plan == NULL)
    	return false;
    display_plan(plan);
    if (!llist_is_empty(plan->error))
    	return false;
    if (!llist_is_empty(plan->ok) && !apply_plan(plan))
    	return false;

    return true;
}

int
main(int argc, char **argv)
{
    struct option *all_options;
    char *format_options = NULL;
    bool interactive = false;

    set_program_name(argv[0]);
    program = base_name(program_name);

    if (setlocale (LC_ALL, "") == NULL)
    	die(_("invalid locale"));
    if (bindtextdomain (PACKAGE, LOCALEDIR) == NULL)
    	die_errno(NULL);
    if (textdomain (PACKAGE) == NULL)
    	die_errno(NULL);

    work_directory = xstrdup(".");
    work_files = llist_new();

    /* Initialize option table, and parse options */
    all_options = append_ls_options(option_table);
    for (;;) {
    	int c;

	c = getopt_long(argc, argv, "aABcdrRStuUXvf:o:e:i", option_table, NULL);
	if (c == -1)
	    break;

	switch (c) {
	case '?':
	    exit(1);
	case 'i': /* --interactive */
	    interactive = true;
	    break;
	case 'v': /* --verbose */
	    verbose = true;
	    break;
	case 'f': /* --format */
	    format = find_edit_format_by_name(optarg);
	    if (format == NULL)
		die(_("no such edit format `%s'"), quotearg(optarg));
	    break;
	case 'o': /* options */
	    format_options = optarg;
	    break;
	case 'e': /* --editor */
	    editor_program = optarg;
	    break;
	case SIMULATE_OPT:
	    simulate = true;
	    break;
	case VERSION_OPT:
	    display_version();
	    exit(0);
	case HELP_OPT:
	    display_help(stdout);
	    exit(0);
	default:
	    process_ls_option(c);
	    break;
	}
    }

    /* Identify program mode - qmv or qcp */
    if (strcmp(program, "qmv") != 0 && strcmp(program, "qcp") != 0)
    	die(_("unable to identify mode (start as qmv or qcp)"));

    /* Create a temporary file for editing */
    edit_filename = create_temporary_file(program);
    if (edit_filename == NULL)
	die(_("cannot create temporary file: %s"), errstr);

    /* Find a suitable editor program */
    if (editor_program == NULL
    	    && (editor_program = getenv("VISUAL")) == NULL
    	    && (editor_program = getenv("EDITOR")) == NULL)
    	editor_program = "editor";
    editor_program = xstrdup(editor_program);

    /* Parse format options */
    if (format_options != NULL && !format->parse_options(format_options))
	exit(0);

    edit_file_list = llist_new();
    if (interactive) {
	display_commandmode_header();
	if (argv[optind] != NULL)
	    list_files(argv+optind);
	commandmode_mainloop();
    } else {
    	if (!noncommandmode_run(argv+optind)) {
	    printf(_("Entering interactive mode.\n"));
	    commandmode_mainloop();
	}
    }
    llist_free(edit_file_list);

    if (plan != NULL)
	free_plan(plan);
    llist_iterate(work_files, free_file_spec);
    llist_free(work_files);
    free(all_options);
    free(editor_program);
    if (unlink(edit_filename) < 0)
    	warn("cannot delete %s: %s\n", quotearg(edit_filename), errstr);
    free(edit_filename);
    free(work_directory);

    return 0;
}
