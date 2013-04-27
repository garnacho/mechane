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

#include <stdlib.h>
#include <unistd.h>
#include <mechane/mech-events.h>
#include "mech-event-source-wayland.h"
#include "mech-backend-wayland.h"

struct _MechEventSourceWayland
{
  GSource source;
  GPollFD event_poll_fd;
};

static gboolean
_mech_event_source_wayland_prepare (GSource *source,
				    gint    *timeout)
{
  MechBackendWayland *backend = _mech_backend_wayland_get ();

  wl_display_flush (backend->wl_display);
  *timeout = -1;

  return FALSE;
}

static gboolean
_mech_event_source_wayland_check (GSource *source)
{
  MechEventSourceWayland *event_source = (MechEventSourceWayland *) source;

  if (event_source->event_poll_fd.revents & G_IO_NVAL)
    {
      g_critical ("Invalid request on event FD. descriptor closed underneath?");
      _exit (EXIT_FAILURE);
    }

  if (event_source->event_poll_fd.revents & (G_IO_ERR | G_IO_HUP))
    {
      g_critical ("Disconnected from wayland display");
      _exit (EXIT_FAILURE);
    }

  if (event_source->event_poll_fd.revents & G_IO_IN)
    return TRUE;

  return FALSE;
}

static gboolean
_mech_event_source_wayland_dispatch (GSource     *source,
				     GSourceFunc  callback,
				     gpointer     user_data)
{
  MechBackendWayland *backend = _mech_backend_wayland_get ();

  wl_display_dispatch(backend->wl_display);

  return TRUE;
}

static GSourceFuncs event_source_funcs = {
  _mech_event_source_wayland_prepare,
  _mech_event_source_wayland_check,
  _mech_event_source_wayland_dispatch,
  NULL
};

MechEventSourceWayland *
_mech_event_source_wayland_new (MechBackendWayland *backend)
{
  MechEventSourceWayland *event_source;
  GSource *source;

  source = g_source_new (&event_source_funcs, sizeof (MechEventSourceWayland));
  event_source = (MechEventSourceWayland *) source;

  event_source->event_poll_fd.fd = wl_display_get_fd (backend->wl_display);
  event_source->event_poll_fd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (source, &event_source->event_poll_fd);

  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, g_main_context_get_thread_default ());

  return event_source;
}
