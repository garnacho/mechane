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
#include "mech-floating-box.h"

G_DEFINE_TYPE (MechFloatingBox, mech_floating_box, MECH_TYPE_AREA)

static gdouble
mech_floating_box_get_extent (MechArea *area,
                              MechAxis  axis)
{
  GNode *children;
  gdouble extent = 0;

  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      gdouble child_size;

      child_size = mech_area_get_extent (children->data, axis);
      extent = MAX (extent, child_size);
    }

  return extent;
}

static gdouble
mech_floating_box_get_second_extent (MechArea *area,
                                     MechAxis  axis,
                                     gdouble   other_value)
{
  gdouble accum, maximum_in_row, space_left;
  GNode *children;

  space_left = other_value;
  accum = maximum_in_row = 0;
  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      if (axis == MECH_AXIS_Y)
        {
          gdouble width, height;

          width = mech_area_get_extent (children->data, MECH_AXIS_X);
          height = mech_area_get_second_extent (children->data,
                                                MECH_AXIS_Y, width);
          if (space_left < width)
            {
              /* Jump to next row */
              accum += maximum_in_row;
              maximum_in_row = height;
              space_left = other_value - width;
            }
          else
            {
              space_left -= width;
              maximum_in_row = MAX (maximum_in_row, height);
            }
        }
      /* FIXME: X axis unimplemented */
    }

  accum += maximum_in_row;

  return accum;
}

static void
mech_floating_box_allocate_size (MechArea *area,
                                 gdouble   width,
                                 gdouble   height)
{
  gdouble max_height, x, y;
  GNode *children;

  max_height = x = y = 0;
  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      cairo_rectangle_t child_rect;

      child_rect.width = mech_area_get_extent (children->data, MECH_AXIS_X);
      child_rect.height = mech_area_get_second_extent (children->data,
                                                       MECH_AXIS_Y,
                                                       child_rect.width);

      if (children->prev && x + child_rect.width > width)
        {
          y += max_height;
          max_height = 0;
          x = 0;
	}

      max_height = MAX (max_height, child_rect.height);

      child_rect.x = x;
      child_rect.y = y;
      x += child_rect.width;
      mech_area_allocate_size (children->data, &child_rect);
    }
}

static void
mech_floating_box_class_init (MechFloatingBoxClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);

  area_class->get_extent = mech_floating_box_get_extent;
  area_class->get_second_extent = mech_floating_box_get_second_extent;
  area_class->allocate_size = mech_floating_box_allocate_size;
}

static void
mech_floating_box_init (MechFloatingBox *box)
{
}

MechArea *
mech_floating_box_new (void)
{
  return g_object_new (MECH_TYPE_FLOATING_BOX, NULL);
}
