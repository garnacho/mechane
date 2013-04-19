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

#include <mechane/mech-scrollable.h>

G_DEFINE_INTERFACE (MechScrollable, mech_scrollable, MECH_TYPE_AREA)

static void
mech_scrollable_default_init (MechScrollableInterface *iface)
{
}

MechAdjustable *
mech_scrollable_get_adjustable (MechScrollable *scrollable,
                                MechAxis        axis)
{
  MechScrollableInterface *iface;

  g_return_val_if_fail (MECH_IS_SCROLLABLE (scrollable), NULL);
  g_return_val_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y, NULL);

  iface = MECH_SCROLLABLE_GET_IFACE (scrollable);

  if (!iface->set_adjustable)
    {
      g_warning ("%s: no vmethod implementation", G_STRFUNC);
      return NULL;
    }

  return iface->get_adjustable (scrollable, axis);
}

void
mech_scrollable_set_adjustable (MechScrollable *scrollable,
                                MechAxis        axis,
                                MechAdjustable *adjustable)
{
  MechScrollableInterface *iface;

  g_return_if_fail (MECH_IS_SCROLLABLE (scrollable));
  g_return_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y);

  iface = MECH_SCROLLABLE_GET_IFACE (scrollable);

  if (!iface->set_adjustable)
    {
      g_warning ("%s: no vmethod implementation", G_STRFUNC);
      return;
    }

  iface->set_adjustable (scrollable, axis, adjustable);
}
