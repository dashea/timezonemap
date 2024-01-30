AC_INIT([libtimezonemap],
        [0.4.4],
        [http://bugs.launchpad.net/libtimezonemap],
        [libtimezonemap],
        [http://launchpad.net/libtimezonemap])
AC_COPYRIGHT([Copyright 2009,2010 Canonical])

AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_PROG_AR
LT_INIT

AM_INIT_AUTOMAKE([1.11 -Wall dist-xz])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O

AC_CHECK_FUNCS([setenv])
AC_CHECK_FUNCS([strrchr])
AC_STRUCT_TIMEZONE

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])


PKG_PROG_PKG_CONFIG

###########################
# Dependencies
###########################

GTK3_REQUIRED_VERSION=3.1.4
SOUP_REQUIRED_VERSION=2.42.0

PKG_CHECK_MODULES(LIBTIMEZONEMAP, gtk+-3.0 >= $GTK3_REQUIRED_VERSION
                                  libsoup-2.4 >= $SOUP_REQUIRED_VERSION
                                  json-glib-1.0
                                  librsvg-2.0)
LIBTIMEZONEMAP_LIBS="$LIBTIMEZONEMAP_LIBS $LIBM"

GOBJECT_INTROSPECTION_CHECK([0.6.7])

###########################
# Files
###########################

AC_CONFIG_FILES([
Makefile
glade/Makefile
src/Makefile
src/timezonemap.pc
])
AC_OUTPUT

###########################
# Results
###########################

AC_MSG_NOTICE([

Timezone Map Configuration:

	Prefix:        $prefix
])
