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

#ifndef __MECH_MONITOR_WAYLAND_H__
#define __MECH_MONITOR_WAYLAND_H__

#include <mechane/mech-monitor.h>
#include <wayland-client.h>

G_BEGIN_DECLS

#define MECH_TYPE_MONITOR_WAYLAND         (mech_monitor_wayland_get_type ())
#define MECH_MONITOR_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_MONITOR_WAYLAND, MechMonitorWayland))
#define MECH_MONITOR_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_MONITOR_WAYLAND, MechMonitorWaylandClass))
#define MECH_IS_MONITOR_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_MONITOR_WAYLAND))
#define MECH_IS_MONITOR_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_MONITOR_WAYLAND))
#define MECH_MONITOR_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_MONITOR_WAYLAND, MechMonitorWaylandClass))

typedef struct _MechMonitorWayland MechMonitorWayland;
typedef struct _MechMonitorWaylandClass MechMonitorWaylandClass;
typedef struct _MechMonitorWaylandPriv MechMonitorWaylandPriv;

struct _MechMonitorWayland
{
  MechMonitor parent_instance;
  MechMonitorWaylandPriv *_priv;
};

struct _MechMonitorWaylandClass
{
  MechMonitorClass parent_class;
};

GType         mech_monitor_wayland_get_type (void) G_GNUC_CONST;

MechMonitor * mech_monitor_wayland_new      (struct wl_output *output);

G_END_DECLS

#endif /* __MECH_MONITOR_WAYLAND_H__ */
