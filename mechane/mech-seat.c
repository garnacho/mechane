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

#include <mechane/mech-seat.h>

G_DEFINE_ABSTRACT_TYPE (MechSeat, mech_seat, G_TYPE_OBJECT)

static void
mech_seat_class_init (MechSeatClass *klass)
{
}

static void
mech_seat_init (MechSeat *seat)
{
}

guint
mech_seat_get_modifiers (MechSeat *seat,
                         guint    *active,
                         guint    *latched,
                         guint    *locked)
{
  guint act, lat, loc;

  g_return_val_if_fail (MECH_IS_SEAT (seat), 0);

  MECH_SEAT_GET_CLASS (seat)->get_modifiers (seat, &act, &lat, &loc);

  if (active)
    *active = act;
  if (latched)
    *latched = lat;
  if (locked)
    *locked = loc;

  return act | lat | loc;
}
