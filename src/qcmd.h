/* qcmd.h - Common definitions for qcmd.
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

#ifndef QCMD_H
#define QCMD_H

#include <stdio.h>		/* for FILE */
#include "common/llist.h"
#include "common/common.h"	/* for bool */

typedef enum {
    STATUS_UNCHECKED,	    	/* (not_checked) status not checked) */
    STATUS_OLD_MISSING,		/* (error) old file no longer exists */
    STATUS_DUPLICATE,		/* (error) part of a set with duplicate destinations */
    STATUS_NO_CHANGE,		/* (no_change) old_name eq new_name */
    STATUS_NEW_EXISTS,		/* (error) new file already exists */
    STATUS_APPLY,		/* (ok) everything is ok, rename/copy file */
    STATUS_CIRCULAR,		/* (ok) a temporary rename need to be made first */
} FileSpecStatus;

typedef struct _EditFormat EditFormat;
typedef struct _FileSpec FileSpec;
typedef struct _ApplyPlan ApplyPlan;

struct _EditFormat {
    char *name;
    char *short_name;
    bool (*parse_options)(char *);
    char *(*option_generator)(const char *, int);
    void (*output)(FILE *, LList *);
    bool (*input)(FILE *, LList *);
};

struct _FileSpec {
    char *old_name;
    char *new_name;
    FileSpec *prev_spec;	/* this must be completed first */
    FileSpec *next_spec;     	/* set if someone depends on us */
    FileSpecStatus status;
};

struct _ApplyPlan {
    /* It is important that this list is sorted according to dependency
     * prior to calling apply. Normally the sorting is done automaticly
     * as entries are inserted into the list.
     */
    LList *ok;
    LList *error;
    LList *no_change;
    /*LList *free_spec;*/
};

/* qcmd.c */
extern bool simulate;
extern LList *work_files;   	/* contains FileSpec * */
extern char *work_directory;
extern char *editor_program;
extern char *ls_program;
extern char *edit_filename;
extern char *program;
extern EditFormat *format;
extern ApplyPlan *plan;
void display_version(void);
void display_help(FILE *out);

/* edit.c */
extern LList *edit_file_list;
EditFormat *find_edit_format_by_name(const char *name);
//void edit_command(char **args);
bool edit_files(bool all, bool force);
char *edit_format_generator(const char *text, int state);

/* list.c */
void import_command(char **args);
void list_command(char **args);
bool list_files(char **args);
void process_ls_option(int c);
struct option *append_ls_options(const struct option *options);
FileSpec *new_file_spec(void);
void free_file_spec(FileSpec *spec);
void free_files(LList *files);
void display_ls_help(FILE *out);
bool cwd_to_work_directory();
bool cwd_from_work_directory();
void dump_spec_list(LList *list);

/* quote.c */
char *quote_output_file(const char *t);
char *dequote_output_file(const char *str);

/* editformats/(misc).c */
extern EditFormat single_column_format;
extern EditFormat dual_column_format;
extern EditFormat destination_only_format;

/* interactive.c */
void commandmode_mainloop(void);
void display_commandmode_header(void);

/* variables.c */
void set_command(char **args);
void show_command(char **args);
char *variable_generator(const char *text, int state);

/* plan.c */
ApplyPlan *make_plan(LList *file_names);
void free_plan(ApplyPlan *plan);

/* display.c */
void display_plan(ApplyPlan *plan);
void display_names(FileSpec *spec);

/* planaction.c */
bool apply_plan(ApplyPlan *plan);

/* apply.c */
extern char *force_command;

#endif
