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

#include <mechane/mech-area.h>
#include <mechane/mech-toggle.h>

enum {
  TOGGLED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE (MechToggle, mech_toggle, MECH_TYPE_AREA)

static void
mech_toggle_default_init (MechToggleInterface *iface)
{
  signals[TOGGLED] =
    g_signal_new ("toggled",
                  MECH_TYPE_TOGGLE,
                  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MechToggleInterface, toggled),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("active",
                                                             "Active",
                                                             "Active",
                                                             FALSE,
                                                             G_PARAM_READWRITE |
                                                             G_PARAM_STATIC_STRINGS));
}

gboolean
mech_toggle_get_active (MechToggle *toggle)
{
  gboolean active;

  g_return_val_if_fail (MECH_IS_TOGGLE (toggle), FALSE);

  g_object_get (toggle, "active", &active, NULL);

  return active;
}

void
mech_toggle_set_active (MechToggle *toggle,
                        gboolean    active)
{
  g_return_if_fail (MECH_IS_TOGGLE (toggle));

  g_object_set (toggle, "active", active, NULL);
}
