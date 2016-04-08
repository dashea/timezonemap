/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Generic timezone utilities.
 *
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 * 
 * Largely based on Michael Fulbright's work on Anaconda.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include "tz.h"


/* Forward declarations for private functions */

static int compare_country_names (const void *a, const void *b);
static void sort_locations_by_country (GPtrArray *locations);
static const gchar * tz_data_file_get (const gchar *env, const gchar *defaultfile);

static void parse_file (const char * filename,
                 const guint ncolumns,
                 GFunc func,
                 gpointer user_data)
{
    FILE *fh = fopen (filename, "r");
    char buf[4096];

    if (!fh) 
      {
        g_warning ("Could not open *%s*\n", filename);
        return;
      }

    while (fgets (buf, sizeof(buf), fh)) 
      {
        if (*buf == '#') continue;

        g_strchomp (buf);
        func (g_strsplit (buf,"\t", ncolumns), user_data);
      }

    fclose (fh);
}

static void parse_admin1Codes (gpointer parsed_data,
                        gpointer user_data)
{
    gchar ** parsed_data_v = (gchar **) parsed_data;
    GHashTable * hash_table = (GHashTable *) user_data;

    g_hash_table_insert (hash_table,
            g_strdup (parsed_data_v[0]),
            g_strdup (parsed_data_v[1]));

    g_strfreev (parsed_data_v);

}

static void parse_countrycode (gpointer parsed_data,
                        gpointer user_data)
{
    gchar ** parsed_data_v = (gchar **) parsed_data;
    GHashTable * hash_table = (GHashTable *) user_data;

    g_hash_table_insert (hash_table,
            g_strdup (parsed_data_v[0]),
            g_strdup (parsed_data_v[4]));

    g_strfreev (parsed_data_v);
}

typedef struct Triple {
    gpointer first;
    gpointer second;
    gpointer third;
} Triple;

static void parse_cities15000 (gpointer parsed_data,
                        gpointer user_data)
{
    gchar ** parsed_data_v = (gchar **) parsed_data;
    Triple * triple = (Triple *) user_data;
    GPtrArray * ptr_array = (GPtrArray *) triple->first;
    GHashTable * stateHash = (GHashTable *) triple->second;
    GHashTable * countryHash = (GHashTable *) triple->third;

    CcTimezoneLocation *loc = cc_timezone_location_new ();

    cc_timezone_location_set_country(loc, parsed_data_v[8]);
    cc_timezone_location_set_en_name(loc, parsed_data_v[2]);

    gchar * tmpState = g_strdup_printf ("%s.%s",
            cc_timezone_location_get_country(loc),
            parsed_data_v[10]);
    cc_timezone_location_set_state(loc, g_hash_table_lookup(
                stateHash,
                tmpState));
    g_free (tmpState);

    cc_timezone_location_set_full_country(loc, g_hash_table_lookup(
                countryHash,
                cc_timezone_location_get_country(loc)));

    cc_timezone_location_set_zone(loc, parsed_data_v[17]);
    cc_timezone_location_set_latitude(loc, g_ascii_strtod(parsed_data_v[4], NULL));
    cc_timezone_location_set_longitude(loc, g_ascii_strtod(parsed_data_v[5], NULL));

    cc_timezone_location_set_comment(loc, NULL);

    g_ptr_array_add (ptr_array, (gpointer) loc);

    g_strfreev (parsed_data_v);

    return;
}


/* ---------------- *
 * Public interface *
 * ---------------- */
TzDB *
tz_load_db (void)
{
    const gchar *tz_data_file, *admin1_file, *country_file;
    TzDB *tz_db;
    char buf[4096];

    tz_data_file = tz_data_file_get ("TZ_DATA_FILE", TZ_DATA_FILE);
    if (!tz_data_file) 
      {
        g_warning ("Could not get the TimeZone data file name");
        return NULL;
      }

    admin1_file = tz_data_file_get ("ADMIN1_FILE", ADMIN1_FILE);
    if (!admin1_file) 
      {
        g_warning ("Could not get the admin1 data file name");
        return NULL;
      }

    country_file = tz_data_file_get ("COUNTRY_FILE", COUNTRY_FILE);
    if (!country_file) 
      {
        g_warning ("Could not get the country data file name");
        return NULL;
      }

    GHashTable *stateHash = g_hash_table_new_full (g_str_hash,
            g_str_equal, g_free, g_free);

    parse_file (admin1_file, 4, parse_admin1Codes, stateHash);

    GHashTable * countryHash = g_hash_table_new_full (g_str_hash,
            g_str_equal, g_free, g_free);

    parse_file (country_file, 19, parse_countrycode, countryHash);

    tz_db = g_new0 (TzDB, 1);
    tz_db->locations = g_ptr_array_new ();

    Triple triple;
    triple.first = tz_db->locations;
    triple.second = stateHash;
    triple.third = countryHash;

    parse_file (tz_data_file, 19, parse_cities15000, &triple);

    g_hash_table_destroy (stateHash);
    g_hash_table_destroy (countryHash);

    /* now sort by country */
    sort_locations_by_country (tz_db->locations);

    return tz_db;
}

