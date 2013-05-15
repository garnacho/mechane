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

#include "mech-orientable.h"

static void mech_orientable_default_init (MechOrientableInterface *iface);

G_DEFINE_INTERFACE (MechOrientable, mech_orientable, MECH_TYPE_AREA)

static void
mech_orientable_default_init (MechOrientableInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("orientation",
                                                          "Orientation",
                                                          "The orientation of the orientable",
                                                          MECH_TYPE_ORIENTATION,
                                                          MECH_ORIENTATION_HORIZONTAL,
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_STRINGS));
}

void
mech_orientable_set_orientation (MechOrientable  *orientable,
                                 MechOrientation  orientation)
{
  g_return_if_fail (MECH_IS_ORIENTABLE (orientable));
  g_return_if_fail (orientation == MECH_ORIENTATION_HORIZONTAL ||
                    orientation == MECH_ORIENTATION_VERTICAL);

  g_object_set (orientable, "orientation", orientation, NULL);
}

MechOrientation
mech_orientable_get_orientation (MechOrientable *orientable)
{
  MechOrientation orientation;

  g_return_val_if_fail (MECH_IS_ORIENTABLE (orientable),
                        MECH_ORIENTATION_HORIZONTAL);

  g_object_get (orientable, "orientation", &orientation, NULL);

  return orientation;
}
