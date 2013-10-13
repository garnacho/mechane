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

#include <cairo/cairo-xlib.h>
#include <mechane/mech-window-private.h>
#include "mech-window-wayland.h"
#include "mech-surface-wayland.h"
#include "mech-clock-wayland.h"
#include "mech-backend-wayland.h"

struct _MechWindowWaylandPriv
{
  struct wl_compositor *wl_compositor;
  struct wl_shell *wl_shell;
  struct wl_surface *wl_surface;
  struct wl_shell_surface *wl_shell_surface;
  MechSurface *surface;
  GSList *outputs;
};

enum {
  PROP_WL_COMPOSITOR = 1,
  PROP_WL_SHELL,
  PROP_WL_SURFACE,
  PROP_WL_SHELL_SURFACE,
};

G_DEFINE_TYPE (MechWindowWayland, mech_window_wayland, MECH_TYPE_WINDOW)

static void
mech_window_wayland_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  MechWindowWaylandPriv *priv = ((MechWindowWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_COMPOSITOR:
      priv->wl_compositor = g_value_get_pointer (value);
      break;
    case PROP_WL_SHELL:
      priv->wl_shell = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_window_wayland_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  MechWindowWaylandPriv *priv = ((MechWindowWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_COMPOSITOR:
      g_value_set_pointer (value, priv->wl_compositor);
      break;
    case PROP_WL_SHELL:
      g_value_set_pointer (value, priv->wl_shell);
      break;
    case PROP_WL_SURFACE:
      g_value_set_pointer (value, priv->wl_surface);
      break;
    case PROP_WL_SHELL_SURFACE:
      g_value_set_pointer (value, priv->wl_shell_surface);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_window_wayland_finalize (GObject *object)
{
  MechWindowWaylandPriv *priv = ((MechWindowWayland *) object)->_priv;

  wl_surface_destroy (priv->wl_surface);
  wl_shell_surface_destroy (priv->wl_shell_surface);

  G_OBJECT_CLASS (mech_window_wayland_parent_class)->finalize (object);
}

static void
_window_surface_enter (gpointer           data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *wl_output)
{
  MechWindowWaylandPriv *priv;
  MechBackendWayland *backend;
  MechWindow *window = data;
  MechMonitor *monitor;

  priv = ((MechWindowWayland *) window)->_priv;
  backend = _mech_backend_wayland_get ();
  monitor = _mech_backend_wayland_lookup_output (backend, wl_output);
  g_assert (monitor != NULL);

  priv->outputs = g_slist_prepend (priv->outputs, monitor);

  if (!mech_window_get_monitor (window))
    _mech_window_set_monitor (window, monitor);
}

static void
_window_surface_leave (gpointer           data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *wl_output)
{
  MechWindowWaylandPriv *priv;
  MechBackendWayland *backend;
  MechWindow *window = data;
  MechMonitor *monitor;

  priv = ((MechWindowWayland *) window)->_priv;
  backend = _mech_backend_wayland_get ();
  monitor = _mech_backend_wayland_lookup_output (backend, wl_output);
  g_assert (monitor != NULL);

  priv->outputs = g_slist_remove (priv->outputs, monitor);

  if (monitor == mech_window_get_monitor (window))
    _mech_window_set_monitor (window,
                              priv->outputs ? priv->outputs->data : NULL);
}

static const struct wl_surface_listener surface_listener_funcs = {
  _window_surface_enter,
  _window_surface_leave
};

static void
_window_shell_surface_ping (gpointer                 data,
                            struct wl_shell_surface *wl_shell_surface,
                            guint32                  serial)
{
  wl_shell_surface_pong (wl_shell_surface, serial);
}

static void
_window_shell_surface_configure (gpointer                 data,
                                 struct wl_shell_surface *wl_shell_surface,
                                 guint32                  edges,
                                 gint32                   width,
                                 gint32                   height)
{
  gint prev_width, prev_height;
  MechWindowWaylandPriv *priv;
  MechWindow *window = data;
  gint tx, ty;

  priv = ((MechWindowWayland *) window)->_priv;
  mech_container_get_size ((MechContainer *) window, &prev_width, &prev_height);
  mech_container_queue_resize ((MechContainer *) window, width, height);
  tx = ty = 0;

  /* Fetch again the size after applying minimal requisition */
  mech_container_get_size ((MechContainer *) window, &width, &height);

  if (edges == WL_SHELL_SURFACE_RESIZE_LEFT ||
      edges == WL_SHELL_SURFACE_RESIZE_TOP_LEFT ||
      edges == WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT)
    tx = prev_width - width;

  if (edges == WL_SHELL_SURFACE_RESIZE_TOP ||
      edges == WL_SHELL_SURFACE_RESIZE_TOP_LEFT ||
      edges == WL_SHELL_SURFACE_RESIZE_TOP_RIGHT)
    ty = prev_height - height;

  _mech_surface_wayland_translate ((MechSurfaceWayland *) priv->surface,
                                   tx, ty);
}

static void
_window_shell_surface_popup_done (gpointer data,
                                  struct wl_shell_surface *wl_shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener_funcs = {
  _window_shell_surface_ping,
  _window_shell_surface_configure,
  _window_shell_surface_popup_done
};

static void
mech_window_wayland_constructed (GObject *object)
{
  MechWindowWayland *window = (MechWindowWayland *) object;
  MechWindowWaylandPriv *priv = window->_priv;
  MechClock *clock;

  priv->wl_surface = wl_compositor_create_surface (priv->wl_compositor);
  wl_surface_add_listener (priv->wl_surface, &surface_listener_funcs, window);

  priv->wl_shell_surface = wl_shell_get_shell_surface (priv->wl_shell,
                                                       priv->wl_surface);
  wl_shell_surface_add_listener (priv->wl_shell_surface,
                                 &shell_surface_listener_funcs,
                                 window);
  wl_shell_surface_set_toplevel (priv->wl_shell_surface);

  clock = _mech_clock_wayland_new (window, priv->wl_surface);
  _mech_window_set_clock ((MechWindow *) object, clock);

  G_OBJECT_CLASS (mech_window_wayland_parent_class)->constructed (object);
}

static void
mech_window_wayland_set_visible (MechWindow *window,
                                 gboolean    visible)
{
  MechWindowWayland *window_wayland = (MechWindowWayland *) window;
  MechWindowWaylandPriv *priv = window_wayland->_priv;

  if (visible)
    {
      if (!priv->surface)
        priv->surface = _mech_surface_wayland_new (MECH_BACKING_SURFACE_TYPE_EGL,
                                                   priv->wl_surface);
      _mech_container_set_surface ((MechContainer *) window, priv->surface);
    }
  else
    {
      wl_surface_attach (priv->wl_surface, NULL, 0 , 0);
      wl_surface_commit (priv->wl_surface);
    }
}

static void
mech_window_wayland_set_title (MechWindow  *window,
			       const gchar *title)
{
  MechWindowWaylandPriv *priv = ((MechWindowWayland *) window)->_priv;

  wl_shell_surface_set_title (priv->wl_shell_surface, title);
}

static gboolean
mech_window_wayland_move (MechWindow *window,
                          MechEvent  *event)
{
  MechWindowWaylandPriv *priv = ((MechWindowWayland *) window)->_priv;
  struct wl_seat *wl_seat;

  if (event->any.serial == 0)
    {
      g_warning ("Window '%s' not moved. The event doesn't have a valid serial",
                 mech_window_get_title (window));
      return FALSE;
    }

  g_object_get ((GObject *) event->any.seat, "wl-seat", &wl_seat, NULL);
  wl_shell_surface_move (priv->wl_shell_surface, wl_seat, event->any.serial);

  return TRUE;
}

static gboolean
mech_window_wayland_resize (MechWindow    *window,
                            MechEvent     *event,
                            MechSideFlags  side)
{
  MechWindowWaylandPriv *priv = ((MechWindowWayland *) window)->_priv;
  struct wl_seat *wl_seat;
  guint resize_mode;

  if (event->any.serial == 0)
    {
      g_warning ("Window '%s' not resized. The event doesn't have a valid serial",
                 mech_window_get_title (window));
      return FALSE;
    }

  if (side == MECH_SIDE_FLAG_LEFT)
    resize_mode = WL_SHELL_SURFACE_RESIZE_LEFT;
  else if (side == MECH_SIDE_FLAG_RIGHT)
    resize_mode = WL_SHELL_SURFACE_RESIZE_RIGHT;
  else if (side == MECH_SIDE_FLAG_TOP)
    resize_mode = WL_SHELL_SURFACE_RESIZE_TOP;
  else if (side == MECH_SIDE_FLAG_BOTTOM)
    resize_mode = WL_SHELL_SURFACE_RESIZE_BOTTOM;
  else if (side == MECH_SIDE_FLAG_CORNER_TOP_LEFT)
    resize_mode = WL_SHELL_SURFACE_RESIZE_TOP_LEFT;
  else if (side == MECH_SIDE_FLAG_CORNER_TOP_RIGHT)
    resize_mode = WL_SHELL_SURFACE_RESIZE_TOP_RIGHT;
  else if (side == MECH_SIDE_FLAG_CORNER_BOTTOM_LEFT)
    resize_mode = WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT;
  else if (side == MECH_SIDE_FLAG_CORNER_BOTTOM_RIGHT)
    resize_mode = WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT;
  else
    return FALSE;

  g_object_get ((GObject *) event->any.seat, "wl-seat", &wl_seat, NULL);
  wl_shell_surface_resize (priv->wl_shell_surface, wl_seat,
                           event->any.serial, resize_mode);
  return TRUE;
}

static void
mech_window_wayland_apply_state (MechWindow      *window,
                                 MechWindowState  state,
                                 MechMonitor     *monitor)
{
  MechWindowWaylandPriv *priv = ((MechWindowWayland *) window)->_priv;
  struct wl_output *wl_output;

  if (state == MECH_WINDOW_STATE_NORMAL)
    wl_shell_surface_set_toplevel (priv->wl_shell_surface);
  else
    {
      if (!monitor)
        monitor = mech_window_get_monitor (window);

      g_object_get (monitor, "wl-output", &wl_output, NULL);

      if (state == MECH_WINDOW_STATE_MAXIMIZED)
        wl_shell_surface_set_maximized (priv->wl_shell_surface, wl_output);
      else if (state == MECH_WINDOW_STATE_FULLSCREEN)
        wl_shell_surface_set_fullscreen (priv->wl_shell_surface,
                                         WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                         mech_monitor_get_frequency (monitor) * 1000,
                                         wl_output);
    }
}

static void
mech_window_wayland_class_init (MechWindowWaylandClass *klass)
{
  MechWindowClass *window_class = MECH_WINDOW_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_window_wayland_set_property;
  object_class->get_property = mech_window_wayland_get_property;
  object_class->constructed = mech_window_wayland_constructed;
  object_class->finalize = mech_window_wayland_finalize;

  window_class->set_visible = mech_window_wayland_set_visible;
  window_class->set_title = mech_window_wayland_set_title;
  window_class->move = mech_window_wayland_move;
  window_class->resize = mech_window_wayland_resize;
  window_class->apply_state = mech_window_wayland_apply_state;

  g_object_class_install_property (object_class,
                                   PROP_WL_COMPOSITOR,
                                   g_param_spec_pointer ("wl-compositor",
                                                         "wl-compositor",
                                                         "wl-compositor",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_WL_SHELL,
                                   g_param_spec_pointer ("wl-shell",
                                                         "wl-shell",
                                                         "wl-shell",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_WL_SURFACE,
                                   g_param_spec_pointer ("wl-surface",
                                                         "wl-surface",
                                                         "wl-surface",
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_WL_SHELL_SURFACE,
                                   g_param_spec_pointer ("wl-shell-surface",
                                                         "wl-shell-surface",
                                                         "wl-shell-surface",
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (MechWindowWaylandPriv));
}

static void
mech_window_wayland_init (MechWindowWayland *window)
{
  window->_priv = G_TYPE_INSTANCE_GET_PRIVATE (window,
                                               MECH_TYPE_WINDOW_WAYLAND,
                                               MechWindowWaylandPriv);
}

MechWindow *
_mech_window_wayland_new (struct wl_compositor *compositor)
{
  MechWindowWayland *window;

  window = g_object_new (MECH_TYPE_WINDOW_WAYLAND, NULL);

  return (MechWindow *) window;
}
