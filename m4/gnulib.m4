# Copyright (C) 2004 Free Software Foundation, Inc.
# This file is free software, distributed under the terms of the GNU
# General Public License.  As a special exception to the GNU General
# Public License, this file may be distributed as part of a program
# that contains a configuration script generated by Autoconf, under
# the same distribution terms as the rest of that program.
#
# Generated by gnulib-tool.
#
# Invoked as: gnulib-tool --import --symlink
# Reproduce by: gnulib-tool --import --dir=. --lib=libgnu --source-base=lib --m4-base=m4 --aux-dir=.   alloca alloca-opt dirname dup2 error exit exitfail extensions free getdelim getline getopt gettext-h gettimeofday lstat malloc mbchar memchr memcpy memmove memset minmax progname quote quotearg realloc regex restrict rpmatch size_max stat-macros stdbool stdint strcase strdup strerror strndup strnlen strnlen1 strstr vasnprintf vasprintf version-etc xalloc xalloc-die xsize xvasprintf yesno

AC_DEFUN([gl_EARLY],
[
  AC_GNU_SOURCE
  gl_USE_SYSTEM_EXTENSIONS
])

AC_DEFUN([gl_INIT],
[
  gl_FUNC_ALLOCA
  gl_DIRNAME
  gl_FUNC_DUP2
  gl_ERROR
  gl_EXITFAIL
  dnl gl_USE_SYSTEM_EXTENSIONS must be added quite early to configure.ac.
  gl_FUNC_FREE
  gl_FUNC_GETDELIM
  gl_FUNC_GETLINE
  gl_GETOPT
  AC_FUNC_GETTIMEOFDAY_CLOBBER
  gl_FUNC_LSTAT
  AC_FUNC_MALLOC
  gl_MBCHAR
  gl_FUNC_MEMCHR
  gl_FUNC_MEMCPY
  gl_FUNC_MEMMOVE
  gl_FUNC_MEMSET
  gl_MINMAX
  gl_QUOTE
  gl_QUOTEARG
  AC_FUNC_REALLOC
  gl_REGEX
  gl_C_RESTRICT
  gl_FUNC_RPMATCH
  gl_SIZE_MAX
  gl_STAT_MACROS
  AM_STDBOOL_H
  gl_STDINT_H
  gl_STRCASE
  gl_FUNC_STRDUP
  gl_FUNC_STRERROR
  gl_FUNC_STRNDUP
  gl_FUNC_STRNLEN
  gl_FUNC_STRSTR
  gl_FUNC_VASNPRINTF
  gl_FUNC_VASPRINTF
  gl_XALLOC
  gl_XSIZE
  gl_YESNO
])

dnl Usage: gl_MODULES(module1 module2 ...)
AC_DEFUN([gl_MODULES], [])

dnl Usage: gl_AVOID(module1 module2 ...)
AC_DEFUN([gl_AVOID], [])

dnl Usage: gl_SOURCE_BASE(DIR)
AC_DEFUN([gl_SOURCE_BASE], [])

dnl Usage: gl_M4_BASE(DIR)
AC_DEFUN([gl_M4_BASE], [])

dnl Usage: gl_LIB(LIBNAME)
AC_DEFUN([gl_LIB], [])

dnl Usage: gl_LGPL
AC_DEFUN([gl_LGPL], [])

# gnulib.m4 ends here