void
tz_db_free (TzDB *db)
{
    g_ptr_array_foreach (db->locations, (GFunc) g_object_unref, NULL);
    g_ptr_array_free (db->locations, TRUE);
    g_free (db);
}

static gdouble
convert_longtitude_to_x (gdouble longitude, gint map_width)
{
  const gdouble xdeg_offset = -6;
  gdouble x;

  x = (map_width * (180.0 + longitude) / 360.0)
    + (map_width * xdeg_offset / 180.0);

  return x;
}

static gdouble
radians (gdouble degrees)
{
  return (degrees / 360.0) * G_PI * 2;
}

static gdouble
convert_latitude_to_y (gdouble latitude, gdouble map_height)
{
  gdouble bottom_lat = -59;
  gdouble top_lat = 81;
  gdouble top_per, y, full_range, top_offset, map_range;

  top_per = top_lat / 180.0;
  y = 1.25 * log (tan (G_PI_4 + 0.4 * radians (latitude)));
  full_range = 4.6068250867599998;
  top_offset = full_range * top_per;
  map_range = fabs (1.25 * log (tan (G_PI_4 + 0.4 * radians (bottom_lat))) - top_offset);
  y = fabs (y - top_offset);
  y = y / map_range;
  y = y * map_height;
  return y;
}

GPtrArray *
tz_get_locations (TzDB *db)
{
    return db->locations;
}

    glong
tz_location_get_utc_offset (CcTimezoneLocation *loc)
{
    TzInfo *tz_info;
    glong offset;

    tz_info = tz_info_from_location (loc);
    offset = tz_info->utc_offset;
    tz_info_free (tz_info);
    return offset;
}

gint
tz_location_set_locally (CcTimezoneLocation *loc)
{
    time_t curtime;
    struct tm *curzone;
    gboolean is_dst = FALSE;
    gint correction = 0;
    const gchar *zone;

    g_return_val_if_fail (loc != NULL, 0);
    zone = cc_timezone_location_get_zone(loc);
    g_return_val_if_fail (zone != NULL, 0);

    curtime = time (NULL);
    curzone = localtime (&curtime);
    is_dst = curzone->tm_isdst;

    setenv ("TZ", zone, 1);
#if 0
    curtime = time (NULL);
    curzone = localtime (&curtime);

    if (!is_dst && curzone->tm_isdst) {
        correction = (60 * 60);
    }
    else if (is_dst && !curzone->tm_isdst) {
        correction = 0;
    }
#endif

    return correction;
}

TzInfo *
tz_info_from_location (CcTimezoneLocation *loc)
{
    TzInfo *tzinfo;
    time_t curtime;
    struct tm *curzone;
    const gchar *zone;

    g_return_val_if_fail (loc != NULL, NULL);
    zone = cc_timezone_location_get_zone(loc);
    g_return_val_if_fail (zone != NULL, NULL);

    setenv ("TZ", zone, 1);

#if 0
    tzset ();
#endif
    tzinfo = g_new0 (TzInfo, 1);

    curtime = time (NULL);
    curzone = localtime (&curtime);

    /* Currently this solution doesnt seem to work - I get that */
    /* America/Phoenix uses daylight savings, which is wrong    */
    tzinfo->tzname_normal = g_strdup (curzone->tm_zone);
    if (curzone->tm_isdst)
        tzinfo->tzname_daylight =
            g_strdup (&curzone->tm_zone[curzone->tm_isdst]);
    else
        tzinfo->tzname_daylight = NULL;

    tzinfo->utc_offset = curzone->tm_gmtoff;

    tzinfo->daylight = curzone->tm_isdst;

    return tzinfo;
}


    void
tz_info_free (TzInfo *tzinfo)
{
    g_return_if_fail (tzinfo != NULL);

    if (tzinfo->tzname_normal) g_free (tzinfo->tzname_normal);
    if (tzinfo->tzname_daylight) g_free (tzinfo->tzname_daylight);
    g_free (tzinfo);
}

/* ----------------- *
 * Private functions *
 * ----------------- */

static const gchar *
tz_data_file_get (const gchar *env, const gchar *defaultfile)
{
    /* Allow passing this in at runtime, to support loading it from the build
     * tree during tests. */
    const gchar * filename = g_getenv (env);

    return filename ? filename : defaultfile;
}


static int
compare_country_names (const void *a, const void *b)
{
    CcTimezoneLocation *tza = * (CcTimezoneLocation **) a;
    CcTimezoneLocation *tzb = * (CcTimezoneLocation **) b;
    const gchar *zone_a = cc_timezone_location_get_zone(tza);
    const gchar *zone_b = cc_timezone_location_get_zone(tzb);

    return strcmp (zone_a, zone_b);
}


    static void
sort_locations_by_country (GPtrArray *locations)
{
    qsort (locations->pdata, locations->len, sizeof (gpointer),
            compare_country_names);
}
