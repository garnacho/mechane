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

#define COPY_RECT(s,d) G_STMT_START                                     \
  {                                                                     \
    (d)->x = ((s)->x);                                                  \
    (d)->y = ((s)->y);                                                  \
    (d)->width = (s)->width;                                            \
    (d)->height = (s)->height;                                          \
  }                                                                     \
  G_STMT_END

#define EXTRA_PIXELS 400
#define AGE_BUFFER_LIMIT 3

typedef struct _MechSurfacePrivate MechSurfacePrivate;

enum {
  PROP_AREA = 1,
  PROP_PARENT,
  PROP_ABOVE,
  PROP_SURFACE_TYPE,
  PROP_RENDERER_TYPE
};

struct _MechSurfacePrivate
{
  MechArea *area;
  MechSurface *parent;
  MechSurface *above;

  /* area coordinates */
  cairo_rectangle_t cached_rect;
  cairo_rectangle_t viewport_rect;
  GArray *damaged;

  /* Pixel sizes */
  GArray *damage_cache;
  gint width;
  gint height;
  gint cache_width;
  gint cache_height;

  gdouble scale_x;
  gdouble scale_y;

  guint surface_type  : 3;
  guint renderer_type : 2;
};

static void mech_surface_initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MechSurface, mech_surface, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (MechSurface)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         mech_surface_initable_iface_init))

static gboolean
mech_surface_initable_init (GInitable     *initable,
                            GCancellable  *cancellable,
                            GError       **error)
{
  return TRUE;
}

static void
mech_surface_initable_iface_init (GInitableIface *iface)
{
  iface->init = mech_surface_initable_init;
}

static gint
mech_surface_get_age_impl (MechSurface *surface)
{
  return 0;
}

static gboolean
mech_surface_set_parent_impl (MechSurface *surface,
                              MechSurface *parent)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);

  if (!parent ||
      (priv->surface_type == MECH_SURFACE_TYPE_OFFSCREEN &&
       priv->renderer_type == _mech_surface_get_renderer_type (parent)))
    return TRUE;

  return FALSE;
}

static void
mech_surface_render_impl (MechSurface *surface,
                          cairo_t     *cr)
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
  cairo_paint (cr);
}

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
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    case PROP_ABOVE:
      g_value_set_object (value, priv->above);
      break;
    case PROP_SURFACE_TYPE:
      g_value_set_enum (value, priv->surface_type);
      break;
    case PROP_RENDERER_TYPE:
      g_value_set_enum (value , priv->renderer_type);
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
  MechSurface *surface;

  surface = (MechSurface *) object;
  priv = mech_surface_get_instance_private (surface);

  switch (prop_id)
    {
    case PROP_AREA:
      priv->area = g_value_get_object (value);
      break;
    case PROP_PARENT:
      _mech_surface_set_parent (surface, g_value_get_object (value));
      break;
    case PROP_ABOVE:
      _mech_surface_set_above (surface, g_value_get_object (value));
      break;
    case PROP_SURFACE_TYPE:
      priv->surface_type = g_value_get_enum (value);
      break;
    case PROP_RENDERER_TYPE:
      priv->renderer_type = g_value_get_enum (value);
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

  if (priv->parent)
    g_object_unref (priv->parent);

  if (priv->above)
    g_object_unref (priv->above);

  G_OBJECT_CLASS (mech_surface_parent_class)->finalize (object);
}

