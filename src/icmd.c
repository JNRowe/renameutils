/* icmd.c - Interactive mv(1) and cp(1).
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
#include <unistd.h> 	    	/* POSIX */
#include <sys/stat.h>		/* POSIX */
#include <stdlib.h> 	    	/* C89 */
#include <signal.h> 	    	/* C89 */
#include <stdio.h>  	    	/* C89 */
#include <errno.h>  	    	/* C89 */
#include <locale.h> 	    	/* C89 */
#include <stdbool.h>	    	/* Gnulib, C99, POSIX */
#include <readline/readline.h>	/* Readline */
#include <readline/history.h>	/* Readline */
#include <getopt.h>		/* Gnulib, GNU libc */
#include "gettext.h"	    	/* Gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) (s)
#include "progname.h"	    	/* Gnulib */
#include "quotearg.h"	    	/* Gnulib */
#include "quote.h"		/* Gnulib */
#include "yesno.h"  	    	/* Gnulib */
#include "version-etc.h"    	/* Gnulib */
#include "xalloc.h"		/* Gnulib */
#include "dirname.h"		/* Gnulib */
#include "common/error.h"
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "common/common.h"

#define MV_COMMAND "mv"
#define CP_COMMAND "cp"
/* This list should be up to date with mv and cp!
 * It was last updated on 2005-08-12 for
 * Debian coreutils 5.2.1-2 in unstable.
 */
#define MV_REQ_ARG_OPTIONS "S,V,reply,target-directory,suffix,version-control"
#define CP_REQ_ARG_OPTIONS "S,V,no-preserve,sparse,suffix,version-control"

static char *first_text = NULL;
const char version_etc_copyright[] = "Copyright (C) 2001, 2002, 2004, 2005 Oskar Liljeblad";

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
    exit(EXIT_SUCCESS);
}

