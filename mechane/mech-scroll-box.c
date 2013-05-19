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

#include <mechane/mech-scroll-box.h>
#include <mechane/mech-enum-types.h>
#include <mechane/mechane.h>
#include <math.h>

typedef struct _MechScrollBoxPrivate MechScrollBoxPrivate;

struct _MechScrollBoxPrivate
{
  MechAdjustable *hadjust;
  MechAdjustable *vadjust;
};

static void mech_scroll_box_scrollable_init (MechScrollableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MechScrollBox, mech_scroll_box, MECH_TYPE_FIXED_BOX,
                         G_ADD_PRIVATE (MechScrollBox)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_SCROLLABLE,
                                                mech_scroll_box_scrollable_init))

static void
_scroll_box_child_bounds (MechArea *child,
                          gdouble  *x1,
                          gdouble  *y1,
                          gdouble  *x2,
                          gdouble  *y2)
{
  if (MECH_IS_VIEW (child))
    {
      gdouble min, max;

      mech_view_get_boundaries ((MechView *) child,
                                MECH_AXIS_X, &min, &max);
      *x1 = min;
      *x2 = max;

      mech_view_get_boundaries ((MechView *) child,
                                MECH_AXIS_Y, &min, &max);
      *y1 = min;
      *y2 = max;
    }
  else
    {
      cairo_rectangle_t rect;

      mech_area_get_allocated_size (child, &rect);
      *x1 = 0;
      *x2 = rect.width;
      *y1 = 0;
      *y2 = rect.height;
    }
}

static void
_mech_scroll_box_update_children (MechScrollBox *box)
{
  gdouble vmin, vmax, vvalue, vrange;
  gdouble hmin, hmax, hvalue, hrange;
  MechScrollBoxPrivate *priv;
  cairo_rectangle_t parent;
  MechArea **children;
  guint i, n_children;

  priv = mech_scroll_box_get_instance_private (box);

  if (priv->hadjust)
    {
      mech_adjustable_get_bounds (priv->hadjust, &hmin, &hmax);
      hvalue = mech_adjustable_get_value (priv->hadjust);
      hrange = mech_adjustable_get_selection_size (priv->hadjust);
    }
  else
    hmin = hmax = hvalue = hrange = 0;

  if (priv->vadjust)
    {
      mech_adjustable_get_bounds (priv->vadjust, &vmin, &vmax);
      vvalue = mech_adjustable_get_value (priv->vadjust);
      vrange = mech_adjustable_get_selection_size (priv->vadjust);
    }
  else
    vmin = vmax = vvalue = vrange = 0;

  mech_area_get_allocated_size ((MechArea *) box, &parent);
  n_children = mech_area_get_children ((MechArea *) box, &children);

  for (i = 0; i < n_children; i++)
    {
      gdouble x1, y1, x2, y2;
      cairo_matrix_t matrix;
      gdouble tx, ty;

      _scroll_box_child_bounds (children[i], &x1, &y1, &x2, &y2);
      tx = ty = 0;

      if (priv->hadjust && (hmax - hrange - hmin) > 0)
        tx = round ((x2 - x1 - parent.width) * hvalue /
                      MAX (hmax - hrange - hmin, 1));

      if (priv->vadjust && (vmax - vrange - vmin) > 0)
        ty = round ((y2 - y1 - parent.height) * vvalue /
                    MAX (vmax - vrange - vmin, 1));

      cairo_matrix_init_translate (&matrix, -tx, -ty);
      mech_area_set_matrix (children[i], &matrix);
    }

  g_free (children);
}

