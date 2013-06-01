/* Mechane:
 * Copyright (C) 2012 Carlos Garnacho <carlosg@gnome.org>
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

#include <math.h>
#include "mech-backend-private.h"
#include "mech-surface-private.h"
#include "mech-area-private.h"

#define ALIGN_RECT(s,d) G_STMT_START                                    \
  {                                                                     \
    (d)->x = floor ((s)->x);                                            \
    (d)->y = floor ((s)->y);                                            \
    (d)->width = ceil ((s)->width + ((s)->x - (gdouble) (d)->x));       \
    (d)->height = ceil ((s)->height + ((s)->y - (gdouble) (d)->y));     \
  }                                                                     \
  G_STMT_END

#define EXTRA_PIXELS 400

typedef struct _MechSurfacePrivate MechSurfacePrivate;

enum {
  PROP_AREA = 1,
  PROP_NEED_FULL_REFRESH
};

struct _MechSurfacePrivate
{
  MechArea *area;

  /* area coordinates */
  cairo_rectangle_t cached_rect;
  cairo_rectangle_t viewport_rect;
  GArray *damaged;

  /* Pixel sizes */
  gint width;
  gint height;
  gint cache_width;
  gint cache_height;

  gdouble scale_x;
  gdouble scale_y;

  guint need_full_refresh : 1;
  guint initialized       : 1;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechSurface, mech_surface, G_TYPE_OBJECT)

