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
#include "mech-fixed-box.h"

G_DEFINE_TYPE (MechFixedBox, mech_fixed_box, MECH_TYPE_AREA)

typedef struct _SideAttachment SideAttachment;
typedef struct _AreaAttachment AreaAttachment;

struct _SideAttachment
{
  guint attach_side : 2;
  guint unit        : 3;
  gdouble distance;
};

struct _AreaAttachment
{
  SideAttachment sides[4];
  guint attached_sides : 4;
};

static GQuark quark_attachment = 0;

static void
_mech_fixed_box_add_attachment_extents (MechFixedBox *box,
                                        MechArea     *area,
                                        MechAxis      axis,
                                        gdouble      *value)
{
  gdouble distance;
  MechSide attach;

  if (axis == MECH_AXIS_X)
    {
      if (mech_fixed_box_get_attachment (box, area, MECH_SIDE_LEFT,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_LEFT)
            *value += distance;
          else if (attach == MECH_SIDE_RIGHT)
            *value = MAX (*value, distance);
        }

      if (mech_fixed_box_get_attachment (box, area, MECH_SIDE_RIGHT,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_RIGHT)
            *value += distance;
          else if (attach == MECH_SIDE_LEFT)
            *value += MAX (*value, distance);
        }
    }
  else
    {
      if (mech_fixed_box_get_attachment (box, area, MECH_SIDE_TOP,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_TOP)
            *value += distance;
          if (attach == MECH_SIDE_BOTTOM)
            *value = MAX (*value, distance);
        }

      if (mech_fixed_box_get_attachment (box, area, MECH_SIDE_BOTTOM,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_BOTTOM)
            *value += distance;
          else if (attach == MECH_SIDE_TOP)
            *value = MAX (*value, distance);
        }
    }
}

static gdouble
mech_fixed_box_get_extent (MechArea *area,
                           MechAxis  axis)
{
  MechFixedBox *box = (MechFixedBox *) area;
  MechSideFlags sides;
  gdouble extent = 0;
  GNode *children;

  if (axis == MECH_AXIS_X)
    sides = MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_RIGHT;
  else
    sides = MECH_SIDE_FLAG_TOP | MECH_SIDE_FLAG_BOTTOM;

  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      gdouble child_extent;

      if (!mech_fixed_box_child_is_attached (box, children->data, sides))
        continue;

      child_extent = mech_area_get_extent (children->data, axis);
      _mech_fixed_box_add_attachment_extents (box, children->data,
                                              axis, &child_extent);
      extent = MAX (extent, child_extent);
    }

  return extent;
}

static gdouble
mech_fixed_box_get_second_extent (MechArea *area,
                                  MechAxis  axis,
                                  gdouble   other_value)
{
  MechFixedBox *box = (MechFixedBox *) area;
  MechSideFlags sides;
  gdouble extent = 0;
  GNode *children;

  if (axis == MECH_AXIS_X)
    sides = MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_RIGHT;
  else
    sides = MECH_SIDE_FLAG_TOP | MECH_SIDE_FLAG_BOTTOM;

  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      gdouble child_extent;

      if (!mech_fixed_box_child_is_attached (box, children->data, sides))
        continue;

      child_extent = mech_area_get_second_extent (children->data,
                                                  axis, other_value);
      _mech_fixed_box_add_attachment_extents (box, children->data,
                                              axis, &child_extent);
      extent = MAX (extent, child_extent);
    }

  return extent;
}

static void
mech_fixed_box_allocate_size (MechArea *area,
                              gdouble   width,
                              gdouble   height)
{
  MechFixedBox *box = (MechFixedBox *) area;
  GNode *children;

  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      cairo_rectangle_t child_rect = { 0 };
      gboolean hattached, vattached;
      gdouble distance, extent;
      gdouble x1, y1, x2, y2;
      MechSide attach;

      hattached = vattached = FALSE;
      x1 = x2 = y1 = y2 = 0;

      extent = mech_area_get_extent (children->data, MECH_AXIS_X);

      if (mech_fixed_box_get_attachment (box, children->data, MECH_SIDE_LEFT,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_LEFT)
            x1 = distance;
          else if (attach == MECH_SIDE_RIGHT)
            x1 = width - distance;

          x2 = x1 + extent;
          hattached = TRUE;
        }

      if (mech_fixed_box_get_attachment (box, children->data, MECH_SIDE_RIGHT,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_RIGHT)
            x2 = width - distance;
          else if (attach == MECH_SIDE_LEFT)
            x2 = distance;

          if (!hattached)
            x1 = x2 - extent;

          hattached = TRUE;
        }

      child_rect.x = x1;
      child_rect.width = MAX (extent, x2 - x1);

      extent = mech_area_get_second_extent (children->data, MECH_AXIS_Y,
                                            child_rect.width);

      if (mech_fixed_box_get_attachment (box, children->data, MECH_SIDE_TOP,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_TOP)
            y1 = distance;
          else if (attach == MECH_SIDE_BOTTOM)
            y1 = height - distance;

          y2 = y1 + extent;
          vattached = TRUE;
        }

      if (mech_fixed_box_get_attachment (box, children->data, MECH_SIDE_BOTTOM,
                                         MECH_UNIT_PX, &attach, &distance))
        {
          if (attach == MECH_SIDE_BOTTOM)
            y2 = height - distance;
          else if (attach == MECH_SIDE_TOP)
            y2 = distance;

          if (!vattached)
            y1 = y2 - extent;

          vattached = TRUE;
        }

      child_rect.y = y1;
      child_rect.height = MAX (extent, y2 - y1);

      if (hattached && !vattached)
        {
          /* Set a default y1/y2 */
          y1 = 0;
          y2 = y1 + child_rect.height;
        }
      else if (vattached && !hattached)
        {
          /* Set a default x1/x2 */
          x1 = 0;
          x2 = x1 + child_rect.width;
        }

      mech_area_allocate_size (children->data, &child_rect);
    }

  g_free (children);
}

