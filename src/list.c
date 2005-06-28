/* listing.c - Running and parsing the output of ls(1). 
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
#include <sys/types.h>	    /* POSIX */
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>	    /* POSIX */
#endif
#if HAVE_UNISTD_H
#include <unistd.h> 	    /* POSIX */
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>  	    /* POSIX */
#endif
#include <sys/stat.h>	    /* POSIX */
#include <getopt.h> 	    /* gnulib (POSIX) */
#include <stdbool.h>	    /* Gnulib (gettext) */
#include <gettext.h>
#define _(s) gettext(s)
#define N_(s) (s)
#include <quotearg.h>	    /* Gnulib */
#include <errno.h>  	    /* C89 */
#include <stdlib.h> 	    /* C89 */
#include <stdio.h>  	    /* C89 */
#include <string.h> 	    /* C89 */
#include "common/common.h"
#include "common/io-utils.h"
#include "xalloc.h"		/* Gnulib */
#include "xvasprintf.h"		/* Gnulib */
#include "common/string-utils.h"
#include "common/strbuf.h"
#include "common/error.h"
#include "common/llist.h"
#include "common/hmap.h"
#include "qcmd.h"

// FIXME: breaks tmp (see below)
#define ls_assert(e)	\
    if (!(e)) { \
	internal_error("unexpected output from `ls' (%s:%d)\nLine:\n%s\n", \
			__FILE__, __LINE__, line); \
    }

static bool run_ls(char **args, pid_t *ls_pid, int *ls_fd);
static bool clean_ls(pid_t ls_pid, int ls_fd, int *status);
static bool process_ls_output(char *firstdir, LList *files, int ls_fd);

static LList *ls_options;
static int old_dir = -1;

enum {
    SORT_OPT = 2000,
    TIME_OPT,
};

static struct option ls_option_table[] = {
    { "all",            no_argument,       NULL, 'a' },
    { "almost-all",     no_argument,       NULL, 'A' },
    { "ignore-backups", no_argument,       NULL, 'B' },
    { "directory",      no_argument,       NULL, 'd' },
    { "reverse",        no_argument,       NULL, 'r' },
    { "recursive",      no_argument,       NULL, 'R' },
    { "sort",           required_argument, NULL, SORT_OPT },
    { "time",           required_argument, NULL, TIME_OPT },
    { 0 },
};

struct option *
append_ls_options(const struct option *options)
{
    struct option *new_options;
    int c;
    int d;

    /* this should be static, but since */
    /* append_ls_options is called early this is ok */
    ls_options = llist_new();

    for (c = 0; options[c].name != NULL; c++);
    d = sizeof(ls_option_table)/sizeof(*ls_option_table);

    new_options = xmalloc((c+d) * sizeof(struct option));
    memcpy(new_options, options, c * sizeof(struct option));
    memcpy(new_options+c, ls_option_table, d * sizeof(struct option));

    return new_options;
}

void
display_ls_help(FILE *out)
{
    fprintf(out, _("\
Listing (`ls') options:\n\
  -a, --all                  do not hide entries starting with .\n\
  -A, --almost-all           do not list implied . and ..\n\
  -B, --ignore-backups       do not list implied entries ending with ~\n\
  -c                         sort by ctime\n\
  -d, --directory            list directory entries instead of contents\n\
  -r, --reverse              reverse order while sorting\n\
  -R, --recursive            list subdirectories recursively\n\
  -S                         sort by file size\n\
      --sort=WORD            extension -X, none -U, size -S, time -t,\n\
                               version -v, status -c, time -t, atime -u,\n\
                               access -u, use -u\n\
      --time=WORD            show time as WORD instead of modification time:\n\
                               atime, access, use, ctime or status; use\n\
                               specified time as sort key if --sort=time\n\
  -t                         sort by modification time\n\
  -u                         sort by access time\n\
  -U                         do not sort; list entries in directory order\n\
  -X                         sort alphabetically by entry extension\n"));
}

void
list_command(char **args)
{
    int argc;
    int c;

    for (argc = 1; args[argc] != NULL; argc++);

    llist_clear(ls_options);

    optind = 0;
    while (true) {
	c = getopt_long(argc, args, "aABcdrRStuUX", ls_option_table, NULL);
	if (c == -1)
	    break;
	if (c == '?')
	    exit(1);	//FIXME don't exit here
	process_ls_option(c);
    }

    if (list_files(args+optind)) {
	if (llist_is_empty(work_files)) {
	    printf(_("no files listed\n"));
	} else if (llist_size(work_files) == 1) {
	    printf(_("1 file listed\n"));
	} else {
	    printf(_("%d files listed\n"), llist_size(work_files));
	}
    }
}

