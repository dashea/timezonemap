#include <config.h>
#include <locale.h>
#include <librsvg/rsvg.h>
#include <math.h>

#include "tz.h"

int main (int argc, char **argv)
{
    TzDB *db;
    GPtrArray *locs;
    guint i;
    char *pixmap_dir;
    int retval = 0;
    RsvgHandle *svg;
    gchar *path;

        setlocale (LC_ALL, "");

    if (argc == 2) 
      {
        pixmap_dir = g_strdup (argv[1]);
      } else if (argc == 1) {
        pixmap_dir = g_strdup ("data/");
      } else {
        g_message ("Usage: %s [PIXMAP DIRECTORY]", argv[0]);
        return 1;
      }

#if !GLIB_CHECK_VERSION(2, 35, 0)
    g_type_init();
#endif
    GValue zone = {0};
    g_value_init(&zone, G_TYPE_STRING);

    path = g_build_filename (pixmap_dir, "time_zones_countryInfo-orig.svg", NULL);
    svg = rsvg_handle_new_from_file (path, NULL);
    if (!svg)
      {
        g_message ("File '%s' cannot be read", path);
        g_free (path);
        return 1;
      }
    g_free (path);

    db = tz_load_db ();
    locs = tz_get_locations (db);
    for (i = 0; i < locs->len ; i++)
      {
        CcTimezoneLocation *loc = locs->pdata[i];
        gchar *layer_name;

        TzInfo *info;
        gdouble selected_offset;
        g_object_get_property(G_OBJECT (loc), "zone", &zone);

        info = tz_info_from_location (loc);
        selected_offset = tz_location_get_utc_offset (loc)
            / (60.0*60.0) + ((info->daylight) ? -1.0 : 0.0);

        layer_name = g_strdup_printf("#%c%g",
                                     selected_offset > 0 ? 'p' : 'm',
                                     fabs(selected_offset));

        if (rsvg_handle_has_sub (svg, layer_name) == FALSE)
          {
            g_message ("Layer '%s' missing for zone '%s'", layer_name, g_value_get_string(&zone));
            retval = 1;
          }

        g_free(layer_name);
        tz_info_free (info);
      }
    tz_db_free (db);
    g_free (pixmap_dir);

    return retval;
}
