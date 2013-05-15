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

#include <mechane/mechane.h>
#include <mechane/mech-activatable.h>

enum {
  ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE (MechActivatable, mech_activatable, MECH_TYPE_AREA)

static void
mech_activatable_default_init (MechActivatableInterface *iface)
{
  signals[ACTIVATED] =
    g_signal_new ("activated",
                  MECH_TYPE_ACTIVATABLE,
                  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MechActivatableInterface, activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

void
mech_activatable_activate (MechActivatable *activatable)
{
  g_return_if_fail (MECH_IS_ACTIVATABLE (activatable));

  g_signal_emit (activatable, signals[ACTIVATED], 0);
}
