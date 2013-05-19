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

#include <cairo/cairo-gobject.h>
#include "mech-area-private.h"
#include "mech-scrollable.h"
#include "mech-view.h"
#include "mech-marshal.h"

typedef struct _MechViewPrivate MechViewPrivate;

enum {
  VIEWPORT_MOVED,
  N_SIGNALS
};

struct _MechViewPrivate
{
  cairo_region_t *view_area;
  guint predraw_signal_id;
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechView, mech_view, MECH_TYPE_AREA)

static void
mech_view_finalize (GObject *object)
{
  MechViewPrivate *priv;

  priv = mech_view_get_instance_private ((MechView *) object);

  if (priv->view_area)
    cairo_region_destroy (priv->view_area);

  G_OBJECT_CLASS (mech_view_parent_class)->finalize (object);
}

static void
_mech_view_store_extents (MechView *view)
{
  cairo_region_t *visible, *old;
  MechViewPrivate *priv;

  priv = mech_view_get_instance_private (view);
  visible = _mech_area_get_renderable_region (MECH_AREA (view));

  if (((!priv->view_area && cairo_region_is_empty (visible)) ||
       (priv->view_area && cairo_region_equal (priv->view_area, visible))))
    {
      cairo_region_destroy (visible);
      return;
    }

  if (visible && priv->view_area &&
      cairo_region_is_empty (visible) &&
      cairo_region_is_empty (priv->view_area))
    {
      cairo_region_destroy (visible);
      return;
    }

  if (priv->view_area)
    {
      old = priv->view_area;
      priv->view_area = NULL;
    }
  else
    old = cairo_region_create ();

  g_signal_emit (view, signals[VIEWPORT_MOVED], 0, visible, old);
  priv->view_area = visible;

  cairo_region_destroy (old);
}

static void
_mech_view_predraw (MechArea *area,
                    cairo_t  *cr,
                    gpointer  user_data)
{
  _mech_view_store_extents (MECH_VIEW (area));
}

static void
mech_view_get_boundaries_impl (MechView *view,
                               MechAxis  axis,
                               gdouble  *min,
                               gdouble  *max)
{
  cairo_rectangle_t alloc;

  mech_area_get_allocated_size ((MechArea *) view, &alloc);
  *min = 0;

  if (axis == MECH_AXIS_X)
    *max = alloc.width;
  else if (axis == MECH_AXIS_Y)
    *max = alloc.height;
}

static cairo_region_t *
mech_view_get_shape (MechArea *area)
{
  cairo_rectangle_int_t rect;
  cairo_rectangle_t alloc;
  gdouble min, max;

  mech_area_get_allocated_size (area, &alloc);

  mech_view_get_boundaries ((MechView *) area, MECH_AXIS_X, &min, &max);
  rect.x = min;
  rect.width = max - min;
  rect.width = MAX (max - min, alloc.width - min);

  mech_view_get_boundaries ((MechView *) area, MECH_AXIS_Y, &min, &max);
  rect.y = min;
  rect.height = max - min;
  rect.height = MAX (max - min, alloc.height - min);

  return cairo_region_create_rectangle (&rect);
}

static void
mech_view_class_init (MechViewClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_view_finalize;

  klass->get_boundaries = mech_view_get_boundaries_impl;
  area_class->get_shape = mech_view_get_shape;

  signals[VIEWPORT_MOVED] =
    g_signal_new ("viewport-moved",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechViewClass, viewport_moved),
                  NULL, NULL,
                  _mech_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE, 2,
                  CAIRO_GOBJECT_TYPE_REGION | G_SIGNAL_TYPE_STATIC_SCOPE,
                  CAIRO_GOBJECT_TYPE_REGION | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
mech_view_init (MechView *view)
{
  MechViewPrivate *priv;

  priv = mech_view_get_instance_private (view);
  priv->predraw_signal_id = g_signal_connect (view, "draw",
                                              G_CALLBACK (_mech_view_predraw),
                                              NULL);
}

void
mech_view_set_position (MechView *view,
                        gdouble   x,
                        gdouble   y)
{
  MechAdjustable *vadj, *hadj;
  MechArea *scrollable;
  gdouble min, max;

  g_return_if_fail (MECH_IS_VIEW (view));

  scrollable = mech_area_get_parent (MECH_AREA (view));

  while (scrollable && !MECH_IS_SCROLLABLE (scrollable))
    scrollable = mech_area_get_parent (MECH_AREA (scrollable));

  if (!scrollable)
    return;

  hadj = mech_scrollable_get_adjustable (MECH_SCROLLABLE (scrollable),
                                         MECH_AXIS_X);
  mech_adjustable_get_bounds (hadj, &min, &max);
  mech_adjustable_set_value (hadj, CLAMP (x, min, max));

  vadj = mech_scrollable_get_adjustable (MECH_SCROLLABLE (scrollable),
                                         MECH_AXIS_Y);
  mech_adjustable_get_bounds (vadj, &min, &max);
  mech_adjustable_set_value (vadj, CLAMP (y, min, max));
}

gdouble
mech_view_get_boundaries (MechView *view,
                          MechAxis  axis,
                          gdouble  *min,
                          gdouble  *max)
{
  gdouble min_v, max_v;

  g_return_if_fail (MECH_IS_VIEW (view));
  g_return_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y);

  MECH_VIEW_GET_CLASS (view)->get_boundaries (view, axis, &min_v, &max_v);

  if (min)
    *min = min_v;
  if (max)
    *max = max_v;

  return max_v - min_v;
}
