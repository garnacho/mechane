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

#include <mechane/mech-types.h>

G_DEFINE_BOXED_TYPE (MechColor, mech_color, mech_color_copy, mech_color_free)

MechColor *
mech_color_copy (MechColor *color)
{
  return g_slice_dup (MechColor, color);
}

void
mech_color_free (MechColor *color)
{
  g_slice_free (MechColor, color);
}

G_DEFINE_BOXED_TYPE (MechPoint, mech_point, mech_point_copy, mech_point_free)

MechPoint *
mech_point_copy (MechPoint *point)
{
  return g_slice_dup (MechPoint, point);
}

void
mech_point_free (MechPoint *point)
{
  g_slice_free (MechPoint, point);
}

G_DEFINE_BOXED_TYPE (MechBorder, mech_border, mech_border_copy, mech_border_free)

MechBorder *
mech_border_copy (MechBorder *border)
{
  return g_slice_dup (MechBorder, border);
}

void
mech_border_free (MechBorder *border)
{
  g_slice_free (MechBorder, border);
}
