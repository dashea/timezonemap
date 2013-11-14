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

#ifdef __sun
static float convert_pos (gchar *pos, int digits);
#endif
static int compare_country_names (const void *a, const void *b);
static void sort_locations_by_country (GPtrArray *locations);
static gchar * tz_data_file_get (gchar *env, gchar *defaultfile);

G_DEFINE_TYPE (CcTimezoneLocation, cc_timezone_location, G_TYPE_OBJECT)

#define TIMEZONE_LOCATION_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_TIMEZONE_LOCATION, CcTimezoneLocationPrivate))

struct _CcTimezoneLocationPrivate
{
    gchar *country;
    gchar *full_country;
    gchar *en_name;
    gchar *state;
    gdouble latitude;
    gdouble longitude;
    gchar *zone;
    gchar *comment;

    gdouble dist; /* distance to clicked point for comparison */
};

enum {
  PROP_0,
  PROP_COUNTRY,
  PROP_FULL_COUNTRY,
  PROP_EN_NAME,
  PROP_STATE,
  PROP_LATITUDE,
  PROP_LONGITUDE,
  PROP_ZONE,
  PROP_COMMENT,
  PROP_DIST,
};

static void
cc_timezone_location_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CcTimezoneLocationPrivate *priv = CC_TIMEZONE_LOCATION (object)->priv;
  switch (property_id) {
    case PROP_COUNTRY:
      g_value_set_string (value, priv->country);
      break;
    case PROP_FULL_COUNTRY:
      g_value_set_string (value, priv->full_country);
      break;
    case PROP_EN_NAME:
      g_value_set_string (value, priv->en_name);
      break;
    case PROP_STATE:
      g_value_set_string (value, priv->state);
      break;
    case PROP_LATITUDE:
      g_value_set_double (value, priv->latitude);
      break;
    case PROP_LONGITUDE:
      g_value_set_double (value, priv->longitude);
      break;
    case PROP_ZONE:
      g_value_set_string (value, priv->zone);
      break;
    case PROP_COMMENT:
      g_value_set_string (value, priv->comment);
      break;
    case PROP_DIST:
      g_value_set_double (value, priv->dist);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_timezone_location_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcTimezoneLocationPrivate *priv = CC_TIMEZONE_LOCATION (object)->priv;
  switch (property_id) {
    case PROP_COUNTRY:
      priv->country = g_value_get_string(value);
      break;
    case PROP_FULL_COUNTRY:
      priv->full_country = g_value_get_string(value);
      break;
    case PROP_EN_NAME:
      priv->en_name = g_value_get_string(value);
      break;
    case PROP_STATE:
      priv->state = g_value_get_string(value);
      break;
    case PROP_LATITUDE:
      priv->latitude = g_value_get_double(value);
      break;
    case PROP_LONGITUDE:
      priv->longitude = g_value_get_double(value);
      break;
    case PROP_ZONE:
      priv->zone = g_value_get_string(value);
      break;
    case PROP_COMMENT:
      priv->comment = g_value_get_string(value);
      break;
    case PROP_DIST:
      priv->dist = g_value_get_double(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_timezone_location_dispose (GObject *object)
{
  CcTimezoneLocationPrivate *priv = CC_TIMEZONE_LOCATION (object)->priv;

  if (priv->country) 
    {
      g_free (priv->country);
      priv->country = NULL;
    }

  if (priv->full_country) 
    {
      g_free (priv->full_country);
      priv->full_country = NULL;
    }

  if (priv->state) 
    {
      g_free (priv->state);
      priv->state = NULL;
    }

  if (priv->zone) 
    {
      g_free (priv->zone);
      priv->zone = NULL;
    }

  if (priv->comment) 
    {
      g_free (priv->comment);
      priv->comment = NULL;
    }

  G_OBJECT_CLASS (cc_timezone_location_parent_class)->dispose (object);
}

static void
cc_timezone_location_finalize (GObject *object)
{
  CcTimezoneLocationPrivate *priv = CC_TIMEZONE_LOCATION (object)->priv;
  G_OBJECT_CLASS (cc_timezone_location_parent_class)->finalize (object);
}

static void
cc_timezone_location_class_init (CcTimezoneLocationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (CcTimezoneLocationPrivate));

  object_class->get_property = cc_timezone_location_get_property;
  object_class->set_property = cc_timezone_location_set_property;
  object_class->dispose = cc_timezone_location_dispose;
  object_class->finalize = cc_timezone_location_finalize;

  g_object_class_install_property(object_class,
                                  PROP_COUNTRY,
                                  g_param_spec_string ("country",
                                          "Country",
                                          "The country for the location",
                                          "",
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_FULL_COUNTRY,
                                  g_param_spec_string ("full_country",
                                          "Country (full name)",
                                          "The full country name",
                                          "",
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_EN_NAME,
                                  g_param_spec_string ("en_name",
                                          "English Name",
                                          "The name of the location",
                                          "",
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_STATE,
                                  g_param_spec_string ("state",
                                          "State",
                                          "The state for the location",
                                          "",
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_LATITUDE,
                                  g_param_spec_double ("latitude",
                                          "Latitude",
                                          "The latitude for the location",
                                          -90.0,
                                          90.0,
                                          0.0,
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_LONGITUDE,
                                  g_param_spec_double ("longitude",
                                          "Longitude",
                                          "The longitude for the location",
                                          -180.0,
                                          180.0,
                                          0.0,
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_ZONE,
                                  g_param_spec_string ("zone",
                                          "Zone",
                                          "The time zone for the location",
                                          "",
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_COMMENT,
                                  g_param_spec_string ("Comment",
                                          "Comment",
                                          "A comment for the location",
                                          "",
                                          G_PARAM_READWRITE));
  g_object_class_install_property(object_class,
                                  PROP_DIST,
                                  g_param_spec_double ("dist",
                                          "Distance",
                                          "The distance for the location",
                                          0.0,
                                          DBL_MAX,
                                          0.0,
                                          G_PARAM_READWRITE));
}

static void
cc_timezone_location_init (CcTimezoneLocation *self) {
  CcTimezoneLocationPrivate *priv;
  priv = self->priv = TIMEZONE_LOCATION_PRIVATE (self);
}

CcTimezoneLocation *
cc_timezone_location_new (void)
{
  return g_object_new (CC_TYPE_TIMEZONE_LOCATION, NULL);
}

void parse_file (const char * filename,
                 const guint ncolumns,
                 GFunc func,
                 gpointer user_data)
{
    FILE *fh = fopen (filename, "r");
    char buf[4096];

    if (!fh) 
      {
        g_warning ("Could not open *%s*\n", filename);
        fclose (fh);
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

void parse_admin1Codes (gpointer parsed_data,
                        gpointer user_data)
{
    gchar ** parsed_data_v = (gchar **) parsed_data;
    GHashTable * hash_table = (GHashTable *) user_data;

    g_hash_table_insert (hash_table,
            g_strdup (parsed_data_v[0]),
            g_strdup (parsed_data_v[1]));

    g_strfreev (parsed_data_v);

}

void parse_countrycode (gpointer parsed_data,
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

void parse_cities15000 (gpointer parsed_data, 
                        gpointer user_data)
{
    gchar ** parsed_data_v = (gchar **) parsed_data;
    Triple * triple = (Triple *) user_data;
    GPtrArray * ptr_array = (GPtrArray *) triple->first;
    GHashTable * stateHash = (GHashTable *) triple->second;
    GHashTable * countryHash = (GHashTable *) triple->third;

    CcTimezoneLocation *loc = cc_timezone_location_new ();

    loc->priv->country = g_strdup (parsed_data_v[8]);
    loc->priv->en_name = g_strdup (parsed_data_v[2]);

    gchar * tmpState = g_strdup_printf ("%s.%s", loc->priv->country,
            parsed_data_v[10]);
    loc->priv->state = g_strdup (
            (gchar *) g_hash_table_lookup (
                stateHash,
                tmpState));
    g_free (tmpState);

    loc->priv->full_country = g_strdup (
            (gchar *) g_hash_table_lookup (
                countryHash,
                loc->priv->country));

    loc->priv->zone = g_strdup (parsed_data_v[17]);
    loc->priv->latitude  = g_ascii_strtod(parsed_data_v[4], NULL);
    loc->priv->longitude = g_ascii_strtod(parsed_data_v[5], NULL);

#ifdef __sun
    gchar *latstr, *lngstr, *p;

    latstr = g_strdup (parsed_data_v[1]);
    p = latstr + 1;
    while (*p != '-' && *p != '+') p++;
    lngstr = g_strdup (p);
    *p = '\0';

    if (parsed_data_v[3] && *parsed_data_v[3] == '-' && parsed_data_v[4])
        loc->comment = g_strdup (parsed_data_v[4]);

    if (parsed_data_v[3] && *parsed_data_v[3] != '-' && !islower(loc->zone)) {
        CcTimezoneLocation *locgrp;

        /* duplicate entry */
        locgrp = cc_timezone_location_new ();
        locgrp->country = g_strdup (parsed_data_v[0]);
        locgrp->en_name = NULL;
        locgrp->zone = g_strdup (parsed_data_v[3]);
        locgrp->latitude  = convert_pos (latstr, 2);
        locgrp->longitude = convert_pos (lngstr, 3);
        locgrp->comment = (parsed_data_v[4]) ? g_strdup (parsed_data_v[4]) : NULL;

        g_ptr_array_add (ptr_array, (gpointer) locgrp);
    }
#else
    loc->priv->comment = NULL;
#endif

    g_ptr_array_add (ptr_array, (gpointer) loc);

#ifdef __sun
    g_free (latstr);
    g_free (lngstr);
#endif
    g_strfreev (parsed_data_v);

    return;
}


/* ---------------- *
 * Public interface *
 * ---------------- */
TzDB *
tz_load_db (void)
{
    gchar *tz_data_file, *admin1_file, *country_file;
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

    Triple * triple = g_new (Triple, 1);
    triple->first = tz_db->locations;
    triple->second = stateHash;
    triple->third = countryHash;

    parse_file (tz_data_file, 19, parse_cities15000, triple);

    g_hash_table_destroy (stateHash);
    g_hash_table_destroy (countryHash);
    triple->second = NULL;
    triple->third = NULL;

    /* now sort by country */
    sort_locations_by_country (tz_db->locations);

    g_free (tz_data_file);

    return tz_db;
}

void
tz_db_free (TzDB *db)
{
    g_ptr_array_foreach (db->locations, (GFunc) g_object_unref, NULL);
    g_ptr_array_free (db->locations, TRUE);
    g_free (db);
}

static gint
sort_locations (CcTimezoneLocation *a,
                CcTimezoneLocation *b)
{
  if (a->priv->dist > b->priv->dist)
    return 1;

  if (a->priv->dist < b->priv->dist)
    return -1;

  return 0;
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

    g_return_val_if_fail (loc != NULL, 0);
    g_return_val_if_fail (loc->priv->zone != NULL, 0);

    curtime = time (NULL);
    curzone = localtime (&curtime);
    is_dst = curzone->tm_isdst;

    setenv ("TZ", loc->priv->zone, 1);
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

    g_return_val_if_fail (loc != NULL, NULL);
    g_return_val_if_fail (loc->priv->zone != NULL, NULL);

    setenv ("TZ", loc->priv->zone, 1);

#if 0
    tzset ();
#endif
    tzinfo = g_new0 (TzInfo, 1);

    curtime = time (NULL);
    curzone = localtime (&curtime);

#ifndef __sun
    /* Currently this solution doesnt seem to work - I get that */
    /* America/Phoenix uses daylight savings, which is wrong    */
    tzinfo->tzname_normal = g_strdup (curzone->tm_zone);
    if (curzone->tm_isdst)
        tzinfo->tzname_daylight =
            g_strdup (&curzone->tm_zone[curzone->tm_isdst]);
    else
        tzinfo->tzname_daylight = NULL;

    tzinfo->utc_offset = curzone->tm_gmtoff;
#else
    tzinfo->tzname_normal = NULL;
    tzinfo->tzname_daylight = NULL;
    tzinfo->utc_offset = 0;
#endif

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

static gchar *
tz_data_file_get (gchar *env, gchar *defaultfile)
{
    /* Allow passing this in at runtime, to support loading it from the build
     * tree during tests. */
    const gchar * filename = g_getenv (env);

    return filename ? g_strdup (filename) : g_strdup (defaultfile);
}

#ifdef __sun
static float
convert_pos (gchar *pos, int digits)
{
    gchar whole[10];
    gchar *fraction;
    gint i;
    float t1, t2;

    if (!pos || strlen(pos) < 4 || digits > 9) return 0.0;

    for (i = 0; i < digits + 1; i++) whole[i] = pos[i];
    whole[i] = '\0';
    fraction = pos + digits + 1;

    t1 = g_strtod (whole, NULL);
    t2 = g_strtod (fraction, NULL);

    if (t1 >= 0.0) return t1 + t2/pow (10.0, strlen(fraction));
    else return t1 - t2/pow (10.0, strlen(fraction));
}
#endif

    static int
compare_country_names (const void *a, const void *b)
{
    const CcTimezoneLocation *tza = * (CcTimezoneLocation **) a;
    const CcTimezoneLocation *tzb = * (CcTimezoneLocation **) b;

    return strcmp (tza->priv->zone, tzb->priv->zone);
}


    static void
sort_locations_by_country (GPtrArray *locations)
{
    qsort (locations->pdata, locations->len, sizeof (gpointer),
            compare_country_names);
}
