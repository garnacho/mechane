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

#ifndef __MECH_MONITOR_H__
#define __MECH_MONITOR_H__

#include <glib-object.h>
#include <mechane/mech-enums.h>

G_BEGIN_DECLS

#define MECH_TYPE_MONITOR         (mech_monitor_get_type ())
#define MECH_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_MONITOR, MechMonitor))
#define MECH_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_MONITOR, MechMonitorClass))
#define MECH_IS_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_MONITOR))
#define MECH_IS_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_MONITOR))
#define MECH_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_MONITOR, MechMonitorClass))

typedef struct _MechMonitor MechMonitor;
typedef struct _MechMonitorClass MechMonitorClass;

struct _MechMonitor
{
  GObject parent_instance;
};

struct _MechMonitorClass
{
  GObjectClass parent_class;

  void (* resolution_changed) (MechMonitor *monitor,
                               gint         width,
                               gint         height);
  void (* frequency_changed)  (MechMonitor *monitor,
                               gdouble      frequency);
};

GType         mech_monitor_get_type       (void) G_GNUC_CONST;

const gchar * mech_monitor_get_name       (MechMonitor *monitor);
gint          mech_monitor_get_frequency  (MechMonitor *monitor);

void          mech_monitor_get_extents    (MechMonitor *monitor,
                                           gint        *width,
                                           gint        *height);
void          mech_monitor_get_mm_extents (MechMonitor *monitor,
                                           gint        *width_mm,
                                           gint        *height_mm);

G_END_DECLS

#endif /* __MECH_MONITOR_H__ */
