/* apply.c - apply the plan, performing the actual renames/copies
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
#include <unistd.h> 	    	    	/* gnulib (POSIX) */
#include <stdbool.h>	    	    	/* gnulib (POSIX) */
#include <sys/types.h>	    	    	/* POSIX */
#include <sys/wait.h>	    	    	/* POSIX */
#include <stdlib.h> 	    	    	/* C89 */
#include <string.h>			/* gnulib (C89) */
#include <gettext.h>	    	   	/* gnulib (gettext) */
#define _(String) gettext(String)
#include "quote.h"			/* gnulib */
#include "quotearg.h"	    	    	/* gnulib */
#include "xalloc.h"			/* Gnulib */
#include "common/error.h"
#include "common/io-utils.h"
#include "qcmd.h"

char *force_command = NULL;

static bool
wait_child_and_check_status(const char *cmd)
{
    int status;

    if (wait(&status) < 0) {
	warn(_("cannot wait for process: %s"), errstr);
	return false;
    } else if (WIFSIGNALED(status)) {
	warn(_("%s was terminated by signal %d"), cmd, WTERMSIG(status));
	return false;
    } else if (WIFSTOPPED(status)) {
	warn(_("%s was stopped by signal %d"), cmd, WSTOPSIG(status));
	return false;
    } else if (WEXITSTATUS(status) != 0) {
	warn(_("%s exited with return code %d"), cmd, WEXITSTATUS(status));
	return false;
    }

    return true;
}

static bool
perform_command(FileSpec *spec)
{
    pid_t child;
    char *command;

    /*if (rename(spec->old_name, spec->new_name) < 0) {
        warn(_("cannot rename %s to %s: %s"), spec->old_name, spec->new_name, errstr);
        printf(_("Rename failed. Command aborted.\n"));
        return false;
    }*/

    if (force_command != NULL)
        command = force_command;
    else if (strcmp(program, "qmv") == 0)
        command = "mv";
    else
        command = "cp";

    child = fork();
    if (child < 0) {
        warn(_("cannot create process: %s"), errstr);
        return false;
    }

    if (child == 0) {
        char *args[5];

        args[0] = command;
        args[1] = "--";
        args[2] = spec->old_name;
        args[3] = spec->new_name;
        args[4] = NULL;
        if (execvp(command, args) < 0)
            die(_("cannot execute %s: %s"), quote(command), errstr);
        exit(EXIT_FAILURE);
    }

    return wait_child_and_check_status(command);
}

bool
apply_plan(ApplyPlan *plan)
{
    LListIterator it;
    bool fail = false;

    if (!llist_is_empty(plan->error))
	printf(_("Plan contains errors - will only apply correct files\n"));
    if (llist_is_empty(plan->ok)) {
	printf(_("No changes made - plan is empty.\n"));
	return true;
    }

    if (!cwd_to_work_directory())
	return false;

    for (llist_iterator(plan->ok, &it); it.has_next(&it); ) {
	FileSpec *spec = it.next(&it);
	if (spec->status != STATUS_APPLY && spec->status != STATUS_CIRCULAR)
	    continue;
	display_names(spec);
	if (spec->prev_spec != NULL) {
	    puts(_("  Skipping - depends on failed\n"));
	    continue;
	}
	if (simulate || perform_command(spec)) {
	    it.remove(&it);
	    llist_remove(work_files, spec); /* XXX: This is slow */
	    free_file_spec(spec); /* Sets prev_spec of next_spec as well! */
	} else {
	    fail = true;
	}
    }

    if (fail)
	printf(_("Some commands failed. Retry with `apply' or edit again with `edit'.\n"));

    if (!cwd_from_work_directory())
	return false;

    return !fail;
}