static void
mech_surface_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private ((MechSurface *) object);

  switch (prop_id)
    {
    case PROP_AREA:
      g_value_set_object (value, priv->area);
      break;
    case PROP_NEED_FULL_REFRESH:
      g_value_set_boolean (value, priv->need_full_refresh);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_surface_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private ((MechSurface *) object);

  switch (prop_id)
    {
    case PROP_AREA:
      priv->area = g_value_get_object (value);
      break;
    case PROP_NEED_FULL_REFRESH:
      priv->need_full_refresh = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_surface_finalize (GObject *object)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private ((MechSurface *) object);

  if (priv->damaged)
    g_array_unref (priv->damaged);

  G_OBJECT_CLASS (mech_surface_parent_class)->finalize (object);
}

static void
mech_surface_class_init (MechSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mech_surface_get_property;
  object_class->set_property = mech_surface_set_property;
  object_class->finalize = mech_surface_finalize;

  g_object_class_install_property (object_class,
                                   PROP_AREA,
                                   g_param_spec_object ("area",
                                                        "area",
                                                        "area",
                                                        MECH_TYPE_AREA,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_NEED_FULL_REFRESH,
                                   g_param_spec_boolean ("need-full-refresh",
                                                         "need-full-refresh",
                                                         "need-full-refresh",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS |
                                                         G_PARAM_CONSTRUCT_ONLY));
}

static void
mech_surface_init (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  priv->scale_x = 1;
  priv->scale_y = 1;
}

static gdouble
_mech_surface_get_axis_scale (cairo_matrix_t *matrix,
                              MechAxis        axis)
{
  gdouble distance_x, distance_y;

  distance_x = (axis == MECH_AXIS_X) ? 1 : 0;
  distance_y = (axis == MECH_AXIS_Y) ? 1 : 0;
  cairo_matrix_transform_distance (matrix, &distance_x, &distance_y);

  return sqrt ((distance_x * distance_x) + (distance_y * distance_y));
}

static gboolean
_mech_surface_update_scale (MechSurface *surface)
{
  MechSurfacePrivate *priv;
  gdouble scale_x, scale_y;
  cairo_matrix_t matrix;

  priv = mech_surface_get_instance_private (surface);
  mech_area_get_relative_matrix (priv->area, NULL, &matrix);
  scale_x = _mech_surface_get_axis_scale (&matrix, MECH_AXIS_X);
  scale_y = _mech_surface_get_axis_scale (&matrix, MECH_AXIS_Y);

  if (scale_x != priv->scale_x ||
      scale_y != priv->scale_y)
    {
      priv->scale_x = scale_x;
      priv->scale_y = scale_y;
      return TRUE;
    }

  return FALSE;
}

static void
_calculate_cache_size (MechSurface *surface,
                       gint        *width,
                       gint        *height)
{
  cairo_rectangle_t allocation;
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  mech_area_get_allocated_size (priv->area, &allocation);
  allocation.x = allocation.y = 0;

  if (!_mech_area_get_node (priv->area)->parent)
    {
      if (width)
        *width = CLAMP (priv->width + (2 * EXTRA_PIXELS),
                        1, allocation.width * priv->scale_x);
      if (height)
        *height = CLAMP (priv->height + (2 * EXTRA_PIXELS),
                         1, allocation.height * priv->scale_y);
    }
  else
    {
      if (width)
        *width = MAX (1, priv->width + (2 * EXTRA_PIXELS));
      if (height)
        *height = MAX (1, priv->height + (2 * EXTRA_PIXELS));
    }
}

static gboolean
_mech_surface_update_backing_size (MechSurface *surface)
{
  gint new_cache_width, new_cache_height;
  cairo_rectangle_t allocation;
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  mech_area_get_allocated_size (priv->area, &allocation);
  allocation.x = allocation.y = 0;

  _calculate_cache_size (surface, &new_cache_width, &new_cache_height);

  if (priv->cache_width != new_cache_width ||
      priv->cache_height != new_cache_height)
    {
      MECH_SURFACE_GET_CLASS (surface)->set_size (surface, new_cache_width,
                                                  new_cache_height);
      priv->cache_width = new_cache_width;
      priv->cache_height = new_cache_height;
      return TRUE;
    }

  return FALSE;
}

static void
_mech_surface_self_copy (MechSurface *surface,
                         gdouble      dx,
                         gdouble      dy)
{
  cairo_surface_t *cairo_surface;
  cairo_rectangle_t clip_rect;
  gint device_dx, device_dy;
  MechSurfacePrivate *priv;
  cairo_t *cr;

  priv = mech_surface_get_instance_private (surface);
  device_dx = dx * priv->scale_x;
  device_dy = dy * priv->scale_y;

  clip_rect.x = MAX (device_dx, 0);
  clip_rect.y = MAX (device_dy, 0);
  clip_rect.width = priv->cache_width - ABS (device_dx);
  clip_rect.height = priv->cache_height - ABS (device_dy);

  cairo_surface = MECH_SURFACE_GET_CLASS (surface)->get_surface (surface);
  cr = cairo_create (cairo_surface);

  cairo_rectangle (cr, clip_rect.x, clip_rect.y,
                   clip_rect.width, clip_rect.height);
  cairo_clip (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

  cairo_push_group (cr);
  cairo_set_source_surface (cr, cairo_surface, device_dx, device_dy);
  cairo_paint (cr);

  cairo_pop_group_to_source (cr);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static gboolean
_mech_surface_update_surface (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);

  if (!_mech_surface_update_backing_size (surface))
    return FALSE;

  _mech_surface_damage (surface, &priv->cached_rect);
  return TRUE;
}

static gboolean
_invalidate_newly_visible_portions (MechSurface       *surface,
                                    cairo_rectangle_t *old,
                                    cairo_rectangle_t *new)
{
  gdouble x1, x2, y1, y2, old_x1, old_x2, old_y1, old_y2;
  cairo_rectangle_t rect;

  x1 = new->x;
  x2 = new->x + new->width;
  y1 = new->y;
  y2 = new->y + new->height;

  old_x1 = old->x;
  old_x2 = old->x + old->width;
  old_y1 = old->y;
  old_y2 = old->y + old->height;

  /* Check whether the old rectangle equals, or fully covers, the new one */
  if (old_x1 <= x1 && old_x2 >= x2 && old_y1 <= y1 && old_y2 >= y2)
    return FALSE;

  if (old_x1 > x2 || old_x2 < x1 || old_y1 > y2 || old_y2 < y1)
    {
      /* Rectangles don't intersect, invalidate the whole new area */
      _mech_surface_damage (surface, new);
    }
  else
    {
      /* Rectangles on the X axis */
      rect.y = y1;
      rect.height = y2 - y1;

      if (old_x1 > x1 && old_x1 < x2)
        {
          rect.x = x1;
          rect.width = old_x1 - x1;
          _mech_surface_damage (surface, &rect);
        }

      if (old_x2 > x1 && old_x2 < x2)
        {
          rect.x = old_x2;
          rect.width = x2 - old_x2;
          _mech_surface_damage (surface, &rect);
        }

      /* Rectangles on the Y axis */
      rect.x = x1;
      rect.width = x2 - x1;

      if (old_y1 > y1 && old_y1 < y2)
        {
          rect.y = y1;
          rect.height = old_y1 - y1;
          _mech_surface_damage (surface, &rect);
        }

      if (old_y2 > y1 && old_y2 < y2)
        {
          rect.y = old_y2;
          rect.height = y2 - old_y2;
          _mech_surface_damage (surface, &rect);
        }
    }

  return TRUE;
}

static void
_mech_surface_update_cached_rect (MechSurface       *surface,
                                  cairo_rectangle_t *viewport,
                                  gboolean           invalidate,
                                  gdouble           *dx,
                                  gdouble           *dy)
{
  cairo_rectangle_t new_surface_rect, allocation;
  gint cache_width, cache_height;
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  mech_area_get_allocated_size (priv->area, &allocation);
  _calculate_cache_size (surface, &cache_width, &cache_height);

  if (dx)
    *dx = 0;
  if (dy)
    *dy = 0;

  if (!_mech_area_get_node (priv->area)->parent)
    {
      new_surface_rect.width =
        MIN ((gdouble) cache_width / priv->scale_x, allocation.width);
      new_surface_rect.height =
        MIN ((gdouble) cache_height / priv->scale_y, allocation.height);
      new_surface_rect.x =
        CLAMP (viewport->x - (EXTRA_PIXELS / priv->scale_x),
               0, MAX (allocation.width - new_surface_rect.width, 0));
      new_surface_rect.y =
        CLAMP (viewport->y - (EXTRA_PIXELS / priv->scale_y),
               0, MAX (allocation.height - new_surface_rect.height, 0));
    }
  else
    {
      new_surface_rect.width = (gdouble) cache_width / priv->scale_x;
      new_surface_rect.height = (gdouble) cache_height / priv->scale_y;
      new_surface_rect.x = viewport->x - (EXTRA_PIXELS / priv->scale_x);
      new_surface_rect.y = viewport->y - (EXTRA_PIXELS / priv->scale_y);
    }

  if (new_surface_rect.x == priv->cached_rect.x &&
      new_surface_rect.y == priv->cached_rect.y &&
      new_surface_rect.width == priv->cached_rect.width &&
      new_surface_rect.height == priv->cached_rect.height)
    return;

  if (invalidate)
    _invalidate_newly_visible_portions (surface, &priv->cached_rect,
                                        &new_surface_rect);
  if (dx)
    *dx = priv->cached_rect.x - new_surface_rect.x;
  if (dy)
    *dy = priv->cached_rect.y - new_surface_rect.y;

  priv->cached_rect = new_surface_rect;
  priv->initialized = TRUE;
}

void
_mech_surface_update_viewport (MechSurface *surface)
{
  cairo_rectangle_t rect, allocation;
  gdouble x1, y1, x2, y2, dx, dy;
  gboolean needs_surface_update;
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  _mech_area_get_visible_rect (priv->area, &rect);
  mech_area_get_allocated_size (priv->area, &allocation);

  if (!_mech_area_get_node (priv->area)->parent)
    {
      x1 = CLAMP (rect.x, 0, allocation.width);
      y1 = CLAMP (rect.y, 0, allocation.height);
      x2 = CLAMP (rect.x + rect.width, 0, allocation.width);
      y2 = CLAMP (rect.y + rect.height, 0, allocation.height);
    }
  else
    {
      x1 = rect.x;
      y1 = rect.y;
      x2 = rect.x + rect.width;
      y2 = rect.y + rect.height;
    }

  needs_surface_update =
    (x1 < priv->cached_rect.x ||
     x2 > priv->cached_rect.x + priv->cached_rect.width ||
     y1 < priv->cached_rect.y ||
     y2 > priv->cached_rect.y + priv->cached_rect.height);
  priv->viewport_rect = rect;

  if (!priv->initialized ||
      priv->need_full_refresh ||
      _mech_surface_update_scale (surface))
    {
      _mech_surface_update_cached_rect (surface, &rect, FALSE, NULL, NULL);
      _mech_surface_update_surface (surface);
      _mech_surface_damage (surface, &priv->cached_rect);
    }
  else if (needs_surface_update ||
           !_mech_area_get_node (priv->area)->parent)
    {
      _mech_surface_update_cached_rect (surface, &rect, TRUE, &dx, &dy);
      _mech_surface_update_surface (surface);

      if (needs_surface_update && (dx != 0 || dy != 0))
        _mech_surface_self_copy (surface, dx, dy);
    }
}

void
_mech_surface_set_size (MechSurface *surface,
                        gint         width,
                        gint         height)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  priv->width = width;
  priv->height = height;
}

void
_mech_surface_get_size (MechSurface *surface,
                        gint        *width,
                        gint        *height)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);

  if (width)
    *width = priv->width;
  if (height)
    *height = priv->height;
}

