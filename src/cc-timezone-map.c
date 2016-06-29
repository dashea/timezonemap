/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2011 Canonical Ltd.
 *
 * Portions from Ubiquity, Copyright (C) 2009 Canonical Ltd.
 * Written by Evan Dandrea <ev@ubuntu.com>
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
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-timezone-map.h"
#include "cc-timezone-location.h"
#include <math.h>
#include "tz.h"
#include <librsvg/rsvg.h>
#include <string.h>

G_DEFINE_TYPE (CcTimezoneMap, cc_timezone_map, GTK_TYPE_WIDGET)

#define TIMEZONE_MAP_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_TIMEZONE_MAP, CcTimezoneMapPrivate))


typedef struct
{
  gdouble offset;
  guchar red;
  guchar green;
  guchar blue;
  guchar alpha;
} CcTimezoneMapOffset;

struct _CcTimezoneMapPrivate
{
  RsvgHandle *map_svg;

  /* Cache the rendered map and highlight images as cairo patterns. The
   * patterns only need to be re-rendered when the widget allocation changes
   * and not on every draw. */
  cairo_pattern_t *background;
  cairo_pattern_t *highlight;

  /* Cache the highlights so they only need to be rendered once for a
   * given size. */
  GHashTable *highlight_table;

  gdouble selected_offset;
  gboolean show_offset;

  gchar *watermark;

  TzDB *tzdb;
  CcTimezoneLocation *location;
  GHashTable *alias_db;
  GList *distances;
  /* Store the head of the list separately so it can be freed later */
  GList *distances_head;

  gint previous_x;
  gint previous_y;
};

enum
{
  LOCATION_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SELECTED_OFFSET,
};

static guint signals[LAST_SIGNAL];

/* Allow datadir to be overridden in the environment */
static const gchar *
get_datadir (void)
{
  const gchar *datadir = g_getenv("DATADIR");

  if (datadir)
    return datadir;
  else
    return DATADIR;
}

static void
cc_timezone_map_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP(object);
  switch (property_id)
    {
    case PROP_SELECTED_OFFSET:
      g_value_set_double(value, map->priv->selected_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_timezone_map_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP(object);
  switch (property_id)
    {
    case PROP_SELECTED_OFFSET:
      cc_timezone_map_set_selected_offset(map, g_value_get_double(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_timezone_map_dispose (GObject *object)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (object)->priv;

  if (priv->map_svg)
    {
      g_object_unref (priv->map_svg);
      priv->map_svg = NULL;
    }

  if (priv->background)
    {
      cairo_pattern_destroy (priv->background);
      priv->background = NULL;
    }

  /* The highlight reference is held by the hash table */
  if (priv->highlight)
    {
      priv->highlight = NULL;
    }

  if (priv->highlight_table)
    {
      g_hash_table_destroy (priv->highlight_table);
      priv->highlight_table = NULL;
    }

  if (priv->alias_db)
    {
      g_hash_table_destroy (priv->alias_db);
      priv->alias_db = NULL;
    }

  if (priv->distances_head)
    {
      g_list_free (priv->distances_head);
      priv->distances_head = NULL;
      priv->distances = NULL;
    }

  if (priv->watermark)
    {
      g_free (priv->watermark);
      priv->watermark = NULL;
    }

  G_OBJECT_CLASS (cc_timezone_map_parent_class)->dispose (object);
}

static void
cc_timezone_map_finalize (GObject *object)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (object)->priv;

  if (priv->tzdb)
    {
      tz_db_free (priv->tzdb);
      priv->tzdb = NULL;
    }


  G_OBJECT_CLASS (cc_timezone_map_parent_class)->finalize (object);
}

/* GtkWidget functions */
static void
cc_timezone_map_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  /* choose a minimum size small enough to prevent the window
   * from growing horizontally
   */
  if (minimum != NULL)
    *minimum = 300;
  if (natural != NULL)
    *natural = 300;
}

static void
cc_timezone_map_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  /* Match the 300 width based on a width:height of almost 2:1 */
  if (minimum != NULL)
    *minimum = 173;
  if (natural != NULL)
    *natural = 173;
}

/* Call whenever the allocation or offset changes */
static void
render_highlight (CcTimezoneMap *cc)
{
  CcTimezoneMapPrivate *priv = cc->priv;
  GtkAllocation alloc;
  RsvgDimensionData svg_dimensions;
  gdouble scale_x, scale_y;
  cairo_surface_t *surface;
  cairo_t *cr;
  gchar *layer_name;
  gdouble *hash_key;

  /* The reference to the cairo_pattern_t object is held by highlight_table */

  /* If no highlight is displayed, just unset the pointer */
  if (!priv->show_offset)
    {
      if (priv->highlight)
        {
          priv->highlight = NULL;
        }

      return;
    }

  /* Check if the highlight has already been rendered */
  priv->highlight = g_hash_table_lookup (priv->highlight_table,
                                         &priv->selected_offset);
  if (priv->highlight)
    return;

  layer_name = g_strdup_printf("#%c%g",
                               priv->selected_offset > 0 ? 'p' : 'm',
                               fabs(priv->selected_offset));
  gtk_widget_get_allocation (GTK_WIDGET (cc), &alloc);

  /* Use the size of the background layer as the scale */
  rsvg_handle_get_dimensions_sub (priv->map_svg, &svg_dimensions, "#background");
  scale_x = (double)alloc.width / svg_dimensions.width;
  scale_y = (double)alloc.height / svg_dimensions.height;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        alloc.width,
                                        alloc.height);
  cr = cairo_create (surface);

  cairo_scale (cr, scale_x, scale_y);
  rsvg_handle_render_cairo_sub (priv->map_svg, cr, layer_name);
  g_free(layer_name);
  cairo_surface_flush (surface);

  priv->highlight = cairo_pattern_create_for_surface (surface);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  /* Save the pattern in the hash table */
  hash_key = g_malloc (sizeof (gdouble));
  *hash_key = priv->selected_offset;
  g_hash_table_insert (priv->highlight_table,
                       hash_key,
                       priv->highlight);
}