static void
display_help(const char *command)
{
    printf(_("Usage: %s [OPTION] FILE...\n"), program_name);
    if (strcmp(base_name(program_name), "imv") == 0) {
        printf(_("Rename a file by editing the destination name using GNU readline.\n"));
    } else {
        printf(_("Copy a file by editing the destination name using GNU readline.\n"));
    }
    printf(_("All options except the options listed below are passed to %s.\n\n"), command);
    printf(_("      --command=FILE         command to run instead of default %s\n"), command);
    printf(_("      --arg-options=OPTIONS  list of options that require an argument\n"));
    printf(_("      --help                 display this help and exit\n"));
    printf(_("      --version              output version information and exit\n"));
    printf(_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
}

/* Improve: return string position or word index in csv where value was found
 * XXX: move to strutil.c or something. Also in gmediaserver and microdc.
 */
bool
csv_contains(const char *csv, char sep, const char *value)
{
    const char *p0;
    const char *p1;

    for (p0 = csv; (p1 = strchr(p0, sep)) != NULL; p0 = p1+1) {
        if (strncmp(p0, value, p1-p0) == 0)
            return true;
    }
    if (strcmp(p0, value) == 0)
        return true;

    return false;
}

static void
trim(char *str)
{
    int h, t;

    for (t = strlen(str); t > 0 && isspace(str[t-1]); t--);
    str[t] = '\0';
    for (h = 0; isspace(str[h]); h++);
    if (h > 0)
        memcpy(str, str+h, t-h+1);
}



int
main(int argc, char **argv)
{
    struct sigaction action;
    bool no_more_opts = false;
    bool skip_opt_arg = false;
    bool posixly_correct;
    bool has_target_dir = false;
    char *last_long_opt = NULL;
    char last_short_opt = '\0';
    char *arg_options = "";
    char *command;
    char *files[argc];
    /*char *opts[argc];*/
    int file_count = 0;
    /*int opt_count = 0;*/
    struct stat sb;
    char *srcfile;
    int c;
    int first_cmd_opt;

    set_program_name(argv[0]);
    if (strcmp(base_name(program_name), "imv") != 0 && strcmp(base_name(program_name), "icp") != 0)
        die(_("must be started as either `imv' or `icp'"));

    if (setlocale(LC_ALL, "") == NULL)
    	die_errno(_("cannot set locale"));
    if (bindtextdomain (PACKAGE, LOCALEDIR) == NULL)
    	die_errno(_("cannot bind message domain"));
    if (textdomain (PACKAGE) == NULL)
    	die_errno(_("cannot set message domain"));

    if (strcmp(base_name(program_name), "imv") == 0) {
        arg_options = MV_REQ_ARG_OPTIONS;
        command = MV_COMMAND;
    } else {
        arg_options = CP_REQ_ARG_OPTIONS;
        command = CP_COMMAND;
    }

    for (c = 1; c < argc; c++) {
        char *arg = argv[c];

        if (strcmp(arg, "--help") == 0) {
            display_help(command);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--version") == 0) {
            version_etc(stdout, base_name(program_name), PACKAGE, VERSION, "Oskar Liljeblad", NULL);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--command") == 0) {
            if (++c >= argc)
                die(_("option `--%s' requires an argument"), arg+2);
            command = argv[c];
        } else if (strcmp(arg, "--arg-options") == 0) {
            if (++c >= argc)
                die(_("option `--%s' requires an argument"), arg+2);
            arg_options = argv[c];
        } else if (strncmp(arg, "--command=", 10) == 0) {
            command = arg+10;
        } else if (strncmp(arg, "--arg-options=", 14) == 0) {
            arg_options = arg+14;
        } else {
            break;
        }
    }
    first_cmd_opt = c;

    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = int_signal_handler;
    action.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &action, NULL) < 0)
	die_errno(NULL);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGQUIT, &action, NULL) < 0)
	die_errno(NULL);

    posixly_correct = getenv("POSIXLY_CORRECT") != NULL;

    for (c = first_cmd_opt; c < argc; c++) {
        char *arg = argv[c];
        int d;

        if (skip_opt_arg) {
            /*opts[opt_count++] = arg;*/
            skip_opt_arg = false;
        } else if (no_more_opts) {
            files[file_count++] = arg;
        } else if (arg[0] == '-' && arg[1] == '-' && arg[2] == '\0') {
            /*if (posixly_correct) {
                files[file_count++] = arg;
            } else {*/
                no_more_opts = true;
            /*}*/
        } else if (arg[0] == '-' && arg[1] == '-') {
            /*opts[opt_count++] = arg;*/
            if (strchr(arg, '=') == NULL) {
                if (csv_contains(arg_options, ',', arg+2)) {
                    skip_opt_arg = true;
                    last_long_opt = arg+2;
                    last_short_opt = '\0';
                }
            }
            if (strncmp(arg, "--target-directory", 18) == 0)
                has_target_dir = true;
        } else if (arg[0] == '-') {
            /*opts[opt_count++] = arg;*/
            for (d = 1; arg[d] != '\0'; d++) {
                char option[2] = { arg[d], '\0' };
                if (csv_contains(arg_options, ',', option)) {
                    skip_opt_arg = true;
                    last_long_opt = NULL;
                    last_short_opt = arg[d];
                    break;
                }
            }
        } else {
            files[file_count++] = arg;
            if (posixly_correct)
                no_more_opts = true;
        }
    }

    if (skip_opt_arg) {
        if (last_long_opt != NULL) 
            warn(_("option `--%s' requires an argument"), last_long_opt);
        else
            warn(_("option requires an argument -- %c"), last_short_opt);
	fprintf(stderr, _("Try `%s --help' for more information.\n"), program_name);
	exit(EXIT_FAILURE);
    }

    if (file_count == 0) {
	warn(_("missing file argument"));
	fprintf(stderr, _("Try `%s --help' for more information.\n"), program_name);
	exit(EXIT_FAILURE);
    }

    if (file_count > 1 || has_target_dir) { /* Pass everything as is to command */
        char *args[argc-first_cmd_opt+3];

        memcpy(args+1, argv+first_cmd_opt, (argc-first_cmd_opt+2) * sizeof(char *));
        args[0] = command;
        if (execvp(command, args) < 0)
            die(_("cannot execute %s: %s"), command, errstr);
        exit(EXIT_FAILURE);
    }

    /* XXX: Here we should check mv version using 'mv --version', so that
     * we know the correct set of options for mv. Pseudocode:
     */
    /*if (opt_count > 0) {
        pipe
        fork
            in child
                close read end of pipe
                dup2 stdout, write end of pipe
                execlp(command, command, "--version", NULL);
        close write end of pipe
        getline from pipe

        if (strncmp(line, "mv (coreutils) ", 15) != 0)
            cannot check version. fail.
        if (strverscmp(line+15, COREUTILS_MAX_VERSION) >= 0)
            incorrect version. fail.

         process line
         while (getline != -1);
         if (ferror)
              report error
         waitpid
         cleanup
    }*/

    srcfile = files[0];
    if (lstat(srcfile, &sb) < 0)
        die(_("cannot stat %s: %s"), quote(srcfile), errstr);

    /* We usually don't want trailing slashes in the edited file name.
     * This is a matter of taste though.
     */
    for (c = strlen(srcfile); c > 0 && srcfile[c-1] == '/'; c--)
    srcfile[c] = '\0';

    rl_readline_name = base_name(program_name);
    first_text = xstrdup(srcfile);
    for (;;) {
        char *newname;

	add_history(srcfile);
	rl_startup_hook = insert_first_text;
	newname = readline("> ");

	if (newname != NULL) {
	    trim(newname);
	    if (strcmp(newname, "") != 0 && strcmp(newname, srcfile) != 0) {
	        char *args[argc-first_cmd_opt+5];

                if (file_exists(newname)) {
                    printf(_("%s: overwrite %s? "), program_name, quote(newname));
                    fflush(stdout);
                    if (!yesno()) {
                        free(first_text);
                        first_text = newname;
                        continue;
                    }
                }

                memcpy(args+1, argv+first_cmd_opt, (argc-first_cmd_opt+1) * sizeof(char *));
                if (posixly_correct || no_more_opts) {
                    args[argc-first_cmd_opt+1] = newname;
                    args[argc-first_cmd_opt+2] = NULL;
                } else {
                    args[argc-first_cmd_opt+1] = "--";
                    args[argc-first_cmd_opt+2] = newname;
                    args[argc-first_cmd_opt+3] = NULL;
                }
                args[0] = command;
                if (execvp(command, args) < 0)
                    die(_("cannot execute %s: %s"), command, errstr);
                exit(EXIT_FAILURE);
            }
	}

        free(first_text);
        if (newname == NULL)
            puts("");

        warn(_("no changes made"));
        exit(EXIT_SUCCESS);
    }
}