static void
mech_surface_class_init (MechSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->get_age = mech_surface_get_age_impl;
  klass->set_parent = mech_surface_set_parent_impl;
  klass->render = mech_surface_render_impl;

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
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        "Parent Surface",
                                                        "Parent Surface",
                                                        MECH_TYPE_SURFACE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_ABOVE,
                                   g_param_spec_object ("above",
                                                        "Sibling surface to place above",
                                                        "Sibling surface to place above",
                                                        MECH_TYPE_SURFACE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_SURFACE_TYPE,
                                   g_param_spec_enum ("surface-type",
                                                      "Surface type",
                                                      "Surface type",
                                                      MECH_TYPE_SURFACE_TYPE,
                                                      MECH_SURFACE_TYPE_SOFTWARE,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_RENDERER_TYPE,
                                   g_param_spec_enum ("renderer-type",
                                                      "Renderer type",
                                                      "Renderer type",
                                                      MECH_TYPE_RENDERER_TYPE,
                                                      MECH_RENDERER_TYPE_SOFTWARE,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
mech_surface_init (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  priv->scale_x = 1;
  priv->scale_y = 1;
  priv->surface_type = MECH_SURFACE_TYPE_SOFTWARE;
  priv->renderer_type = MECH_RENDERER_TYPE_SOFTWARE;
  priv->damage_cache = g_array_new (FALSE, FALSE, sizeof (cairo_region_t *));
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
  gint extra_pixels = 0;

  priv = mech_surface_get_instance_private (surface);
  mech_area_get_allocated_size (priv->area, &allocation);

  if (priv->surface_type == MECH_SURFACE_TYPE_OFFSCREEN)
    extra_pixels = EXTRA_PIXELS;

  if (!_mech_area_get_node (priv->area)->parent)
    {
      if (width)
        *width = CLAMP (priv->width + (2 * extra_pixels),
                        1, allocation.width * priv->scale_x);
      if (height)
        *height = CLAMP (priv->height + (2 * extra_pixels),
                         1, allocation.height * priv->scale_y);
    }
  else
    {
      if (width)
        *width = MAX (1, priv->width + (2 * extra_pixels));
      if (height)
        *height = MAX (1, priv->height + (2 * extra_pixels));
    }
}

static gboolean
_mech_surface_update_backing_size (MechSurface *surface)
{
  gint new_cache_width, new_cache_height;
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
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
  gint cache_width, cache_height, extra_pixels = 0;
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  mech_area_get_allocated_size (priv->area, &allocation);
  _calculate_cache_size (surface, &cache_width, &cache_height);

  if (dx)
    *dx = 0;
  if (dy)
    *dy = 0;

  if (priv->surface_type == MECH_SURFACE_TYPE_OFFSCREEN)
    extra_pixels = EXTRA_PIXELS;

  if (!_mech_area_get_node (priv->area)->parent)
    {
      new_surface_rect.width =
        MIN ((gdouble) cache_width / priv->scale_x, allocation.width);
      new_surface_rect.height =
        MIN ((gdouble) cache_height / priv->scale_y, allocation.height);
      new_surface_rect.x =
        CLAMP (viewport->x - (extra_pixels / priv->scale_x),
               0, MAX (allocation.width - new_surface_rect.width, 0));
      new_surface_rect.y =
        CLAMP (viewport->y - (extra_pixels / priv->scale_y),
               0, MAX (allocation.height - new_surface_rect.height, 0));
    }
  else
    {
      new_surface_rect.width = (gdouble) cache_width / priv->scale_x;
      new_surface_rect.height = (gdouble) cache_height / priv->scale_y;
      new_surface_rect.x = viewport->x - (extra_pixels / priv->scale_x);
      new_surface_rect.y = viewport->y - (extra_pixels / priv->scale_y);
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
}

static gint
_mech_surface_get_age (MechSurface *surface)
{
  MechSurfacePrivate *priv;
  gint age;

  priv = mech_surface_get_instance_private (surface);
  age = MECH_SURFACE_GET_CLASS (surface)->get_age (surface);

  if (age < 0 || age > AGE_BUFFER_LIMIT)
    age = 0;

  return age;
}

static void
_mech_surface_update_viewport (MechSurface *surface)
{
  cairo_rectangle_t rect, allocation;
  gdouble x1, y1, x2, y2, dx, dy;
  MechSurfacePrivate *priv;
  gboolean needs_scroll;

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

  needs_scroll =
    (x1 < priv->cached_rect.x ||
     x2 > priv->cached_rect.x + priv->cached_rect.width ||
     y1 < priv->cached_rect.y ||
     y2 > priv->cached_rect.y + priv->cached_rect.height);
  priv->viewport_rect = rect;

  if (_mech_surface_get_age (surface) == 0 ||
      _mech_surface_update_scale (surface))
    {
      _mech_surface_update_cached_rect (surface, &rect, FALSE, NULL, NULL);
      _mech_surface_damage (surface, &priv->cached_rect);
    }
  else if (needs_scroll || !_mech_area_get_node (priv->area)->parent)
    {
      _mech_surface_update_cached_rect (surface, &rect, TRUE, &dx, &dy);

      if (needs_scroll && (dx != 0 || dy != 0))
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

static void
_mech_surface_get_matrix (MechSurface    *surface,
                          cairo_matrix_t *matrix)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  cairo_matrix_init_identity (matrix);
  cairo_matrix_translate (matrix, priv->cached_rect.x,
                          priv->cached_rect.y);
  cairo_matrix_scale (matrix, 1 / priv->scale_x, 1 / priv->scale_y);
}

static void
_mech_surface_store_damage (MechSurface *surface)
{
  MechSurfacePrivate *priv;
  cairo_region_t *region;

  priv = mech_surface_get_instance_private (surface);
  region = cairo_region_create ();

  if (priv->damaged && priv->damaged->len > 0)
    {
      cairo_matrix_t matrix;
      gint i;

      _mech_surface_get_matrix (surface, &matrix);
      cairo_matrix_invert (&matrix);

      for (i = 0; i < priv->damaged->len; i++)
        {
          cairo_rectangle_int_t pixels;
          cairo_rectangle_t *rect;

          rect = &g_array_index (priv->damaged, cairo_rectangle_t, i);
          cairo_matrix_transform_point (&matrix, &rect->x, &rect->y);
          cairo_matrix_transform_distance (&matrix, &rect->width, &rect->height);

          ALIGN_RECT (rect, &pixels);

          cairo_region_union_rectangle (region, &pixels);
        }

      g_array_unref (priv->damaged);
      priv->damaged = NULL;
    }

  g_array_prepend_val (priv->damage_cache, region);

  if (priv->damage_cache->len > AGE_BUFFER_LIMIT)
    {
      region = g_array_index (priv->damage_cache, cairo_region_t *,
                              priv->damage_cache->len - 1);
      cairo_region_destroy (region);

      g_array_remove_index (priv->damage_cache, priv->damage_cache->len - 1);
    }
}

cairo_t *
_mech_surface_cairo_create (MechSurface *surface)
{
  cairo_surface_t *cairo_surface;
  MechSurfacePrivate *priv;
  cairo_t *cr;

  priv = mech_surface_get_instance_private (surface);
  _mech_surface_update_surface (surface);

  cairo_surface = MECH_SURFACE_GET_CLASS (surface)->get_surface (surface);
  g_assert (cairo_surface != NULL);

  _mech_surface_update_viewport (surface);

  cr = cairo_create (cairo_surface);
  cairo_scale (cr, priv->scale_x, priv->scale_y);
  cairo_translate (cr, -priv->cached_rect.x, -priv->cached_rect.y);

  _mech_surface_store_damage (surface);

  return cr;
}

MechSurface *
_mech_surface_new (MechArea        *area,
                   MechSurface     *parent,
                   MechSurfaceType  surface_type)
{
  MechSurface *surface;
  MechBackend *backend;

  backend = mech_backend_get ();
  surface = mech_backend_create_surface (backend, parent, surface_type);
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

static cairo_region_t *
_mech_surface_get_damage_region (MechSurface *surface)
{
  MechSurfacePrivate *priv;
  cairo_region_t *region;
  gint age;

  priv = mech_surface_get_instance_private (surface);
  age = _mech_surface_get_age (surface);
  region = cairo_region_create ();

  if (age == 0 || age > priv->damage_cache->len)
    {
      cairo_rectangle_int_t rect;

      /* Too new, or too old, everything must be redrawn */
      rect.x = rect.y = 0;
      rect.width = priv->cache_width;
      rect.height = priv->cache_height;
      cairo_region_union_rectangle (region, &rect);
    }
  else
    {
      cairo_region_t *cache_region;
      gint i;

      for (i = 0; i < age; i++)
        {
          cache_region = g_array_index (priv->damage_cache,
                                        cairo_region_t *, i);
          cairo_region_union (region, cache_region);
        }
    }

  return region;
}

void
_mech_surface_push_update (MechSurface *surface)
{
  MechSurfaceClass *surface_class;
  cairo_region_t *region;

  surface_class = MECH_SURFACE_GET_CLASS (surface);

  if (!surface_class->push_update)
    return;

  region = _mech_surface_get_damage_region (surface);
  surface_class->push_update (surface, region);
  cairo_region_destroy (region);
}

gboolean
_mech_surface_apply_clip (MechSurface *surface,
                          cairo_t     *cr)
{
  cairo_region_t *damage_region;
  MechSurfacePrivate *priv;
  gint i, n_rects;

  priv = mech_surface_get_instance_private (surface);
  damage_region = _mech_surface_get_damage_region (surface);

  if (cairo_region_is_empty (damage_region))
    {
      cairo_region_destroy (damage_region);
      return FALSE;
    }

  n_rects = cairo_region_num_rectangles (damage_region);

  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t region_rect;
      cairo_rectangle_t rect;

      cairo_region_get_rectangle (damage_region, i, &region_rect);
      COPY_RECT (&region_rect, &rect);

      cairo_device_to_user (cr, &rect.x, &rect.y);
      cairo_device_to_user_distance (cr, &rect.width, &rect.height);
      cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
    }

  cairo_clip (cr);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);
  cairo_restore (cr);

  cairo_region_destroy (damage_region);

  return TRUE;
}

cairo_region_t *
_mech_surface_get_clip (MechSurface *surface)
{
  cairo_region_t *damage_region, *clip;
  MechSurfacePrivate *priv;
  cairo_matrix_t matrix;
  gint i, n_rects;

  priv = mech_surface_get_instance_private (surface);
  damage_region = _mech_surface_get_damage_region (surface);
  n_rects = cairo_region_num_rectangles (damage_region);
  clip = cairo_region_create ();

  _mech_surface_get_matrix (surface, &matrix);

  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t region_rect;
      cairo_rectangle_t rect;

      cairo_region_get_rectangle (damage_region, i, &region_rect);
      COPY_RECT (&region_rect, &rect);

      cairo_matrix_transform_point (&matrix, &rect.x, &rect.y);
      cairo_matrix_transform_distance (&matrix, &rect.width, &rect.height);

      ALIGN_RECT (&rect, &region_rect);
      cairo_region_union_rectangle (clip, &region_rect);
    }

  cairo_region_destroy (damage_region);

  return clip;
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

MechSurface *
_mech_surface_get_parent (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  g_return_val_if_fail (MECH_IS_SURFACE (surface), NULL);

  priv = mech_surface_get_instance_private (surface);
  return priv->parent;
}

gboolean
_mech_surface_set_parent (MechSurface *surface,
                          MechSurface *parent)
{
  MechSurfacePrivate *priv;

  g_return_val_if_fail (MECH_IS_SURFACE (surface), NULL);

  priv = mech_surface_get_instance_private (surface);

  if (priv->parent == parent)
    return TRUE;

  if (priv->parent)
    {
      g_object_unref (priv->parent);
      priv->parent = NULL;
    }

  if (MECH_SURFACE_GET_CLASS (surface)->set_parent (surface, parent))
    {
      priv->parent = (parent) ? g_object_ref (parent) : NULL;
      g_object_notify ((GObject *) surface, "parent");
      return TRUE;
    }

  return FALSE;
}

MechSurface *
_mech_surface_get_above (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  g_return_val_if_fail (MECH_IS_SURFACE (surface), NULL);

  priv = mech_surface_get_instance_private (surface);

  return priv->above;
}

void
_mech_surface_set_above (MechSurface *surface,
                         MechSurface *sibling)
{
  MechSurfacePrivate *priv;

  g_return_if_fail (MECH_IS_SURFACE (surface));

  priv = mech_surface_get_instance_private (surface);

  if (priv->above == sibling)
    return;

  if (sibling)
    g_object_ref (sibling);

  if (priv->above)
    g_object_unref (priv->above);

  MECH_SURFACE_GET_CLASS (surface)->set_above (surface, sibling);
  priv->above = sibling;
  g_object_notify ((GObject *) surface, "above");
}

void
_mech_surface_render (MechSurface *surface,
                      cairo_t     *cr)
{
  g_return_if_fail (MECH_IS_SURFACE (surface));
  g_return_if_fail (cr != NULL);

  MECH_SURFACE_GET_CLASS (surface)->render (surface, cr);
}

MechSurfaceType
_mech_surface_get_surface_type (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  g_return_val_if_fail (MECH_IS_SURFACE (surface), MECH_SURFACE_TYPE_NONE);

  priv = mech_surface_get_instance_private (surface);
  return priv->surface_type;
}

MechRendererType
_mech_surface_get_renderer_type (MechSurface *surface)
{
  MechSurfacePrivate *priv;

  g_return_val_if_fail (MECH_IS_SURFACE (surface), MECH_SURFACE_TYPE_NONE);

  priv = mech_surface_get_instance_private (surface);
  return priv->renderer_type;
}