/* Update the cached map patterns when the widget allocation changes */
static void
cc_timezone_map_size_allocate (GtkWidget *widget,
                               GtkAllocation *allocation)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (widget)->priv;
  RsvgDimensionData svg_dimensions;
  gdouble scale_x, scale_y;
  cairo_surface_t *surface;
  cairo_t *cr;

  GTK_WIDGET_CLASS(cc_timezone_map_parent_class)->size_allocate (widget, allocation);

  /* Figure out the scaling factor between the SVG and the allocation */
  rsvg_handle_get_dimensions_sub (priv->map_svg, &svg_dimensions, "#background");
  scale_x = (double)allocation->width / svg_dimensions.width;
  scale_y = (double)allocation->height / svg_dimensions.height;

  /* Create a new cairo context over an image surface the size of the
   * allocation */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        allocation->width,
                                        allocation->height);
  cr = cairo_create (surface);

  cairo_scale (cr, scale_x, scale_y);
  rsvg_handle_render_cairo_sub (priv->map_svg, cr, "#background");

  cairo_surface_flush (surface);

  if (priv->background)
    cairo_pattern_destroy (priv->background);

  priv->background = cairo_pattern_create_for_surface (surface);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  /* Invalidate the highlight cache and render the current one */
  g_hash_table_remove_all (priv->highlight_table);
  render_highlight (CC_TIMEZONE_MAP(widget));
}

static void
cc_timezone_map_realize (GtkWidget *widget)
{
  GdkWindowAttr attr = { 0, };
  GtkAllocation allocation;
  GdkCursor *cursor;
  GdkWindow *window;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attr.window_type = GDK_WINDOW_CHILD;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.width = allocation.width;
  attr.height = allocation.height;
  attr.x = allocation.x;
  attr.y = allocation.y;
  attr.event_mask = gtk_widget_get_events (widget)
                                 | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

  window = gdk_window_new (gtk_widget_get_parent_window (widget), &attr,
                           GDK_WA_X | GDK_WA_Y);

  gdk_window_set_user_data (window, widget);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  cursor = gdk_cursor_new (GDK_HAND2);
G_GNUC_END_IGNORE_DEPRECATIONS
  gdk_window_set_cursor (window, cursor);

  gtk_widget_set_window (widget, window);
}


static gdouble
convert_longtitude_to_x (gdouble longitude, gint map_width)
{
  const gdouble xdeg_offset = -6;
  gdouble x;

  x = (map_width * (180.0 + longitude) / 360.0)
    + (map_width * xdeg_offset / 180.0);

  /* If x is negative, wrap back to the beginning */
  if (x < 0)
      x = (gdouble) map_width + x;

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


static gboolean
cc_timezone_map_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (widget)->priv;
  gchar *file;
  GdkPixbuf *pin;
  GtkAllocation alloc;
  GError *err = NULL;
  gdouble pointx, pointy;
  gdouble alpha = 1.0;
  GtkStyle *style;

  gtk_widget_get_allocation (widget, &alloc);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  style = gtk_widget_get_style (widget);
G_GNUC_END_IGNORE_DEPRECATIONS

  /* Check if insensitive */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE)
    alpha = 0.5;
