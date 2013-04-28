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

#ifndef __MECH_MONITOR_LAYOUT_PRIVATE_H__
#define __MECH_MONITOR_LAYOUT_PRIVATE_H__

#include <mechane/mech-monitor-layout.h>
#include <mechane/mech-monitor.h>

G_BEGIN_DECLS

void        _mech_monitor_layout_add      (MechMonitorLayout *layout,
                                           MechMonitor       *monitor);
void        _mech_monitor_layout_remove   (MechMonitorLayout *layout,
                                           MechMonitor       *monitor);
void        _mech_monitor_layout_relocate (MechMonitorLayout *layout,
                                           MechMonitor       *monitor,
                                           gint               x,
                                           gint               y);
G_END_DECLS

#endif /* __MECH_MONITOR_LAYOUT_PRIVATE_H__ */
