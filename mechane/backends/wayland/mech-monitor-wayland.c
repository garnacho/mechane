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

#include "mech-monitor-wayland.h"
#include <mechane/mech-monitor-private.h>
#include <mechane/mech-monitor-layout-private.h>

G_DEFINE_TYPE (MechMonitorWayland, mech_monitor_wayland, MECH_TYPE_MONITOR)

enum {
  PROP_WL_OUTPUT = 1
};

struct _MechMonitorWaylandPriv
{
  struct wl_output *wl_output;
};

static void
mech_monitor_wayland_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MechMonitorWaylandPriv *priv = ((MechMonitorWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_OUTPUT:
      priv->wl_output = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_monitor_wayland_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MechMonitorWaylandPriv *priv = ((MechMonitorWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_OUTPUT:
      g_value_set_pointer (value, priv->wl_output);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_monitor_output_geometry (gpointer          data,
                          struct wl_output *wl_output,
                          gint32            x,
                          gint32            y,
                          gint32            physical_width,
                          gint32            physical_height,
                          gint32            subpixel,
                          const char       *make,
                          const char       *model,
                          gint32            transform)
{
  MechMonitorLayout *layout;
  MechMonitor *monitor = data;

  _mech_monitor_set_physical_size (monitor, physical_width, physical_height);

  layout = mech_monitor_layout_get ();
  _mech_monitor_layout_relocate (layout, monitor, x, y);
}

static void
_monitor_output_mode (gpointer          data,
                      struct wl_output *wl_output,
                      guint32           flags,
                      gint32            width,
                      gint32            height,
                      gint32            refresh)
{
  MechMonitor *monitor = data;

  _mech_monitor_set_mode (monitor, width, height, refresh / 1000);
}

static const struct wl_output_listener output_listener_funcs = {
  _monitor_output_geometry,
  _monitor_output_mode
};

static void
mech_monitor_wayland_constructed (GObject *object)
{
  MechMonitorWaylandPriv *priv = ((MechMonitorWayland *) object)->_priv;

  wl_output_add_listener (priv->wl_output, &output_listener_funcs, object);

  G_OBJECT_CLASS (mech_monitor_wayland_parent_class)->constructed (object);
}

static void
mech_monitor_wayland_finalize (GObject *object)
{
  MechMonitorWaylandPriv *priv = ((MechMonitorWayland *) object)->_priv;

  if (priv->wl_output)
    wl_output_destroy (priv->wl_output);

  G_OBJECT_CLASS (mech_monitor_wayland_parent_class)->finalize (object);
}

static void
mech_monitor_wayland_class_init (MechMonitorWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_monitor_wayland_set_property;
  object_class->get_property = mech_monitor_wayland_get_property;
  object_class->constructed = mech_monitor_wayland_constructed;
  object_class->finalize = mech_monitor_wayland_finalize;

  g_object_class_install_property (object_class,
                                   PROP_WL_OUTPUT,
                                   g_param_spec_pointer ("wl-output",
                                                         "wl-output",
                                                         "wl-output",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (MechMonitorWaylandPriv));
}

static void
mech_monitor_wayland_init (MechMonitorWayland *monitor)
{
  monitor->_priv = G_TYPE_INSTANCE_GET_PRIVATE (monitor,
                                                MECH_TYPE_MONITOR_WAYLAND,
                                                MechMonitorWaylandPriv);
}

MechMonitor *
mech_monitor_wayland_new (struct wl_output *wl_output)
{
  return g_object_new (MECH_TYPE_MONITOR_WAYLAND,
                       "wl-output", wl_output,
                       NULL);
}