void
process_ls_option(int c)
{
    switch (c) {
    case 'a': /* all */
	llist_add(ls_options, "--all");
	break;
    case 'A': /* almost-all */
	llist_add(ls_options, "--almost-all");
	break;
    case 'B': /* ignore-backups */
	llist_add(ls_options, "--ignore-backups");
	break;
    case 'c':
	llist_add(ls_options, "--time=ctime");
	break;
    case 'd': /* directory */
	llist_add(ls_options, "--directory");
	break;
    case 'r': /* reverse */
	llist_add(ls_options, "--reverse");
	break;
    case 'R': /* recursive */
	llist_add(ls_options, "--recursive");
	break;
    case 'S':
	llist_add(ls_options, "--sort=size");
	break;
    case SORT_OPT: /* sort */
	llist_add(ls_options, "--sort");
	llist_add(ls_options, optarg);
	break;
    case 't':
	llist_add(ls_options, "--sort=time");
	break;
    case TIME_OPT: /* time */
	llist_add(ls_options, "--time");
	llist_add(ls_options, optarg);
	break;
    case 'u':
	llist_add(ls_options, "--time=atime");
	break;
    case 'U':
	llist_add(ls_options, "--sort=none");
	break;
    case 'X':
	llist_add(ls_options, "--sort=extension");
	break;
    }
}

bool
cwd_from_work_directory()
{
    if (old_dir != -1) {
	if (fchdir(old_dir) < 0) {
	    close(old_dir);
	    warn(_("cannot change back to old directory: %s"), errstr);
	    return false;
	}
	close(old_dir);
	old_dir = -1;
    }
    return true;
}

bool
cwd_to_work_directory()
{
    if (strcmp(work_directory, ".") == 0) {
	old_dir = -1;
	return true;
    }
    if ((old_dir = open(".", O_RDONLY)) < 0) {
	warn(_("cannot get current directory: %s"), errstr);
	return false;
    }
    if (chdir(work_directory) < 0) {
	close(old_dir);
	warn(_("%s: cannot change to directory: %s"), quotearg(work_directory), errstr);
	return false;
    }
    return true;
}

/**
 * Generate a list of specified files.
 * Return true if listing succeeded.
 *
 * XXX: clean up management of work_directory, old and old_dir
 */
bool
list_files(char **args)
{
    char *firstdir;
    LList *ls_args_list;
    char **ls_args;
    pid_t ls_pid;
    int ls_fd;
    int status;

    free(work_directory);
    work_directory = xstrdup(".");

    ls_args_list = llist_clone(ls_options);	/* llist_add_all! */
    llist_add_last(ls_args_list, "--");
    llist_add_first(ls_args_list, "--quoting-style=c");
    llist_add_first(ls_args_list, "ls");

    if (llist_contains(ls_options, "--directory")) {
	firstdir = ".";
    } else {
	firstdir = (*args == NULL ? NULL : *args);
    }

    for (; *args != NULL; args++) {
	if (*args == firstdir
		&& args[1] == NULL
		&& is_directory(*args)) {
	    char *old = work_directory;
	    work_directory = xstrdup(*args);
	    if (!cwd_to_work_directory()) {
		free(work_directory);
		work_directory = old;
		return false;
	    }
	    free(old);
	    llist_add(ls_args_list, ".");
	    firstdir = ".";
	} else {
	    llist_add(ls_args_list, *args);
	}
    }

    ls_args = (char **) llist_to_null_terminated_array(ls_args_list);
    llist_free(ls_args_list);
    if (!run_ls(ls_args, &ls_pid, &ls_fd)) {
	if (old_dir != -1) {
	    old_dir = -1;
	    close(old_dir);
	}
	warn(_("cannot execute `ls': %s"), errstr);
	free(ls_args);
	clean_ls(ls_pid, ls_fd, &status);
	return false;
    }
    free(ls_args);

    if (!cwd_from_work_directory()) {
	clean_ls(ls_pid, ls_fd, &status);
	return false;
    }

    llist_iterate(work_files, free_file_spec);
    llist_clear(work_files);

    if (!process_ls_output(firstdir, work_files, ls_fd)) {
	warn(_("cannot read `ls' output: %s"), errstr);
	clean_ls(ls_pid, ls_fd, &status);
	return false;
    }
    if (!clean_ls(ls_pid, ls_fd, &status))
	return false;
    if (!WIFEXITED(status)) {
	warn(_("ls: abnormal exit"));
	return false;
    }

    return true;
}

/**
 * Execute ls(1) in a child process. A pipe is created
 * so that the output of ls can be read from the parent
 * process.
 *
 * @param args
 *   The argument vector to use when executing ls.
 */