G_GNUC_END_IGNORE_DEPRECATIONS

  /* paint background */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_cairo_set_source_color (cr, &style->bg[gtk_widget_get_state (widget)]);
G_GNUC_END_IGNORE_DEPRECATIONS
  cairo_paint (cr);
  cairo_set_source (cr, priv->background);
  cairo_paint_with_alpha (cr, alpha);

  /* paint watermark */
  if (priv->watermark) {
    cairo_text_extents_t extent;
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
    cairo_text_extents(cr, priv->watermark, &extent);
    cairo_move_to(cr, alloc.width - extent.x_advance + extent.x_bearing - 5,
                      alloc.height - extent.height - extent.y_bearing - 5);
    cairo_show_text(cr, priv->watermark);
    cairo_stroke(cr);
  }

  if (priv->highlight)
    {
      cairo_set_source (cr, priv->highlight);
      cairo_paint_with_alpha (cr, alpha);
    }

  if (!priv->location) {
    return TRUE;
  }

  /* load pin icon */
  file = g_strdup_printf ("%s/pin.png", get_datadir ());
  pin = gdk_pixbuf_new_from_file (file, &err);
  g_free (file);

  if (err)
    {
      g_warning ("Could not load pin icon: %s", err->message);
      g_clear_error (&err);
    }

  pointx = convert_longtitude_to_x (
          cc_timezone_location_get_longitude(priv->location), alloc.width);
  pointy = convert_latitude_to_y (
          cc_timezone_location_get_latitude(priv->location), alloc.height);

  if (pointy > alloc.height)
    pointy = alloc.height;

  if (pin)
    {
      gdk_cairo_set_source_pixbuf (cr, pin, pointx - 8, pointy - 14);
      cairo_paint_with_alpha (cr, alpha);
      g_object_unref (pin);
    }

  return TRUE;
}

static void
cc_timezone_map_class_init (CcTimezoneMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcTimezoneMapPrivate));

  object_class->get_property = cc_timezone_map_get_property;
  object_class->set_property = cc_timezone_map_set_property;
  object_class->dispose = cc_timezone_map_dispose;
  object_class->finalize = cc_timezone_map_finalize;

  widget_class->get_preferred_width = cc_timezone_map_get_preferred_width;
  widget_class->get_preferred_height = cc_timezone_map_get_preferred_height;
  widget_class->size_allocate = cc_timezone_map_size_allocate;
  widget_class->realize = cc_timezone_map_realize;
  widget_class->draw = cc_timezone_map_draw;

  g_object_class_install_property(G_OBJECT_CLASS(klass),
                                  PROP_SELECTED_OFFSET,
                                  g_param_spec_string ("selected-offset",
                                      "Selected offset",
                                      "The selected offset from GMT in hours",
                                      "",
                                      G_PARAM_READWRITE));

  signals[LOCATION_CHANGED] = g_signal_new ("location-changed",
                                            CC_TYPE_TIMEZONE_MAP,
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__OBJECT,
                                            G_TYPE_NONE, 1,
                                            CC_TYPE_TIMEZONE_LOCATION);
}


static gint
sort_locations (CcTimezoneLocation *a,
                CcTimezoneLocation *b)
{
  gdouble dist_a, dist_b;
  dist_a = cc_timezone_location_get_dist(a);
  dist_b = cc_timezone_location_get_dist(b);
  if (dist_a > dist_b)
    return 1;

  if (dist_a < dist_b)
    return -1;

  return 0;
}

/* Return the UTC offset (in hours) for the standard (winter) time at a location */
static gdouble
get_location_offset (CcTimezoneLocation *location)
{
  const gchar *zone_name;
  GTimeZone *zone;
  gint interval;
  gint64 curtime;
  gint32 offset;

  g_return_val_if_fail (location != NULL, 0);
  zone_name = cc_timezone_location_get_zone (location);
  g_return_val_if_fail (zone_name != NULL, 0);

  zone = g_time_zone_new (zone_name);

  /* Query the zone based on the current time, since otherwise the data
   * may not make sense. */
  curtime = g_get_real_time () / 1000000;    /* convert to seconds */
  interval = g_time_zone_find_interval (zone, G_TIME_TYPE_UNIVERSAL, curtime);

  offset = g_time_zone_get_offset (zone, interval);
  if (g_time_zone_is_dst (zone, interval))
    {
      /* Subtract an hour's worth of seconds to get the standard time offset */
      offset -= (60 * 60);
    }

  g_time_zone_unref (zone);

  return offset / (60.0 * 60.0);
}

