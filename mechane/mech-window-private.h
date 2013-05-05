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

#ifndef __MECH_WINDOW_PRIVATE_H__
#define __MECH_WINDOW_PRIVATE_H__

#include <mechane/mech-clock-private.h>
#include <mechane/mech-stage-private.h>
#include <mechane/mech-window.h>

G_BEGIN_DECLS

void         _mech_window_set_surface        (MechWindow  *window,
                                              MechSurface *surface);
void         _mech_window_grab_focus         (MechWindow  *window,
                                              MechArea    *area,
                                              MechSeat    *seat);
void         _mech_window_set_clock          (MechWindow  *window,
					      MechClock   *clock);
MechClock  * _mech_window_get_clock          (MechWindow  *window);
MechStage  * _mech_window_get_stage          (MechWindow  *window);
void         _mech_window_set_monitor        (MechWindow  *window,
                                              MechMonitor *monitor);
MechCursor * _mech_window_get_current_cursor (MechWindow  *window);

G_END_DECLS

#endif /* __MECH_WINDOW_PRIVATE_H__ */
