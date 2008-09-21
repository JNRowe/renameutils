/* commandmode.c - Interpreter and completion for the interactive mode.
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
#include <sys/types.h>	    	    	/* POSIX */
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>	    	    	/* POSIX */
#endif
#include <unistd.h> 	    	    	/* gnulib (POSIX) */
#if HAVE_WORDEXP_H
#include <wordexp.h>	    	    	/* POSIX */
#endif
#include <signal.h> 	    	    	/* gnulib (POSIX) */
#include <errno.h>  	    	    	/* C89 */
#include <stdio.h>  	    	    	/* C89 */
#include <stdlib.h> 	    	    	/* C89 */
#include <readline/readline.h>	    	/* GNU Readline */
#include <readline/history.h>	    	/* GNU Readline */
#include <stdbool.h>			/* gnulib (POSIX) */
#include <gettext.h> 	    	    	/* gnulib (gettext) */
#define _(s) gettext(s)
#define N_(s) (s)
#include "progname.h"			/* gnulib */
#include "quotearg.h"	    	    	/* gnulib */
#include "xalloc.h"			/* gnulib */
#include "xvasprintf.h"			/* gnulib */
#include "version-etc.h"		/* gnulib */
#include "common/common.h"
#include "common/comparison.h"
#include "common/string-utils.h"
#include "common/error.h"
#include "qcmd.h"

struct Command {
    char *word;
    void (*handler)(char **);
};

static int command_compare(const struct Command *c1, const struct Command *c2);
static char *command_generator(const char *text, int state);
static char **completion(const char *text, int start, int end);
static void edit_command(char **args);
static void help_command(char **args);
static void quit_command(char **args);
static void version_command(char **args);
static void apply_command(char **args);
static void plan_command(char **args);

/* This array must be sorted by the `word' field */
static struct Command commands[] = {
    { "apply",	    apply_command },
    { "ed",	    edit_command },
    { "edit",	    edit_command },
    { "exit",	    quit_command },
    { "help",	    help_command },
    { "import",	    import_command },
    { "list",	    list_command },
    { "ls",	    list_command },
    { "plan",	    plan_command },
    { "quit",	    quit_command },
    { "set",	    set_command },
    { "show",	    show_command },
    { "version",    version_command },
};
static bool running = true;
static bool exit_warned = false;

static int
command_compare(const struct Command *c1, const struct Command *c2)
{
    return strcmp(c1->word, c2->word);
}

static char *
command_generator(const char *text, int state)
{
    static int c;

    if (state == 0)
	c = 0;

    while (c < sizeof(commands)/sizeof(*commands)) {
	char *name = commands[c].word;
	c++;
	if (starts_with(name, text))
	    return xstrdup(name);
    }

    return NULL;
}

static char **
completion(const char *text, int start, int end)
{
    int idx = word_get_index(rl_line_buffer, start);

    if (idx == 0) {
	return rl_completion_matches(text, command_generator);
    } else if (idx == 1) {
	char *command = word_get(rl_line_buffer, 0);
	if (strcmp(command, "set") == 0 || strcmp(command, "show") == 0) {
	    free(command);
	    return rl_completion_matches(text, variable_generator);
	}
	free(command);
    } else if (idx == 2) {
	char *command = word_get(rl_line_buffer, 0);
	if (strcmp(command, "set") == 0) {
	    char *variable = word_get(rl_line_buffer, 1);
	    if (strcmp(variable, "format") == 0) {
		free(command);
		free(variable);
		return rl_completion_matches(text, edit_format_generator);
	    }
	    else if (strcmp(variable, "options") == 0) {
		free(command);
		free(variable);
		return rl_completion_matches(text, format->option_generator);
	    }
	    free(variable);
	}
	free(command);
    }

    return NULL;
}

void
display_commandmode_header(void)
{
    printf(_("%s (%s) %s\n\
%s.\n\
This program is free software, covered by the GNU General Public License,\n\
and you are welcome to change it and/or distribute copies of it under\n\
certain conditions. There is absolutely no warranty for this program.\n\
See the COPYING file for details.\n"),
	program, PACKAGE, VERSION, version_etc_copyright);
}

void
terminate_program(void)
{
    bool valid = false;

    if (plan != NULL && llist_is_empty(plan->error)) {
//	Iterator *it;

	valid = true;
	if (!llist_is_empty(plan->ok)) {
	    valid = false;
/*	    for (it = llist_iterator(plan->ok); iterator_has_next(it); ) {
		FileSpec *spec = iterator_next(it);
		if (spec->status != APPLY_COMPLETE) {
		    valid = false;
		    break;
		}
FIXME
	    }
	    iterator_free(it);*/
	}
    }
    if (valid || plan == NULL || exit_warned) {
	running = false;
	puts("");
	return;
    }
    puts("exit");
    printf(_("There are unapplied changes.\n"));
    exit_warned = true;
}

static void
int_signal_handler(int signal)
{
    puts("");
    rl_line_buffer[0] = '\0';
    rl_point = 0;
    rl_end = 0;
    rl_forced_update_display();
}

