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

#include "mech-controller.h"

G_DEFINE_ABSTRACT_TYPE (MechController, mech_controller, G_TYPE_OBJECT)

static void
mech_controller_class_init (MechControllerClass *klass)
{
}

static void
mech_controller_init (MechController *controller)
{
}

gboolean
mech_controller_handle_event (MechController *controller,
                              MechEvent      *event)
{
  MechControllerClass *controller_class;

  g_return_val_if_fail (MECH_IS_CONTROLLER (controller), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  controller_class = MECH_CONTROLLER_GET_CLASS (controller);

  if (!controller_class->handle_event)
    return FALSE;

  return controller_class->handle_event (controller, event);
}
