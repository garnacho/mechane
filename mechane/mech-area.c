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
#include <cairo/cairo-gobject.h>

#include <mechane/mech-area.h>

typedef struct _MechAreaPrivate MechAreaPrivate;

struct _MechAreaPrivate
{
  gdouble width_requested;
  gdouble height_requested;

  guint need_width_request  : 1;
  guint need_height_request : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechArea, mech_area, G_TYPE_INITIALLY_UNOWNED)

static void
mech_area_init (MechArea *area)
{
  MechAreaPrivate *priv = mech_area_get_instance_private (area);

  priv->need_width_request = priv->need_height_request = TRUE;
}

static gdouble
mech_area_get_extent_impl (MechArea *area,
                           MechAxis  axis)
{
  gdouble ret = 0;

  return ret;
}

static gdouble
mech_area_get_second_extent_impl (MechArea *area,
                                  MechAxis  axis,
                                  gdouble   other_value)
{
  gdouble ret = 0;

  return ret;
}

static void
mech_area_class_init (MechAreaClass *klass)
{
  klass->get_extent = mech_area_get_extent_impl;
  klass->get_second_extent = mech_area_get_second_extent_impl;
}

gdouble
mech_area_get_extent (MechArea *area,
                      MechAxis  axis)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);
  g_return_val_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y, 0);

  priv = mech_area_get_instance_private (area);

  if ((axis == MECH_AXIS_X && priv->need_width_request) ||
      (axis == MECH_AXIS_Y && priv->need_height_request))
    {
      gdouble val;

      val = MECH_AREA_GET_CLASS (area)->get_extent (area, axis);

      if (axis == MECH_AXIS_X)
        {
          priv->width_requested = val;
          priv->need_width_request = FALSE;
        }
      else if (axis == MECH_AXIS_Y)
        {
          priv->height_requested = val;
          priv->need_height_request = FALSE;
        }
    }

  return (axis == MECH_AXIS_X) ? priv->width_requested : priv->height_requested;
}

gdouble
mech_area_get_second_extent (MechArea *area,
                             MechAxis  axis,
                             gdouble   other_value)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);
  g_return_val_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y, 0);

  priv = mech_area_get_instance_private (area);

  if ((axis == MECH_AXIS_X && priv->need_width_request) ||
      (axis == MECH_AXIS_Y && priv->need_height_request))
    {
      gdouble val;

      val = MECH_AREA_GET_CLASS (area)->get_second_extent (area, axis,
                                                           other_value);

      if (axis == MECH_AXIS_X)
        {
          priv->width_requested = val;
          priv->need_width_request = FALSE;
        }
      else if (axis == MECH_AXIS_Y)
        {
          priv->height_requested = val;
          priv->need_height_request = FALSE;
        }
    }

  return (axis == MECH_AXIS_X) ? priv->width_requested : priv->height_requested;
}