void
commandmode_mainloop(void)
{
    struct sigaction action;
    char *prompt;

    rl_readline_name = program_name;
    rl_attempted_completion_function = completion;
    rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(,";

    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = int_signal_handler;
    action.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &action, NULL) < 0)
	die(_("cannot register signal handler: %s"), errstr);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGQUIT, &action, NULL) < 0)
	die(_("cannot register signal handler: %s"), errstr);

    prompt = xasprintf("%s> ", program);

    while (running) {
	char *line;
	char **words;
	wordexp_t result;
	int rc;

	line = readline(prompt);
	if (line == NULL) {
	    terminate_program();
	    continue;
	}
	add_history(line);
	rc = wordexp(line, &result, WRDE_NOCMD|WRDE_UNDEF);
	if (rc != 0) {
	    free(line);
	    switch (rc) {
	    case WRDE_BADCHAR:
		warn(_("input contains unquoted invalid character"));
		break;
	    case WRDE_BADVAL:
		warn(_("variable reference using dollar sign ($) is not allowed"));
		break;
	    case WRDE_CMDSUB:
		warn(_("command substitution using backticks (``) is not allowed"));
		break;
	    case WRDE_NOSPACE:
		errno = ENOMEM;
		die("%s", errstr);
		break;
	    case WRDE_SYNTAX:
		warn(_("syntax error in input"));
		break;
	    }
	    continue;
	}
	words = result.we_wordv;
	if (words != NULL && *words != NULL) {
	    struct Command *result;
	    struct Command target;

	    target.word = words[0];
	    result = bsearch(&target, commands,
		    sizeof(commands)/sizeof(*commands),
		    sizeof(*commands), (comparison_fn_t) command_compare);
	    if (result == NULL)
		warn(_("undefined command `%s'. Try `help'."), quotearg(words[0]));
	    else
		result->handler(words);
	}
	if (words != NULL)
	    wordfree(&result);
	free(line);
    }

    free(prompt);
}

static void
apply_command(char **args)
{
    if (plan == NULL) {
    	printf(_("No plan - use `list' and `edit' first.\n"));
	return;
    }
    apply_plan(plan);
}

static void
plan_command(char **args)
{
    if (plan == NULL) {
	printf(_("No plan - use `list' and `edit' first.\n"));
	return;
    }
    display_plan(plan);
}

static void
edit_command(char **args)
{
    bool all;
    bool force;

    if (llist_is_empty(work_files)) {
	printf(_("No files to edit - use `list' first.\n"));
	return;
    }
    all = (args[1] != NULL && strcmp(args[1], "all") == 0);
    force = false;
    if (edit_files(all, force)) {
	if (plan != NULL)
	    free_plan(plan);
	plan = make_plan(work_files);
	if (plan != NULL)
    	    display_plan(plan);
    }
}

static void
quit_command(char **args)
{
    running = false;
}

static FILE *
launch_pager(pid_t *child_pid)
{
    FILE *pager;
    pid_t pid;
    int pager_fd[2];

    *child_pid = -1;
    if (pipe(pager_fd) != 0) {
	warn(_("cannot start pager: %s"), errstr);
	return stdout;
    }

    pid = fork();
    if (pid < 0) {
	warn(_("cannot start pager: %s"), errstr);
	return stdout;
    }
    if (pid == 0) {
	if (close(pager_fd[1]) < 0)
	    die(_("cannot start pager: %s"), errstr);
	if (dup2(pager_fd[0], STDIN_FILENO) < 0)
	    die(_("cannot start pager: %s"), errstr);
	execlp("pager", "pager", NULL);
	if (errno == ENOENT)
	    execlp("more", "more", NULL);
	if (errno == ENOENT)
	    execlp("cat", "cat", NULL);
	die(_("cannot start pager: %s"), errstr);
    }
    *child_pid = pid;

    if (close(pager_fd[0]) < 0) {
	warn(_("cannot start pager: %s"), errstr);
	return stdout;
    }

    pager = fdopen(pager_fd[1], "w");
    if (pager == NULL) {
	warn(_("cannot start pager: %s"), errstr);
	return stdout;
    }
    return pager;
}

static bool
close_pager(FILE *pager, pid_t child_pid)
{
    if (pager != stdout && fclose(pager) < 0) {
	warn(_("cannot terminate pager: %s"), errstr);
	return false;
    }
    if (child_pid != -1 && waitpid(child_pid, NULL, 0) != child_pid) {
	warn(_("cannot terminate pager: %s"), errstr);
	return false;
    }
    return true;
}

static void
help_command(char **args)
{
    FILE *pager;
    pid_t child;

    if (args[1] == NULL) {
	pager = launch_pager(&child);
	fprintf(pager, _("\
ls, list [OPTIONS].. [FILES]..\n\
  Select files to rename. If no files are specified, select all files in\n\
  current directory. Use `help ls' to display a list of allowed options.\n\
import FILE\n\
  Read files to rename from a text file. Each line should correspond to an\n\
  existing file to rename.\n\
ed, edit [all]\n\
  Edit renames in a text editor. If this command has been run before, and\n\
  not `all' is specified, only edit renames with errors in.\n\
plan\n\
  Display the current rename-plan. (This plan is created after `edit'.)\n\
apply\n\
  Apply the current plan, i.e. rename files. Only those files marked as OK\n\
  in the plan will be renamed.\n\
show [VARIABLE]\n\
  Display the value of the specified variable, or all variables if none\n\
  specified.\n\
set VARIABLE VALUE\n\
  Set the value of a variable.\n\
exit, quit\n\
  Exit this program\n\
help [ls|usage]\n\
  If `ls' is specified, display list options. If `usage' is specified,\n\
  display accepted command line options. Otherwise display this help\n\
  message.\n\
version\n\
  Display version information for this program.\n"));
	close_pager(pager, child);
    } else if (strcmp(args[1], "ls") == 0) {
	pager = launch_pager(&child);
	display_ls_help(pager);
	close_pager(pager, child);
    } else if (strcmp(args[1], "usage") == 0) {
	pager = launch_pager(&child);
	display_help(pager);
	close_pager(pager, child);
    } else {
	printf(_("Usage: help [ls|usage]\n"));
    }
}

static void
version_command(char **args)
{
    display_version();
    printf(_("\nSend bug reports and questions to <%s>.\n"), PACKAGE_BUGREPORT);
}
