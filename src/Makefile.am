CLEANFILES = 

uidir = $(pkgdatadir)
dist_ui_DATA = \
	data/pin.png \
	data/time_zones_countryInfo-orig.svg \
	data/admin1Codes.txt \
	data/countryInfo.txt

nodist_ui_DATA = \
	data/citiesInfo.txt

dist_noinst_DATA = \
	data/cities15000.txt \
	data/citiesExtra.txt

data/citiesInfo.txt: data/cities15000.txt data/citiesExtra.txt
	@$(MKDIR_P) $(builddir)/data
	$(AM_V_GEN)cat $(srcdir)/data/cities15000.txt $(srcdir)/data/citiesExtra.txt > $@

CLEANFILES = data/citiesInfo.txt

tzdatadir = $(pkgdatadir)/
dist_tzdata_DATA = data/backward

pkgconfigdir = $(libdir)/pkgconfig
dist_pkgconfig_DATA = timezonemap.pc

AM_CPPFLAGS =						\
	$(LIBTIMEZONEMAP_CFLAGS)				\
	-DDATADIR="\"$(uidir)\""			\
	$(NULL)

noinst_PROGRAMS = test-timezone


AM_TESTS_ENVIRONMENT = TZ_DATA_FILE=$(builddir)/data/citiesInfo.txt ; \
		       ADMIN1_FILE=$(srcdir)/data/admin1Codes.txt ; \
		       COUNTRY_FILE=$(srcdir)/data/countryInfo.txt ; \
		       DATADIR=$(srcdir)/data ; \
		       export TZ_DATA_FILE ADMIN1_FILE COUNTRY_FILE DATADIR ;

check_PROGRAMS = test-timezone \
		 test-clickability

test_timezone_SOURCES = test-timezone.c
test_timezone_LDADD = $(LIBTIMEZONEMAP_LIBS) -lm libtimezonemap.la
test_timezone_CFLAGS = $(LIBTIMEZONEMAP_CFLAGS)

test_clickability_SOURCES = test-clickability.c
test_clickability_LDADD = $(LIBTIMEZONEMAP_LIBS) $(LIBXML_LIBS) libtimezonemap.la
test_clickability_CFLAGS = $(LIBTIMEZONEMAP_CFLAGS) $(LIBXML_CFLAGS)

TESTS = test-timezone test-clickability

lib_LTLIBRARIES = libtimezonemap.la

libtimezonemap_GISOURCES = cc-timezone-map.c cc-timezone-map.h \
			   cc-timezone-location.c cc-timezone-location.h \
			   timezone-completion.c timezone-completion.h
libtimezonemap_NONGISOURCES = tz.c tz.h
libtimezonemap_la_SOURCES = $(libtimezonemap_GISOURCES) $(libtimezonemap_NONGISOURCES)

# Specify 'timezonemap' twice: once for package (so we could eventually add
# a timezonemap-gtk4 for example), and once for namespacing inside code so
# that users do "#include <timezonemap/tz.h>" instead of "#include <tz.h>"
timezonemapincludesdir = $(includedir)/timezonemap/timezonemap
timezonemapincludes_HEADERS = \
  cc-timezone-location.h \
  cc-timezone-map.h \
  timezone-completion.h \
  tz.h

libtimezonemap_la_LIBADD = $(LIBTIMEZONEMAP_LIBS) -lm
libtimezonemap_la_LDFLAGS = \
	-version-info 1:0:0 \
	-no-undefined \
	-export-symbols-regex "^[^_].*"

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

INTROSPECTION_GIRS = TimezoneMap-1.0.gir

introspection_sources = $(libtimezonemap_GISOURCES)

TimezoneMap-1.0.gir: libtimezonemap.la
TimezoneMap_1_0_gir_INCLUDES = GObject-2.0 Gtk-3.0 Json-1.0
TimezoneMap_1_0_gir_CFLAGS = $(INCLUDES)
TimezoneMap_1_0_gir_SCANNERFLAGS = --identifier-prefix=Cc --warn-all
TimezoneMap_1_0_gir_LIBS = libtimezonemap.la
TimezoneMap_1_0_gir_FILES = $(introspection_sources)

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
