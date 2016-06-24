#include "config.h"
#include "tz.h"

#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

/* For each offset png, check that there is at least one clickable location in
 * the tzdb with the offset */
int main(int argc, char **argv)
{
    TzDB *db;
    GPtrArray *locs;
    CcTimezoneLocation *loc;
    TzInfo *info;
    gdouble loc_offset;

    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodeSetPtr nodes;
    
    const char *pixmap_dir;
    gchar *path;

    GError *error = NULL;
    gchar *endptr;
    gdouble timezone_offset;
    int i, j;

    int retval = 0;

    if (argc == 2)
      {
        pixmap_dir = argv[1];
      }
    else if (argc == 1)
      {
        const char *datadir = g_getenv("DATADIR");
        if (datadir != NULL)
          {
            pixmap_dir = datadir;
          }
        else
          {
            pixmap_dir = "./data";
          }
      }
    else
      {
        g_message("Usage: %s [PIXMAP DIRECTORY]", argv[0]);
        return 1;
      }

    if (! g_file_test(pixmap_dir, G_FILE_TEST_IS_DIR))
      {
        g_message("Pixmap directory %s does not exist", pixmap_dir);
        return 1;
      }

#if !GLIB_CHECK_VERSION(2, 35, 0)
    g_type_init();
#endif

    xmlInitParser ();
    path = g_build_filename (pixmap_dir, "time_zones_countryInfo-orig.svg", NULL);
    doc = xmlParseFile (path);
    if (doc == NULL)
      {
        g_message("Unable to parse '%s'", path);
        g_free (path);
        return 1;
      }
    g_free (path);

    /* Iterate over each layer, which can be found as <g> element wit
     * inkscape:groupmode="layer" */
    xpathCtx = xmlXPathNewContext(doc);
    xmlXPathRegisterNs(xpathCtx, "svg", "http://www.w3.org/2000/svg");
    xmlXPathRegisterNs(xpathCtx, "inkscape", "http://www.inkscape.org/namespaces/inkscape");

    xpathObj = xmlXPathEvalExpression("//svg:g[@inkscape:groupmode = 'layer']", xpathCtx);
    if (!xpathObj)
      {
        g_message("Unable to evaluate xpath");
        return 1;
      }

    nodes = xpathObj->nodesetval;
    if (nodes == NULL)
      {
        g_message("Unable to find layers in SVG");
        return 1;
      }

    db = tz_load_db ();
    locs = tz_get_locations (db);

    for (i = 0; i < nodes->nodeNr; i++)
      {
        char *id = xmlGetProp(nodes->nodeTab[i], "id");

        if (id[0] != 'm' && id[0] != 'p')
          {
            continue;
          }

        timezone_offset = g_ascii_strtod (id+1, &endptr);
        if (*endptr != '\0')
          {
            g_message ("Unable to parse layer name %s", id);
            retval = 1;
          }
        if (id[0] == 'm')
          {
            timezone_offset = -timezone_offset;
          }

        /* Look for a location in tzdb with the same offset */
        loc_offset = G_MAXDOUBLE;
        for (j = 0; j < locs->len; j++)
          {
            loc = locs->pdata[j];
            info = tz_info_from_location (loc);
            loc_offset = tz_location_get_utc_offset (loc)
                / (60.0 * 60.0) + ((info->daylight) ? -1.0 : 0.0);
            
            if (loc_offset == timezone_offset)
              {
                break;
              }
          }

        if (loc_offset != timezone_offset)
          {
            g_message ("Unable to find location for offset %.02f", timezone_offset);
            retval = 1;
          }
      }

    tz_db_free (db);
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    return retval;
}