static void
set_location (CcTimezoneMap *map,
              CcTimezoneLocation    *location)
{
  CcTimezoneMapPrivate *priv = map->priv;

  priv->location = location;

  if (priv->location)
  {
    priv->selected_offset = get_location_offset (priv->location);
    priv->show_offset = TRUE;
  }
  else
  {
    priv->show_offset = FALSE;
    priv->selected_offset = 0.0;
  }

  render_highlight (map);
  gtk_widget_queue_draw (GTK_WIDGET (map));

  g_signal_emit (map, signals[LOCATION_CHANGED], 0, priv->location);

}

static CcTimezoneLocation *
get_loc_for_xy (GtkWidget * widget, gint x, gint y)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (widget)->priv;
  gint i;

  const GPtrArray *array;
  gint width, height;
  GtkAllocation alloc;
  CcTimezoneLocation* location;

  if ((x - priv->previous_x < 5 && x - priv->previous_x > -5) &&
      (y - priv->previous_y < 5 && y - priv->previous_y > -5)) {
    x = priv->previous_x;
    y = priv->previous_y;
  }

  gtk_widget_queue_draw (widget);

  /* work out the co-ordinates */

  array = tz_get_locations (priv->tzdb);

  gtk_widget_get_allocation (widget, &alloc);
  width = alloc.width;
  height = alloc.height;

  if (x == priv->previous_x && y == priv->previous_y) 
    {
      priv->distances = g_list_next (priv->distances);
      if (!priv->distances)
          priv->distances = priv->distances_head;

      location = (CcTimezoneLocation*) priv->distances->data;
    } else {
      GList *node;

      g_list_free (priv->distances_head);
      priv->distances_head = NULL;
      for (i = 0; i < array->len; i++)
        {
          gdouble pointx, pointy, dx, dy;
          CcTimezoneLocation *loc = array->pdata[i];

          pointx = convert_longtitude_to_x (cc_timezone_location_get_longitude(loc), width);
          pointy = convert_latitude_to_y (cc_timezone_location_get_latitude(loc), height);

          dx = pointx - x;
          dy = pointy - y;

          cc_timezone_location_set_dist(loc, (gdouble) dx * dx + dy * dy);
          priv->distances_head = g_list_prepend (priv->distances_head, loc);
        }
      priv->distances_head = g_list_sort (priv->distances_head, (GCompareFunc) sort_locations);

      /* Remove locations from the list with a distance of greater than 50px,
       * so that repeated clicks cycle through a smaller area instead of
       * jumping all over the map. Start with the second element to ensure
       * that at least one element stays in the list.
       */
      node = priv->distances_head->next;
      while (node != NULL)
        {
          if (cc_timezone_location_get_dist(node->data) > (50 * 50))
            {
              /* Cut the list off here */
              node->prev->next = NULL;
              g_list_free(node);
              break;
            }

          node = g_list_next(node);
        }

      priv->distances = priv->distances_head;
      location = (CcTimezoneLocation*) priv->distances->data;
      priv->previous_x = x;
      priv->previous_y = y;
    }

    return location;
}

static gboolean
button_press_event (GtkWidget      *widget,
                    GdkEventButton *event)
{
  CcTimezoneLocation * loc = get_loc_for_xy (widget, event->x, event->y);
  set_location (CC_TIMEZONE_MAP (widget), loc);
  return TRUE;
}

static void
state_flags_changed (GtkWidget *widget)
{
  // To catch sensitivity changes
  gtk_widget_queue_draw (widget);
}