cairo_t *
_mech_surface_cairo_create (MechSurface *surface)
{
  cairo_surface_t *cairo_surface;
  MechSurfacePrivate *priv;
  cairo_t *cr;

  priv = mech_surface_get_instance_private (surface);
  _mech_surface_update_viewport (surface);

  cairo_surface = MECH_SURFACE_GET_CLASS (surface)->get_surface (surface);
  g_assert (cairo_surface != NULL);

  cr = cairo_create (cairo_surface);
  cairo_scale (cr, priv->scale_x, priv->scale_y);
  cairo_translate (cr, -priv->cached_rect.x, -priv->cached_rect.y);

  return cr;
}

void
_mech_surface_set_source (cairo_t     *cr,
                          MechSurface *surface)
{
  cairo_surface_t *cairo_surface;
  cairo_pattern_t *pattern;
  MechSurfacePrivate *priv;
  cairo_matrix_t matrix;

  priv = mech_surface_get_instance_private (surface);
  cairo_surface = MECH_SURFACE_GET_CLASS (surface)->get_surface (surface);
  g_assert (cairo_surface != NULL);

  cairo_set_source_surface (cr, cairo_surface, 0, 0);
  pattern = cairo_get_source (cr);
  cairo_pattern_get_matrix (pattern, &matrix);

  cairo_matrix_scale (&matrix, priv->scale_x, priv->scale_y);
  cairo_matrix_translate (&matrix, -priv->cached_rect.x,
                          -priv->cached_rect.y);
  cairo_pattern_set_matrix (pattern, &matrix);
}

