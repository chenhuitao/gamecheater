# Process this file with autoconf to produce a configure script.

AC_INIT([gamecheater], [0.5], [h980501427@163.com])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

LIB_CURRENT=0
LIB_REVERSION=0
LIB_AGE=0 

VERSION_INFO="$LIB_CURRENT:$LIB_REVERSION:$LIB_AGE"
AC_SUBST(VERSION_INFO)

# Checks for libraries.
PKG_CHECK_MODULES(DEPS, gtk+-3.0 >= 3.0 glib-2.0 >= 2.6)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(stdlib.h string.h unistd.h)

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_STAT
AC_CHECK_FUNCS(ptrace memcpy memcmp)

#AM_GNU_GETTEXT
AM_GNU_GETTEXT([external])
ALL_LINGUAS="zh_CN"

AC_OUTPUT([
    Makefile
    libgcheater/Makefile
    src/Makefile
    pixmaps/Makefile
    ui/Makefile
    po/Makefile.in
])