static void
load_backward_tz (CcTimezoneMap *self)
{
  GError *error = NULL;
  char **lines, *contents;
  gchar *file;
  gboolean result;
  guint i;

  self->priv->alias_db = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  file = g_strdup_printf ("%s/backward", get_datadir ());
  result = g_file_get_contents (file, &contents, NULL, &error);
  g_free (file);
  if (result == FALSE)
    {
      g_warning ("Failed to load 'backward' file: %s", error->message);
      return;
    }
  lines = g_strsplit (contents, "\n", -1);
  g_free (contents);
  for (i = 0; lines[i] != NULL; i++)
    {
      char **items;
      guint j;
      char *real, *alias;

      if (g_ascii_strncasecmp (lines[i], "Link\t", 5) != 0)
        continue;

      items = g_strsplit (lines[i], "\t", -1);
      real = NULL;
      alias = NULL;
      /* Skip the "Link<tab>" part */
      for (j = 1; items[j] != NULL; j++)
        {
          if (items[j][0] == '\0')
            continue;
          if (real == NULL)
            {
              real = items[j];
              continue;
            }
          alias = items[j];
          break;
        }

      if (real == NULL || alias == NULL)
        g_warning ("Could not parse line: %s", lines[i]);

      g_hash_table_insert (self->priv->alias_db, g_strdup (alias), g_strdup (real));
      g_strfreev (items);
    }
  g_strfreev (lines);
}