static void
mech_fixed_box_class_init (MechFixedBoxClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);

  area_class->get_extent = mech_fixed_box_get_extent;
  area_class->get_second_extent = mech_fixed_box_get_second_extent;
  area_class->allocate_size = mech_fixed_box_allocate_size;

  quark_attachment = g_quark_from_static_string ("MECH_QUARK_CHILD_ATTACHMENT");
}

static void
mech_fixed_box_init (MechFixedBox *box)
{
}

MechArea *
mech_fixed_box_new (void)
{
  return g_object_new (MECH_TYPE_FIXED_BOX, NULL);
}

static AreaAttachment *
_mech_fixed_box_get_child_attachment (MechFixedBox *box,
                                      MechArea     *area)
{
  AreaAttachment *attachment;
  GNode *node;

  node = _mech_area_get_node (area);

  if (!node->parent || node->parent->data != box)
    return NULL;

  attachment = g_object_get_qdata ((GObject *) area, quark_attachment);

  if (!attachment)
    {
      attachment = g_new0 (AreaAttachment, 1);
      g_object_set_qdata_full ((GObject *) area, quark_attachment,
                               attachment, g_free);
    }

  return attachment;
}

gboolean
mech_fixed_box_child_is_attached (MechFixedBox    *box,
				  MechArea        *area,
				  MechSideFlags    sides)
{
  AreaAttachment *attachment;

  g_return_val_if_fail (MECH_IS_FIXED_BOX (box), FALSE);
  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);

  sides &= MECH_SIDE_FLAG_ALL;
  attachment = _mech_fixed_box_get_child_attachment (box, area);

  if (!attachment)
    return FALSE;

  if (!sides)
    return attachment->attached_sides != 0;
  else
    return (attachment->attached_sides & sides) == sides;
}

void
mech_fixed_box_attach (MechFixedBox    *box,
                       MechArea        *area,
                       MechSide         side,
                       MechSide         attach_side,
                       MechUnit         unit,
                       gdouble          distance)
{
  AreaAttachment *attachment;

  g_return_if_fail (MECH_IS_FIXED_BOX (box));
  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (side >= MECH_SIDE_LEFT && side <= MECH_SIDE_BOTTOM);
  g_return_if_fail (attach_side >= MECH_SIDE_LEFT && attach_side <= MECH_SIDE_BOTTOM);
  g_return_if_fail (unit >= MECH_UNIT_PX && unit <= MECH_UNIT_PERCENTAGE);

  attachment = _mech_fixed_box_get_child_attachment (box, area);
  g_return_if_fail (attachment != NULL);

  attachment->sides[side].attach_side = attach_side;
  attachment->sides[side].unit = unit;
  attachment->sides[side].distance = distance;
  attachment->attached_sides |= (1 << side);
}

void
mech_fixed_box_detach (MechFixedBox    *box,
                       MechArea        *area,
                       MechSide         side)
{
  AreaAttachment *attachment;

  g_return_if_fail (MECH_IS_FIXED_BOX (box));
  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (side >= MECH_SIDE_LEFT && side <= MECH_SIDE_BOTTOM);

  attachment = _mech_fixed_box_get_child_attachment (box, area);
  g_return_if_fail (attachment != NULL);

  attachment->attached_sides &= ~(1 << side);
}

gboolean
mech_fixed_box_get_attachment (MechFixedBox    *box,
                               MechArea        *area,
                               MechSide         side,
                               MechUnit         unit,
                               MechSide        *attach_side,
                               gdouble         *attach)
{
  AreaAttachment *attachment;

  g_return_val_if_fail (MECH_IS_FIXED_BOX (box), FALSE);
  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);
  g_return_val_if_fail (side >= MECH_SIDE_LEFT && side <= MECH_SIDE_BOTTOM, FALSE);
  g_return_val_if_fail (unit >= MECH_UNIT_PX && unit <= MECH_UNIT_PERCENTAGE, FALSE);

  attachment = _mech_fixed_box_get_child_attachment (box, area);

  if (!attachment || (attachment->attached_sides & (1 << side)) == 0)
    return FALSE;

  if (attach_side)
    *attach_side = attachment->sides[side].attach_side;

  if (attach)
    {
      MechAxis axis;

      axis = (side == MECH_SIDE_LEFT || side == MECH_SIDE_RIGHT) ?
        MECH_AXIS_X : MECH_AXIS_Y;
      *attach = mech_area_translate_unit (area,
                                          attachment->sides[side].distance,
                                          attachment->sides[side].unit, unit,
                                          axis);
    }

  return TRUE;
}
