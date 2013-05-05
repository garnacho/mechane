/* Mechane:
 * Copyright (C) 2013 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <librsvg/rsvg.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "mech-pattern-private.h"

enum {
  TYPE_PATTERN,
  TYPE_ASSET_SVG
};

typedef struct _AssetCacheData AssetCacheData;

struct _AssetCacheData
{
  cairo_pattern_t *pattern;
  gdouble scale_x;
  gdouble scale_y;
};

struct _MechPattern
{
  cairo_pattern_t *pattern;

  union {
    struct {
      RsvgHandle *asset;
      gchar *layer;
      AssetCacheData cache_data;
    } svg;
  };

  gdouble width;
  gdouble height;

  guint type : 2;
  guint width_unit : 3;
  guint height_unit : 3;
  gint ref_count;
};

G_DEFINE_BOXED_TYPE (MechPattern, _mech_pattern,
                     _mech_pattern_ref, _mech_pattern_unref)

MechPattern *
_mech_pattern_new (cairo_pattern_t *pattern)
{
  MechPattern *pat;

  pat = g_new0 (MechPattern, 1);
  pat->pattern = cairo_pattern_reference (pattern);
  pat->type = TYPE_PATTERN;
  pat->ref_count = 1;

  return pat;
}

static cairo_pattern_t *
_mech_pattern_render_asset (MechPattern *pattern,
                            gdouble      scale_x,
                            gdouble      scale_y)
{
  cairo_surface_t *surface = NULL;
  cairo_pattern_t *pat = NULL;
  cairo_t *cr;

  if (pattern->type == TYPE_ASSET_SVG && pattern->svg.asset)
    {
      RsvgDimensionData dimensions;

      rsvg_handle_get_dimensions (pattern->svg.asset, &dimensions);
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            (int) (dimensions.width * scale_x),
                                            (int) (dimensions.height * scale_y));
      cr = cairo_create (surface);
      cairo_scale (cr, scale_x, scale_y);

      if (pattern->svg.layer)
        {
          gchar *hashed;

          hashed = g_strdup_printf ("#%s", pattern->svg.layer);
          rsvg_handle_render_cairo_sub (pattern->svg.asset, cr, hashed);
          g_free (hashed);
        }
      else
        rsvg_handle_render_cairo (pattern->svg.asset, cr);

      cairo_destroy (cr);
    }

  if (surface)
    {
      pat = cairo_pattern_create_for_surface (surface);
      cairo_surface_destroy (surface);
    }

  return pat;
}

static MechPattern *
_mech_pattern_new_svg_asset (GFile           *file,
                             const gchar     *layer,
                             cairo_extend_t   extend,
                             GError         **error)
{
  RsvgDimensionData dimensions;
  MechPattern *pattern;
  RsvgHandle *handle;

  handle = rsvg_handle_new_from_gfile_sync (file, 0, NULL, error);

  if (!handle)
    return NULL;

  pattern = g_new0 (MechPattern, 1);
  pattern->svg.asset = handle;
  pattern->svg.layer = g_strdup (layer);
  pattern->type = TYPE_ASSET_SVG;
  pattern->ref_count = 1;

  pattern->pattern = _mech_pattern_render_asset (pattern, 1, 1);
  cairo_pattern_set_extend (pattern->pattern, extend);

  rsvg_handle_get_dimensions (pattern->svg.asset, &dimensions);
  _mech_pattern_set_size (pattern,
                          dimensions.width, MECH_UNIT_PX,
                          dimensions.height, MECH_UNIT_PX);
  return pattern;
}

MechPattern *
_mech_pattern_new_asset (GFile          *file,
                         const gchar    *layer,
                         cairo_extend_t  extend)
{
  MechPattern *pattern;
  GError *error = NULL;
  gchar *uri;

  uri = g_file_get_uri (file);
  pattern = _mech_pattern_new_svg_asset (file, layer, extend, &error);
  g_free (uri);

  if (error)
    {
      /* FIXME: return "missing image" icon? */
      g_warning ("Could not load asset: %s\n", error->message);
      g_error_free (error);
    }

  return pattern;
}