MechSurface *
_mech_surface_new (MechArea *area)
{
  MechSurface *surface;
  MechBackend *backend;

  backend = mech_backend_get ();
  surface = mech_backend_create_surface (backend);
  g_object_set (surface, "area", area, NULL);

  return surface;
}

gboolean
_mech_surface_acquire (MechSurface *surface)
{
  MechSurfaceClass *surface_class;

  surface_class = MECH_SURFACE_GET_CLASS (surface);

  if (!surface_class->acquire)
    return TRUE;

  return surface_class->acquire (surface);
}

void
_mech_surface_release (MechSurface *surface)
{
  MechSurfaceClass *surface_class;

  surface_class = MECH_SURFACE_GET_CLASS (surface);

  if (surface_class->release)
    surface_class->release (surface);
}

void
_mech_surface_damage (MechSurface       *surface,
                      cairo_rectangle_t *rect)
{
  MechSurfacePrivate *priv;
  cairo_rectangle_t damage;

  priv = mech_surface_get_instance_private (surface);

  if (rect)
    damage = *rect;
  else
    damage = priv->cached_rect;

  if (!priv->damaged)
    priv->damaged = g_array_new (FALSE, FALSE, sizeof (cairo_rectangle_t));

  g_array_append_val (priv->damaged, damage);
}

