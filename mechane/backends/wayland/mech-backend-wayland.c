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
#include <string.h>
#include <mechane/mech-monitor-layout-private.h>
#include "mech-backend-wayland.h"
#include "mech-event-source-wayland.h"
#include "mech-monitor-wayland.h"
#include "mech-window-wayland.h"
#include "mech-surface-wayland.h"
#include "mech-seat-wayland.h"

G_DEFINE_TYPE (MechBackendWayland, _mech_backend_wayland, MECH_TYPE_BACKEND)

struct _MechBackendWaylandPriv
{
  MechEventSourceWayland *event_source;
  MechSeat *seat;
  GHashTable *windows;
  GHashTable *outputs;
};

static MechWindow *
_mech_backend_wayland_create_window (MechBackend *backend)
{
  MechBackendWayland *backend_wayland = (MechBackendWayland *) backend;
  struct wl_surface *wl_surface;
  MechWindow *window;

  window = g_object_new (MECH_TYPE_WINDOW_WAYLAND,
                         "wl-compositor", backend_wayland->wl_compositor,
                         "wl-shell", backend_wayland->wl_shell,
                         NULL);

  g_object_get (window, "wl-surface", &wl_surface, NULL);
  g_hash_table_insert (backend_wayland->_priv->windows,
                       wl_surface, window);

  return window;
}

static MechSurface *
_mech_backend_wayland_create_surface (MechBackend *backend)
{
  return _mech_surface_wayland_new (MECH_BACKING_SURFACE_TYPE_SHM, NULL);
}

static void
_mech_backend_wayland_class_init (MechBackendWaylandClass *klass)
{
  MechBackendClass *backend_class = MECH_BACKEND_CLASS (klass);

  backend_class->create_window = _mech_backend_wayland_create_window;
  backend_class->create_surface = _mech_backend_wayland_create_surface;

  g_type_class_add_private (klass, sizeof (MechBackendWaylandPriv));
}

static void
_backend_registry_handle_global (gpointer            user_data,
                                 struct wl_registry *registry,
                                 guint32             id,
                                 const char         *interface,
                                 guint32             version)
{
  MechBackendWayland *backend = user_data;

  if (strcmp (interface, "wl_compositor") == 0)
    backend->wl_compositor =
      wl_registry_bind (registry, id, &wl_compositor_interface, 1);
  else if (strcmp (interface, "wl_shm") == 0)
    backend->wl_shm = wl_registry_bind (registry, id, &wl_shm_interface, 1);
  else if (strcmp (interface, "wl_shell") == 0)
    backend->wl_shell = wl_registry_bind (registry, id, &wl_shell_interface, 1);
  else if (strcmp (interface, "wl_seat") == 0)
    {
      struct wl_seat *wl_seat;

      wl_seat = wl_registry_bind (registry, id, &wl_seat_interface, 1);
      backend->_priv->seat = mech_seat_wayland_new (wl_seat);
    }
  else if (strcmp (interface, "wl_output") == 0)
    {
      struct wl_output *wl_output;
      MechMonitorLayout *layout;
      MechMonitor *monitor;

      wl_output = wl_registry_bind (registry, id, &wl_output_interface, 1);
      monitor = mech_monitor_wayland_new (wl_output);

      layout = mech_monitor_layout_get ();
      _mech_monitor_layout_add (layout, monitor);
      g_hash_table_insert (backend->_priv->outputs, wl_output, monitor);
    }
}

static void
_backend_registry_handle_global_remove (gpointer            user_data,
                                        struct wl_registry *registry,
                                        guint32             name)
{
  /* FIXME */
}

static const struct wl_registry_listener registry_listener_funcs = {
  _backend_registry_handle_global,
  _backend_registry_handle_global_remove
};

static void
_mech_backend_wayland_init (MechBackendWayland *backend)
{
  MechBackendWaylandPriv *priv;
  const gchar *display;

  backend->_priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (backend,
                                 MECH_TYPE_BACKEND_WAYLAND,
                                 MechBackendWaylandPriv);
  display = g_getenv ("WAYLAND_DISPLAY");

  if (!display || !*display)
    {
      g_critical ("No WAYLAND_DISPLAY environment variable.");
      _exit (EXIT_FAILURE);
    }

  backend->wl_display = wl_display_connect (display);

  if (!backend->wl_display)
    {
      g_critical ("Could not open display '%s'", display);
      _exit (EXIT_FAILURE);
    }

  priv->event_source = _mech_event_source_wayland_new (backend);
  priv->windows = g_hash_table_new (NULL, NULL);
  priv->outputs = g_hash_table_new (NULL, NULL);

  backend->wl_registry =
    wl_display_get_registry (backend->wl_display);
  wl_registry_add_listener (backend->wl_registry,
                            &registry_listener_funcs,
                            backend);

  if (wl_display_dispatch (backend->wl_display) < 0)
    {
      g_warning ("Failed to process Wayland connection");
      _exit (EXIT_FAILURE);
    }
}

MechBackendWayland *
_mech_backend_wayland_get (void)
{
  static MechBackendWayland *backend = NULL;

  if (g_once_init_enter (&backend))
    {
      MechBackendWayland *object;

      object = g_object_new (MECH_TYPE_BACKEND_WAYLAND, NULL);
      g_once_init_leave (&backend, object);
    }

  return backend;
}

MechWindow *
_mech_backend_wayland_lookup_window (MechBackendWayland *backend,
                                     struct wl_surface  *wl_surface)
{
  return g_hash_table_lookup (backend->_priv->windows, wl_surface);
}

MechMonitor *
_mech_backend_wayland_lookup_output (MechBackendWayland *backend,
                                     struct wl_output   *wl_output)
{
  return g_hash_table_lookup (backend->_priv->outputs, wl_output);
}