static cairo_pattern_t *
_mech_pattern_ensure_size_cached (MechPattern *pattern,
                                  gdouble      scale_x,
                                  gdouble      scale_y)
{
  if (pattern->type != TYPE_ASSET_SVG)
    return NULL;

  if (scale_x == 1 && scale_y == 1)
    return pattern->pattern;

  if (!pattern->svg.cache_data.pattern ||
      scale_x != pattern->svg.cache_data.scale_x ||
      scale_y != pattern->svg.cache_data.scale_y)
    {
      if (pattern->svg.cache_data.pattern)
        cairo_pattern_destroy (pattern->svg.cache_data.pattern);

      pattern->svg.cache_data.pattern =
        _mech_pattern_render_asset (pattern, scale_x, scale_y);
      pattern->svg.cache_data.scale_x = scale_x;
      pattern->svg.cache_data.scale_y = scale_y;
    }

  return pattern->svg.cache_data.pattern;
}

void
_mech_pattern_set_source (MechPattern             *pattern,
                          cairo_t                 *cr,
                          const cairo_rectangle_t *rect)
{
  cairo_pattern_t *pat = NULL;
  gdouble scale_x, scale_y;
  cairo_matrix_t matrix;

  cairo_matrix_init_translate (&matrix, -rect->x, -rect->y);

  if (pattern->type == TYPE_ASSET_SVG)
    {
      scale_x = scale_y = 1;
      cairo_user_to_device_distance (cr, &scale_x, &scale_y);

      if (pattern->type == TYPE_ASSET_SVG && pattern->svg.asset)
        {
          pat = _mech_pattern_ensure_size_cached (pattern, scale_x, scale_y);
          cairo_matrix_scale (&matrix, scale_x, scale_y);
        }
    }
  else
    pat = pattern->pattern;

  cairo_pattern_set_matrix (pat, &matrix);
  cairo_set_source (cr, pat);
}

void
_mech_pattern_render (MechPattern             *pattern,
                      cairo_t                 *cr,
                      const cairo_rectangle_t *rect)
{
  _mech_pattern_set_source (pattern, cr, rect);
  cairo_fill (cr);
}

void
_mech_pattern_set_size (MechPattern *pattern,
			gdouble      width,
			MechUnit     width_unit,
			gdouble      height,
			MechUnit     height_unit)
{
  g_return_if_fail (pattern != NULL);

  pattern->width = width;
  pattern->width_unit= width_unit;
  pattern->height = height;
  pattern->height_unit = height_unit;
}

void
_mech_pattern_get_size (MechPattern *pattern,
                        gdouble     *width,
                        MechUnit    *width_unit,
                        gdouble     *height,
                        MechUnit    *height_unit)
{
  *width = pattern->width;
  *width_unit = pattern->width_unit;
  *height = pattern->height;
  *height_unit = pattern->height_unit;
}

MechPattern *
_mech_pattern_ref (MechPattern *pattern)
{
  g_atomic_int_inc (&pattern->ref_count);
  return pattern;
}

static void
_mech_pattern_dispose (MechPattern *pattern)
{
  if (pattern->pattern)
    cairo_pattern_destroy (pattern->pattern);

  if (pattern->type == TYPE_ASSET_SVG)
    {
      g_free (pattern->svg.layer);

      if (pattern->svg.asset)
        g_object_unref (pattern->svg.asset);
      if (pattern->svg.cache_data.pattern)
        cairo_pattern_destroy (pattern->svg.cache_data.pattern);
    }
}

void
_mech_pattern_unref (MechPattern *pattern)
{
  if (g_atomic_int_dec_and_test (&pattern->ref_count))
    {
      _mech_pattern_dispose (pattern);
      g_free (pattern);
    }
}
