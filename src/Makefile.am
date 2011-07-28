CLEANFILES = 

uidir = $(pkgdatadir)/libmap/ui
dist_ui_DATA = \
	data/bg.png \
	data/cc.png \
	data/olsen_map.png \
	data/pin.png \
	data/timezone_0.png \
	data/timezone_-10.png \
	data/timezone_10.png \
	data/timezone_10.5.png \
	data/timezone_-1.png \
	data/timezone_1.png \
	data/timezone_-11.png \
	data/timezone_11.png \
	data/timezone_11.5.png \
	data/timezone_12.png \
	data/timezone_12.75.png \
	data/timezone_13.png \
	data/timezone_-2.png \
	data/timezone_2.png \
	data/timezone_-3.png \
	data/timezone_3.png \
	data/timezone_-3.5.png \
	data/timezone_3.5.png \
	data/timezone_-4.png \
	data/timezone_4.png \
	data/timezone_-4.5.png \
	data/timezone_4.5.png \
	data/timezone_-5.png \
	data/timezone_5.png \
	data/timezone_-5.5.png \
	data/timezone_5.5.png \
	data/timezone_5.75.png \
	data/timezone_-6.png \
	data/timezone_6.png \
	data/timezone_6.5.png \
	data/timezone_-7.png \
	data/timezone_7.png \
	data/timezone_-8.png \
	data/timezone_8.png \
	data/timezone_-9.png \
	data/timezone_9.png \
	data/timezone_-9.5.png \
	data/timezone_9.5.png

tzdatadir = $(pkgdatadir)/libmap/datetime
dist_tzdata_DATA = backward

AM_CPPFLAGS =						\
	$(LIBMAP_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)/libmap\""		\
	-DDATADIR="\"$(uidir)\""			\
	$(NULL)

noinst_PROGRAMS = test-timezone

test_timezone_SOURCES = test-timezone.c tz.c tz.h
test_timezone_LDADD = $(LIBMAP_LIBS)
test_timezone_CFLAGS = $(LIBMAP_CFLAGS)

all-local: check-local

# FIXME remove "|| :" when we have all the necessary pixmaps
check-local: test-timezone
	$(builddir)/test-timezone $(srcdir)/data || :

lib_LTLIBRARIES = libmap.la

libmap_la_SOURCES =	\
	cc-timezone-map.c	\
	cc-timezone-map.h	\
	tz.c tz.h

libmap_la_LIBADD = $(LIBMAP_LIBS)
libmap_la_LDFLAGS = \
	-version-info 1:0:0 \
	-no-undefined \
	-export-symbols-regex "^[^_].*"

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

INTROSPECTION_GIRS = Map-1.0.gir

introspection_sources = $(libmap_la_SOURCES)

Map-1.0.gir: libmap.la
Map_1_0_gir_INCLUDES = GObject-2.0 Gtk-2.0
Map_1_0_gir_CFLAGS = $(INCLUDES) --identifier-prefix=Cc
Map_1_0_gir_LIBS = libmap.la
Map_1_0_gir_FILES = $(introspection_sources)

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