static void
cc_timezone_map_init (CcTimezoneMap *self)
{
  CcTimezoneMapPrivate *priv;
  GError *err = NULL;
  gchar *file;

  priv = self->priv = TIMEZONE_MAP_PRIVATE (self);

  priv->previous_x = -1;
  priv->previous_y = -1;

  file = g_strdup_printf ("%s/time_zones_countryInfo-orig.svg", get_datadir ());
  priv->map_svg = rsvg_handle_new_from_file (file, &err);
  g_free (file);

  if (!priv->map_svg)
    {
      g_warning("Could not load map data: %s",
                (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  priv->highlight_table = g_hash_table_new_full (g_double_hash,
                                                 g_double_equal,
                                                 g_free,
                                                 (GDestroyNotify) cairo_pattern_destroy);

  priv->selected_offset = 0.0;
  priv->show_offset = FALSE;

  priv->tzdb = tz_load_db ();

  g_signal_connect (self, "button-press-event", G_CALLBACK (button_press_event),
                    NULL);
  g_signal_connect (self, "state-flags-changed", G_CALLBACK (state_flags_changed),
                    NULL);

  load_backward_tz (self);
}

CcTimezoneMap *
cc_timezone_map_new (void)
{
  return g_object_new (CC_TYPE_TIMEZONE_MAP, NULL);
}

void
cc_timezone_map_set_timezone (CcTimezoneMap *map,
                              const gchar   *timezone)
{
  GPtrArray *locations;
  GList *zone_locations = NULL;
  GList *location_node = NULL;
  guint i;
  const char *real_tz;
  const char *tz_city_start;
  char *tz_city;
  char *tmp;
  gboolean found_location = FALSE;

  real_tz = g_hash_table_lookup (map->priv->alias_db, timezone);
  if (!real_tz)
      real_tz = timezone;

  tz_city_start = strrchr (timezone, '/');
  if (tz_city_start)
    {
      /* Move to the first character after the / */
      tz_city_start++;
    }
  else
    {
      tz_city_start = real_tz;
    }

  tz_city = g_strdup(tz_city_start);

  /* Replace the underscores with spaces */
  tmp = tz_city;
  while (*tmp != '\0')
    {
      if (*tmp == '_')
        {
          *tmp = ' ';
        }
      tmp++;
    }

  locations = tz_get_locations (map->priv->tzdb);

  for (i = 0; i < locations->len; i++)
    {
      CcTimezoneLocation *loc = locations->pdata[i];

      if (!g_strcmp0 (cc_timezone_location_get_zone (loc), real_tz))
        {
          zone_locations = g_list_prepend (zone_locations, loc);
        }
    }

  /* No location found. Use GLib to set the highlight. g_time_zone_new always
   * returns a GTimeZone, so invalid zones will just be offset 0.
   */
  if (zone_locations == NULL)
    {
      CcTimezoneLocation *test_location = cc_timezone_location_new ();
      gdouble offset;

      cc_timezone_location_set_zone (test_location, real_tz);
      offset = get_location_offset (test_location);
      g_object_unref (test_location);

      set_location (map, NULL);
      cc_timezone_map_set_selected_offset (map, offset);

      return;
    }

  /* Look for a location with a name that either starts or ends with the name
   * of the zone
   */
  location_node = zone_locations;
  while (location_node != NULL)
    {
      const gchar *name = cc_timezone_location_get_en_name (location_node->data);
      if (!strncmp (name, tz_city, strlen (tz_city)) ||
          ((strlen(name) > strlen(tz_city)) &&
           !strncmp (name + (strlen(name) - strlen(tz_city)), tz_city, strlen (tz_city))))
        {
          set_location (map, location_node->data);
          found_location = TRUE;
          break;
        }

      location_node = location_node->next;
    }

  /* If that didn't work, try by state */
  if (!found_location)
    {
      location_node = zone_locations;
      while (location_node != NULL)
        {
          const gchar *state = cc_timezone_location_get_state (location_node->data);

          if ((state != NULL) &&
                  !strncmp (state, tz_city, strlen (tz_city)))
            {
              set_location (map, location_node->data);
              found_location = TRUE;
              break;
            }

          location_node = location_node->next;
        }
    }

  /* If nothing matched, just use the first location */
  if (!found_location)
    {
      set_location (map, zone_locations->data);
    }

  g_list_free (zone_locations);
}

void
cc_timezone_map_set_location (CcTimezoneMap *map,
			      gdouble lon,
			      gdouble lat)
{
  GtkAllocation alloc;
  gtk_widget_get_allocation (GTK_WIDGET(map), &alloc);
  gdouble x = convert_longtitude_to_x(lon, alloc.width);
  gdouble y = convert_latitude_to_y(lat, alloc.height);
  CcTimezoneLocation * loc = get_loc_for_xy (GTK_WIDGET(map), x, y);
  set_location (map, loc);
}

void
cc_timezone_map_set_coords (CcTimezoneMap *map, gdouble lon, gdouble lat)
{
  const gchar * zone = cc_timezone_map_get_timezone_at_coords (map, lon, lat);
  cc_timezone_map_set_timezone (map, zone);
}

const gchar *
cc_timezone_map_get_timezone_at_coords (CcTimezoneMap *map, gdouble lon, gdouble lat)
{
  CcTimezoneMapPrivate *priv = map->priv;
  gdouble min_dist = G_MAXDOUBLE;
  CcTimezoneLocation *min_loc = NULL;
  int i;
  const GPtrArray *array;

  array = tz_get_locations(priv->tzdb);

  /* Find the location closest to the specified lat/lng */
  for (i = 0; i < array->len; i++)
    {
      gdouble dlat, dlon, dist;
      CcTimezoneLocation *loc = array->pdata[i];

      dlat = lat - cc_timezone_location_get_latitude(loc);
      dlon = lon - cc_timezone_location_get_longitude(loc);
      dist = (dlat * dlat) + (dlon * dlon);
      if (dist < min_dist)
        {
          min_dist = dist;
          min_loc = loc;
        }
    }

  return cc_timezone_location_get_zone(min_loc);
}

void
cc_timezone_map_set_watermark (CcTimezoneMap *map, const gchar * watermark)
{
  if (map->priv->watermark)
    g_free (map->priv->watermark);

  map->priv->watermark = g_strdup (watermark);
  gtk_widget_queue_draw (GTK_WIDGET (map));
}

/**
 * cc_timezone_map_get_location:
 * @map: A #CcTimezoneMap
 *
 * Returns the current location set for the map.
 *
 * Returns: (transfer none): the map location.
 */
CcTimezoneLocation *
cc_timezone_map_get_location (CcTimezoneMap *map)
{
  return map->priv->location;
}

/**
 * cc_timezone_map_clear_location:
 * @map: A #CcTimezoneMap
 *
 * Clear the location currently set for the #CcTimezoneMap. This will remove
 * the highlight and reset the map to its original state.
 *
 */
void
cc_timezone_map_clear_location (CcTimezoneMap *map)
{
  set_location(map, NULL);
}

/**
 * cc_timezone_map_get_selected_offset:
 * @map: A #CcTimezoneMap
 *
 * Returns the currently selected offset in hours from GMT.
 *
 * Returns: The selected offset.
 *
 */
gdouble cc_timezone_map_get_selected_offset(CcTimezoneMap *map)
{
  return map->priv->selected_offset;
}

/**
 * cc_timezone_map_set_selected_offset:
 * @map: A #CcTimezoneMap
 * @offset: The offset from GMT in hours
 *
 * Set the currently selected offset for the map and redraw the highlighted
 * time zone.
 */
void cc_timezone_map_set_selected_offset (CcTimezoneMap *map, gdouble offset)
{
  map->priv->selected_offset = offset;
  map->priv->show_offset = TRUE;
  g_object_notify(G_OBJECT(map), "selected-offset");
  render_highlight (map);
  gtk_widget_queue_draw (GTK_WIDGET (map));
}
