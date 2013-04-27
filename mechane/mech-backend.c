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

#include <mechane/mech-backend-private.h>
#include <backends/wayland/mech-backend-wayland.h>

G_DEFINE_ABSTRACT_TYPE (MechBackend, mech_backend, G_TYPE_OBJECT)

static void
mech_backend_class_init (MechBackendClass *klass)
{
}

static void
mech_backend_init (MechBackend *backend)
{
}

MechBackend *
mech_backend_get (void)
{
  /* FIXME: make this loadable */
  return (MechBackend *) _mech_backend_wayland_get ();
}

MechWindow *
mech_backend_create_window (MechBackend *backend)
{
  g_return_val_if_fail (MECH_IS_BACKEND (backend), NULL);

  return MECH_BACKEND_GET_CLASS (backend)->create_window (backend);
}

MechSurface *
mech_backend_create_surface (MechBackend *backend)
{
  g_return_val_if_fail (MECH_IS_BACKEND (backend), NULL);

  return MECH_BACKEND_GET_CLASS (backend)->create_surface (backend);
}
