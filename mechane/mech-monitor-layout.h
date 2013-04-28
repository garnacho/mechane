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

#ifndef __MECH_MONITOR_LAYOUT_H__
#define __MECH_MONITOR_LAYOUT_H__

#include <mechane/mech-monitor.h>

G_BEGIN_DECLS

#define MECH_TYPE_MONITOR_LAYOUT         (mech_monitor_layout_get_type ())
#define MECH_MONITOR_LAYOUT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_MONITOR_LAYOUT, MechMonitorLayout))
#define MECH_MONITOR_LAYOUT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_MONITOR_LAYOUT, MechMonitorLayoutClass))
#define MECH_IS_MONITOR_LAYOUT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_MONITOR_LAYOUT))
#define MECH_IS_MONITOR_LAYOUT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_MONITOR_LAYOUT))
#define MECH_MONITOR_LAYOUT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_MONITOR_LAYOUT, MechMonitorLayoutClass))

typedef struct _MechMonitorLayout MechMonitorLayout;
typedef struct _MechMonitorLayoutClass MechMonitorLayoutClass;

struct _MechMonitorLayout
{
  GObject parent_instance;
};

struct _MechMonitorLayoutClass
{
  GObjectClass parent_class;

  void (* monitor_added)   (MechMonitorLayout *layout,
                            MechMonitor       *monitor);
  void (* monitor_removed) (MechMonitorLayout *layout,
                            MechMonitor       *monitor);
};

GType               mech_monitor_layout_get_type     (void) G_GNUC_CONST;

MechMonitorLayout * mech_monitor_layout_get         (void);
MechMonitor       * mech_monitor_layout_get_primary (MechMonitorLayout *manager);
GList             * mech_monitor_layout_list        (MechMonitorLayout *manager);

G_END_DECLS

#endif /* __MECH_MONITOR_LAYOUT_H__ */