cairo_region_t *
_mech_surface_apply_clip (MechSurface *surface,
                          cairo_t     *cr)
{
  cairo_rectangle_int_t check;
  MechSurfacePrivate *priv;
  cairo_region_t *region;
  gint i;

  priv = mech_surface_get_instance_private (surface);
  region = cairo_region_create ();

  if (!priv->damaged || priv->damaged->len == 0)
    return region;

  for (i = 0; i < priv->damaged->len; i++)
    {
      cairo_rectangle_t *rect, pixels;

      rect = &g_array_index (priv->damaged, cairo_rectangle_t, i);
      cairo_user_to_device (cr, &rect->x, &rect->y);
      cairo_user_to_device_distance (cr, &rect->width, &rect->height);

      ALIGN_RECT (rect, &pixels);

      cairo_device_to_user (cr, &pixels.x, &pixels.y);
      cairo_device_to_user_distance (cr, &pixels.width, &pixels.height);

      cairo_rectangle (cr, pixels.x, pixels.y, pixels.width, pixels.height);

      ALIGN_RECT (&pixels, &check);
      cairo_region_union_rectangle (region, &check);
    }

  cairo_clip (cr);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);
  cairo_restore (cr);

  ALIGN_RECT (&priv->cached_rect, &check);
  cairo_region_intersect_rectangle (region, &check);

  g_array_unref (priv->damaged);
  priv->damaged = NULL;

  return region;
}

gboolean
_mech_surface_area_is_rendered (MechSurface       *surface,
                                MechArea          *area,
                                cairo_rectangle_t *rect)
{
  cairo_rectangle_int_t shape_rect;
  cairo_rectangle_t alloc, parent;
  gdouble x1, y1, x2, y2, dx, dy;
  MechSurfacePrivate *priv;
  cairo_region_t *region;
  GNode *parent_node;

  priv = mech_surface_get_instance_private (surface);

  if (area == priv->area)
    {
      if (rect)
        *rect = priv->cached_rect;
      return TRUE;
    }

  parent_node = _mech_area_get_node (area)->parent;

  while (parent_node && parent_node->data != priv->area)
    {
      if (mech_area_get_clip (parent_node->data))
        break;

      parent_node = parent_node->parent;
    }

  if (!parent_node)
    return FALSE;

  _mech_area_get_stage_rect (area, &alloc);
  _mech_area_get_stage_rect (priv->area, &parent);
  dx = alloc.x - parent.x;
  dy = alloc.y - parent.y;

  region = mech_area_get_shape (area);
  cairo_region_translate (region, dx, dy);
  cairo_region_get_extents (region, &shape_rect);
  cairo_region_destroy (region);

  x1 = CLAMP (shape_rect.x, priv->cached_rect.x,
              priv->cached_rect.x + priv->cached_rect.width);
  y1 = CLAMP (shape_rect.y, priv->cached_rect.y,
              priv->cached_rect.y + priv->cached_rect.height);
  x2 = CLAMP (shape_rect.x + shape_rect.width, priv->cached_rect.x,
              priv->cached_rect.x + priv->cached_rect.width);
  y2 = CLAMP (shape_rect.y + shape_rect.height, priv->cached_rect.y,
              priv->cached_rect.y + priv->cached_rect.height);

  if (rect)
    {
      rect->x = x1 - dx;
      rect->y = y1 - dy;
      rect->width = x2 - x1;
      rect->height = y2 - y1;
    }

  return x2 > x1 && y2 > y1;
}

gboolean
_mech_surface_get_need_full_refresh (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  return priv->need_full_refresh;
}
