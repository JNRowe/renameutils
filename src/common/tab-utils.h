/* tab-utils.h - Utility functions dealing with strings and tabs.
 *
 * Copyright (C) 1998-2005 Oskar Liljeblad
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef COMMON_TAB_UTILS_H
#define COMMON_TAB_UTILS_H

#include <stdio.h>	/* C89 */

void tab_to(FILE *out, int from, int to, int tabsize);
size_t tab_len(const char *string, int tabsize);
char tab_char_at(const char *string, int pos, int tabsize);
int tab_index(const char *string, int pos, int tabsize);

extern inline char
tab_char_at(const char *string, int pos, int tabsize)
{
	return string[tab_index(string, pos, tabsize)];
}

#endif