static void
_mech_scroll_box_update_bounds (MechScrollBox *box)
{
  MechScrollBoxPrivate *priv;
  gdouble x1, y1, x2, y2;
  MechArea **children;
  guint i, n_children;

  priv = mech_scroll_box_get_instance_private (box);
  n_children = mech_area_get_children ((MechArea *) box, &children);
  x1 = y1 = G_MAXDOUBLE;
  x2 = y2 = -G_MAXDOUBLE;

  if (n_children == 0)
    return;

  for (i = 0; i < n_children; i++)
    {
      gdouble cx1, cy1, cx2, cy2;

      _scroll_box_child_bounds (children[i], &cx1, &cy1, &cx2, &cy2);

      x1 = MIN (x1, cx1);
      x2 = MAX (x2, cx2);
      y1 = MIN (y1, cy1);
      y2 = MAX (y2, cy2);
    }

  if (priv->hadjust)
    mech_adjustable_set_bounds (priv->hadjust, x1, x2);

  if (priv->vadjust)
    mech_adjustable_set_bounds (priv->vadjust, y1, y2);

  g_free (children);
}

static void
mech_scroll_box_allocate_size (MechArea *area,
                               gdouble   width,
                               gdouble   height)
{
  MechScrollBox *box = (MechScrollBox *) area;
  MechScrollBoxPrivate *priv;

  priv = mech_scroll_box_get_instance_private (box);
  MECH_AREA_CLASS (mech_scroll_box_parent_class)->allocate_size (area, width,
                                                                 height);
  if (priv->hadjust)
    mech_adjustable_set_selection_size (priv->hadjust, width);

  if (priv->vadjust)
    mech_adjustable_set_selection_size (priv->vadjust, height);

  _mech_scroll_box_update_bounds (box);
  _mech_scroll_box_update_children (box);
}

static gboolean
mech_scroll_box_child_resize (MechArea *area,
                              MechArea *child)
{
  _mech_scroll_box_update_bounds (MECH_SCROLL_BOX (area));
  return FALSE;
}

static void
mech_scroll_box_class_init (MechScrollBoxClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);

  area_class->allocate_size = mech_scroll_box_allocate_size;
  area_class->child_resize = mech_scroll_box_child_resize;
}

static void
_mech_scroll_box_adjustable_changed (MechAdjustable *adjustable,
                                     MechScrollable *scrollable)
{
  _mech_scroll_box_update_children (MECH_SCROLL_BOX (scrollable));
}

static MechAdjustable *
mech_scroll_box_get_adjustable (MechScrollable *scrollable,
                                MechAxis        axis)
{
  MechScrollBoxPrivate *priv;
  MechScrollBox *box;

  box = MECH_SCROLL_BOX (scrollable);
  priv = mech_scroll_box_get_instance_private (box);

  if (axis == MECH_AXIS_X)
    return priv->hadjust;
  else if (axis == MECH_AXIS_Y)
    return priv->vadjust;

  return NULL;
}

static void
mech_scroll_box_set_adjustable (MechScrollable *scrollable,
                                MechAxis        axis,
                                MechAdjustable *adjustable)
{
  MechScrollBoxPrivate *priv;
  MechAdjustable **adjust;
  MechScrollBox *box;

  box = MECH_SCROLL_BOX (scrollable);
  priv = mech_scroll_box_get_instance_private (box);

  if (axis == MECH_AXIS_X)
    adjust = &priv->hadjust;
  else if (axis == MECH_AXIS_Y)
    adjust = &priv->vadjust;
  else
    g_assert_not_reached ();

  if (*adjust)
    g_signal_handlers_disconnect_by_data (*adjust, scrollable);

  *adjust = adjustable;

  if (adjustable)
    g_signal_connect (adjustable, "value-changed",
                      G_CALLBACK (_mech_scroll_box_adjustable_changed),
                      scrollable);
}

static void
mech_scroll_box_scrollable_init (MechScrollableInterface *iface)
{
  iface->get_adjustable = mech_scroll_box_get_adjustable;
  iface->set_adjustable = mech_scroll_box_set_adjustable;
}

static void
mech_scroll_box_init (MechScrollBox *box)
{
}

MechArea *
mech_scroll_box_new (void)
{
  return g_object_new (MECH_TYPE_SCROLL_BOX, NULL);
}