static bool
run_ls(char **args, pid_t *ls_pid, int *ls_fd)
{
    int child_pipe[2];
    pid_t child_pid;

    *ls_fd = -1;
    *ls_pid = -1;

    if (pipe(child_pipe) == -1)
	return false;
    *ls_fd = child_pipe[0];

    child_pid = fork();
    if (child_pid == -1) {
	close(child_pipe[1]);
	return false;
    }
    if (child_pid == 0) {
	if (close(child_pipe[0]) == -1)
	    die(_("cannot close file: %s"), errstr);
	if (dup2(child_pipe[1], STDOUT_FILENO) == -1)
	    die(_("cannot duplicate file descriptor: %s"), errstr);
	execvp("ls", args);
	die(_("cannot execute `ls': %s"), errstr);
    }
    *ls_pid = child_pid;

    if (close(child_pipe[1]) == -1)
	return false;

    return true;
}

/**
 * Wait for the ls child process to exit.
 * @returns
 *   true if everything went successfully.
 */
static bool
clean_ls(pid_t ls_pid, int ls_fd, int *status)
{
    if (ls_fd != -1 && close(ls_fd) == -1)
	return false;
    if (ls_pid != -1 && waitpid(ls_pid, status, 0) != ls_pid)
	return false;
    /* We ignore the return code of the ls child process. The error
     * will be discovered when reading the output of ls anyway.
     */
    return true;
}

/* Read and parse output from ls(1), converting it
 * into a list of file paths.
 *
 * @param firstdir
 *   The first non-option argument to ls. If this is a directory,
 *   it is used as the default path for files listed.
 * @returns
 *   true if processing succeeded (otherwise errno will be set).
 */
static bool
process_ls_output(char *firstdir, LList *files, int ls_fd)
{
    HMap *map_files;
    FILE *lsout;
    StrBuf *dir;
    char *line;

    map_files = hmap_new();

    lsout = fdopen(ls_fd, "r");
    if (lsout == NULL)
	return false;

    dir = strbuf_new();
    if (firstdir != NULL
	    && strcmp(firstdir, ".") != 0
	    && is_directory(firstdir)) {
	strbuf_set(dir, firstdir);
	if (firstdir[strlen(firstdir)-1] != '/')
	    strbuf_append_char(dir, '/');
    }

    while ((line = read_line(lsout)) != NULL) {
	int len;
	char *tmp;

	chomp(line);
	len = strlen(line);
	if (len != 0) {
	    ls_assert(line[0] == '"');

    	    /* If line ends with :, then it denotes a directory */
	    if (line[len-1] == ':') {
		ls_assert(line[len-2] == '"');
		line[len-2] = '\0';

		tmp = dequote_output_file(&line[1]);
		ls_assert(tmp != NULL);

		strbuf_set(dir, tmp);
		if (strbuf_char_at(dir, strbuf_length(dir)-1) != '/')
		    strbuf_append_char(dir, '/');
	    } else {
		ls_assert(line[len-1] == '"');
		line[len-1] = '\0';

		tmp = dequote_output_file(&line[1]);
		ls_assert(tmp != NULL);

    	    	if (strcmp(tmp, ".") != 0 && strcmp(tmp, "..") != 0) {
		    FileSpec *spec;

		    spec = new_file_spec();
		    spec->old_name = xasprintf("%s%s", strbuf_buffer(dir), tmp);
		    spec->new_name = xstrdup(spec->old_name);

		    if (hmap_contains_key(map_files, spec->old_name)) {
			warn(_("%s: file already listed"), quotearg(spec->old_name));
		    } else {
			hmap_put(map_files, spec->old_name, spec);
			/*printf("add %p\n", spec);*/
			llist_add(files, spec);
		    }
		}
	    }

	    free(tmp);
	}
	free(line);
    }
    strbuf_free(dir);
    hmap_free(map_files);

    return !ferror(lsout);
}

/* Create and initialize a FileSpec instance.
 * The returned FileSpec should be freed when
 * no longer used.
 */
FileSpec *
new_file_spec(void)
{
    FileSpec *spec;

    spec = xmalloc(sizeof(FileSpec));
    spec->old_name = NULL;
    spec->new_name = NULL;
    spec->status = STATUS_UNCHECKED;
    spec->prev_spec = NULL;
    spec->next_spec = NULL;

    return spec;
}

/* Free the specified FileSpec.
 */
void
free_file_spec(FileSpec *spec)
{
    if (spec->prev_spec != NULL)
    	spec->prev_spec->next_spec = spec->next_spec;
    if (spec->next_spec != NULL)
    	spec->next_spec->prev_spec = spec->prev_spec;
    free(spec->old_name);
    free(spec->new_name);
    free(spec);
}