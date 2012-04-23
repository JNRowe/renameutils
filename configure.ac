#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Initialization
AC_PREREQ(2.59)
AC_INIT(renameutils, 0.12.0, oskar@osk.mine.nu)
AC_CONFIG_MACRO_DIR([m4]) # doesn't seem to have any effect at the moment
AC_CONFIG_SRCDIR([src/qcmd.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE

# Initialize config.h
AC_GNU_SOURCE

# Checks for programs, initialize Gnulib
AC_PROG_CC
gl_EARLY
AC_PROG_INSTALL
#AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LN_S
gl_INIT

# Gettext
AM_GNU_GETTEXT([external])

# Checks for libraries.
OL_LIB_READLINE
if test "$ol_cv_lib_readline" = ""; then
  AC_MSG_ERROR([cannot find a readline compatible library])
fi
ORIG_LIBS="$LIBS"
LIBS="$LIBS $READLINE_LIBS"
AC_CHECK_FUNC(rl_completion_matches, [], [
  AC_CHECK_FUNC(completion_matches, [
    AC_DEFINE(rl_completion_matches, completion_matches, [Define to completion_matches if not available])
  ], [
    AC_MSG_ERROR([readline library not found, or readline is too old. 4.2 or above is required.])
  ])
])
LIBS="$ORIG_LIBS"

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS([fcntl.h stdint.h unistd.h regex.h wordexp.h sys/time.h])
# AC_HEADER_DIRENT: AC_CHECK_HEADERS([dirent.h])
# AC_HEADER_SYS_WAIT: AC_CHECK_HEADERS([sys/wait.h])
# always available (?): AC_CHECK_HEADERS([sys/stat.h sys/types.h])
# c89: AC_HEADER_STDC
# c89: AC_CHECK_HEADERS([stdint.h stdlib.h string.h locale.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
# c89: AC_TYPE_SIZE_T
# c89: AC_C_CONST
# gnulib: AC_HEADER_STDBOOL
AC_CHECK_TYPES([comparison_fn_t])

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_CHECK_FUNCS([fchdir lstat64])
# c89: AC_FUNC_VPRINTF
# c89: AC_CHECK_FUNCS([strchr strrchr setlocale])
# gnulib: AC_FUNC_LSTAT
# gnulib: AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
# gnulib: AC_FUNC_MALLOC
# gnulib: AC_FUNC_REALLOC
# gnulib: AC_FUNC_STAT
# gnulib: AC_CHECK_FUNCS([dup2 gettimeofday memmove strcasecmp strdup strerror strndup strstr])
AC_SYS_LARGEFILE

AC_CONFIG_FILES([Makefile
		 po/Makefile.in
		 gnulib-po/Makefile.in
		 gnulib-lib/Makefile
                 src/Makefile
                 src/common/Makefile
                 src/editformats/Makefile
		 renameutils.spec])
AC_OUTPUT
