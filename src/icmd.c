/* icmd.c - Interactive mv(1) and cp(1).
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
#include <unistd.h> 	    	/* gnulib (POSIX) */
#include <sys/stat.h>		/* POSIX */
#include <signal.h> 	    	/* gnulib (POSIX) */
#include <stdlib.h> 	    	/* C89 */
#include <stdio.h>  	    	/* C89 */
#include <errno.h>  	    	/* C89 */
#include <locale.h> 	    	/* gnulib (POSIX) */
#include <stdbool.h>	    	/* gnulib (POSIX) */
#include <readline/readline.h>	/* Readline */
#include <readline/history.h>	/* Readline */
#include <getopt.h>		/* gnulib (POSIX) */
#include <gettext.h>	    	/* gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) (s)
#include "configmake.h"		/* gnulib */
#include "progname.h"	    	/* gnulib */
#include "quotearg.h"	    	/* gnulib */
#include "quote.h"		/* gnulib */
#include "yesno.h"  	    	/* gnulib */
#include "version-etc.h"    	/* gnulib */
#include "xalloc.h"		/* gnulib */
#include "dirname.h"		/* gnulib */
#include "common/error.h"
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "common/common.h"

#define MV_COMMAND "mv"
#define CP_COMMAND "cp"
/* This list should be up to date with mv and cp!
 * It was last updated on 2007-11-30 for
 * Debian coreutils 5.97-5.4 in unstable.
 *
 * Do not include -t, --target-directory in this list!
 *
 * If you update these lists, don't forget to update the manual page as well!
 */
#define MV_REQ_ARG_OPTIONS "S,reply,suffix"
#define CP_REQ_ARG_OPTIONS "S,Z,no-preserve,sparse,suffix,context"

const char version_etc_copyright[] = "Copyright (C) 2001, 2002, 2004, 2005, 2007, 2008 Oskar Liljeblad";

static char *first_text = NULL;

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
display_help(void)
{
    printf(_("Usage: %s [OPTION] FILE...\n"), program_name);
    printf(_("Rename (imv) or copy a file (icp) by editing the destination name using\n"
             "GNU Readline. All options except those listed below are passed on to mv,\n"
             "cp, or the command specified by --command.\n\n"));
    printf(_("      --command=FILE         command to run instead of default mv/cp\n"));
    printf(_("      --arg-options=OPTIONS  list of options that require an argument\n"));
    printf(_("      --pass-through         run the command if two or more arguments are specified\n"));
    printf(_("      --help                 display this help and exit\n"));
    printf(_("      --version              output version information and exit\n"));
    printf(_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
}

/* Improve: return string position or word index in csv where value was found
 * XXX: move to strutil.c or something. Also in gmediaserver and microdc.
 */
bool
string_in_csv(const char *csv, char sep, const char *value)
{
    const char *p0;
    const char *p1;
    size_t len;
    
    len = strlen(value);
    for (p0 = csv; (p1 = strchr(p0, sep)) != NULL; p0 = p1+1) {
        if (p1-p0 == len && strncmp(p0, value, len) == 0)
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
    bool pass_through = false;
    char *last_long_opt = NULL;
    char last_short_opt = '\0';
    char *arg_options = "";
    char *command = NULL;
    char *files[argc];
    /*char *opts[argc];*/
    int file_count = 0;
    /*int opt_count = 0;*/
    struct stat sb;
    char *srcfile;
    char *srcfile_stripped;
    char *program_base_name;
    int c;
    int first_cmd_opt;

    set_program_name(argv[0]);
    program_base_name = base_name(program_name);

    if (setlocale(LC_ALL, "") == NULL)
        warn(_("cannot set locale: %s\n"), errstr);
#ifdef ENABLE_NLS
    if (bindtextdomain(PACKAGE, LOCALEDIR) == NULL)
        warn(_("cannot bind message domain: %s\n"), errstr);
    if (textdomain(PACKAGE) == NULL)
        warn(_("cannot set message domain: %s\n"), errstr);
#endif

    if (strcmp(program_base_name, "imv") == 0) {
        arg_options = MV_REQ_ARG_OPTIONS;
        command = MV_COMMAND;
    } else if (strcmp(program_base_name, "icp") == 0)  {
        arg_options = CP_REQ_ARG_OPTIONS;
        command = CP_COMMAND;
    }

    for (c = 1; c < argc; c++) {
        char *arg = argv[c];

        if (strcmp(arg, "--help") == 0) {
            display_help();
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
        } else if (strcmp(arg, "--pass-through") == 0) {
            pass_through = true;
        } else if (strncmp(arg, "--command=", 10) == 0) {
            command = arg+10;
        } else if (strncmp(arg, "--arg-options=", 14) == 0) {
            arg_options = arg+14;
        } else {
            break;
        }
    }

    if (command == NULL && strcmp(program_base_name, "imv") != 0 && strcmp(program_base_name, "icp") != 0)
        die(_("must be started as either `imv' or `icp', or --command must be specified"));

    first_cmd_opt = c;

    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = int_signal_handler;
    action.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &action, NULL) < 0)
	die(_("cannot register signal handler - %s\n"), errstr);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGQUIT, &action, NULL) < 0)
	die(_("cannot register signal handler - %s\n"), errstr);

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
                if (string_in_csv(arg_options, ',', arg+2)) {
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
                if (string_in_csv(arg_options, ',', option)) {
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

    if (!pass_through && file_count > 1) {
        warn(_("too many arguments"));
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
    srcfile_stripped = xstrdup(srcfile);
    strip_trailing_slashes(srcfile_stripped);

    rl_readline_name = base_name(program_name);
    first_text = xstrdup(srcfile_stripped);
    for (;;) {
        char *newname;

	add_history(srcfile_stripped);
	rl_startup_hook = insert_first_text;
	newname = readline("> ");

	if (newname != NULL) {
	    trim(newname);
	    strip_trailing_slashes(newname);

	    if (newname[0] != '\0' && strcmp(newname, srcfile_stripped) != 0) {
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

                free(srcfile_stripped);
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
        free(srcfile_stripped);
        exit(EXIT_SUCCESS);
    }
}
