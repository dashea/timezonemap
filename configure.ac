AC_INIT([libtimezonemap],
        [1.0.0],
        [http://bugs.launchpad.net/libtimezonemap],
        [libtimezonemap],
        [http://launchpad.net/libtimezonemap])
AC_COPYRIGHT([Copyright 2009,2010 Canonical])

AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 -Wall dist-xz])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])


AC_ARG_ENABLE([deprecations],
  [AS_HELP_STRING([--enable-deprecations],
    [allow deprecated API usage @<:@default=yes@:>@])],
  [],
  [enable_deprecations=yes])
AS_IF([test "x$enable_deprecations" = xno],
  [CFLAGS="$CFLAGS -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE -DGTK_DISABLE_SINGLE_INCLUDES"]
)

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

PKG_PROG_PKG_CONFIG

###########################
# Dependencies
###########################

CAIRO_REQUIRED_VERSION=1.10
GDK_REQUIRED_VERSION=2.22
GLIB_REQUIRED_VERSION=2.26
GTK3_REQUIRED_VERSION=3.1.4
GIO_REQUIRED_VERSION=2.5.11
SOUP_REQUIRED_VERSION=2.42.0

AC_CHECK_LIBM

PKG_CHECK_MODULES(LIBTIMEZONEMAP, gio-2.0 >= $GIO_REQUIRED_VERSION
                                  gtk+-3.0 >= $GTK3_REQUIRED_VERSION
                                  libsoup-2.4 >= $SOUP_REQUIRED_VERSION
                                  json-glib-1.0)
LIBTIMEZONEMAP_LIBS="$LIBTIMEZONEMAP_LIBS $LIBM"

GOBJECT_INTROSPECTION_CHECK([0.6.7])

###########################
# Grab the GSettings Macros
###########################

GLIB_GSETTINGS

###########################
# Check to see if we're local
###########################

with_localinstall="no"
AC_ARG_ENABLE(localinstall, AS_HELP_STRING([--enable-localinstall], [install all of the files localy instead of system directories (for distcheck)]), with_localinstall=$enableval, with_localinstall=no)

###########################
# Files
###########################

AC_CONFIG_FILES([
Makefile
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
