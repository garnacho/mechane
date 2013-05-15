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

#include "mech-area-private.h"
#include "mech-orientable.h"
#include "mech-linear-box.h"

typedef struct _MechLinearBoxPrivate MechLinearBoxPrivate;

enum {
  PROP_ORIENTATION = 1
};

struct _MechLinearBoxPrivate
{
  guint orientation : 1;
};

G_DEFINE_TYPE_WITH_CODE (MechLinearBox, mech_linear_box, MECH_TYPE_AREA,
                         G_ADD_PRIVATE (MechLinearBox)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_ORIENTABLE, NULL))

static void
mech_linear_box_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  MechLinearBoxPrivate *priv;

  priv = mech_linear_box_get_instance_private ((MechLinearBox *) object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_linear_box_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MechLinearBoxPrivate *priv;

  priv = mech_linear_box_get_instance_private ((MechLinearBox *) object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      priv->orientation = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gdouble
mech_linear_box_get_extent (MechArea *area,
                            MechAxis  axis)
{
  gdouble child_extent, extent = 0;
  MechLinearBoxPrivate *priv;
  GNode *children;

  priv = mech_linear_box_get_instance_private ((MechLinearBox *) area);
  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      child_extent = mech_area_get_extent (children->data, axis);

      if (priv->orientation == MECH_ORIENTATION_VERTICAL)
        extent = MAX (extent, child_extent);
      else
        extent += child_extent;
    }

  return extent;
}

static gdouble
mech_linear_box_get_second_extent (MechArea *area,
                                   MechAxis  axis,
                                   gdouble   other_value)
{
  MechLinearBox *box = (MechLinearBox *) area;
  gdouble child_extent, extent = 0;
  MechLinearBoxPrivate *priv;
  MechAxis other_axis;
  GNode *children;

  priv = mech_linear_box_get_instance_private (box);
  children = _mech_area_get_node (area)->children;
  other_axis = (axis == MECH_AXIS_X) ? MECH_AXIS_Y : MECH_AXIS_X;

  for (; children; children = children->next)
    {
      child_extent = mech_area_get_second_extent (children->data,
                                                  axis, other_value);

      if (priv->orientation == MECH_ORIENTATION_VERTICAL)
        extent += child_extent;
      else
        {
          other_value -= mech_area_get_extent (children->data, other_axis);
          extent = MAX (extent, child_extent);
        }
    }

  return extent;
}

static void
mech_linear_box_allocate_size (MechArea *area,
                               gdouble   width,
                               gdouble   height)
{
  MechLinearBox *box = (MechLinearBox *) area;
  MechLinearBoxPrivate *priv;
  gdouble accum = 0;
  GNode *children;

  priv = mech_linear_box_get_instance_private (box);
  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      cairo_rectangle_t rect;

      if (priv->orientation == MECH_ORIENTATION_VERTICAL)
        {
          rect.x = 0;
          rect.y = accum;
          rect.width = width;
          rect.height = mech_area_get_second_extent (children->data,
                                                     MECH_AXIS_Y, rect.width);

          mech_area_allocate_size (children->data, &rect);
          accum += rect.height;
        }
      else
        {
          rect.x = accum;
          rect.y = 0;
          rect.width = mech_area_get_extent (children->data, MECH_AXIS_X);
          rect.height = height;

          mech_area_allocate_size (children->data, &rect);
          accum += rect.width;
        }
    }

  g_free (children);
}

static void
mech_linear_box_class_init (MechLinearBoxClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_linear_box_set_property;
  object_class->get_property = mech_linear_box_get_property;

  area_class->get_extent = mech_linear_box_get_extent;
  area_class->get_second_extent = mech_linear_box_get_second_extent;
  area_class->allocate_size = mech_linear_box_allocate_size;

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");
}

static void
mech_linear_box_init (MechLinearBox *box)
{
}

MechArea *
mech_linear_box_new (MechOrientation orientation)
{
  return g_object_new (MECH_TYPE_LINEAR_BOX,
                       "orientation", orientation,
                       NULL);
}
