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

#ifndef __MECHANE_H__
#define __MECHANE_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* Base */
#include <mechane/mech-cursor.h>
#include <mechane/mech-seat.h>
#include <mechane/mech-enum-types.h>
#include <mechane/mech-enums.h>
#include <mechane/mech-events.h>
#include <mechane/mech-monitor.h>
#include <mechane/mech-monitor-layout.h>
#include <mechane/mech-theme.h>
#include <mechane/mech-renderer.h>
#include <mechane/mech-area.h>
#include <mechane/mech-window.h>

/* Basic interfaces */
#include <mechane/mech-activatable.h>
#include <mechane/mech-orientable.h>
#include <mechane/mech-toggle.h>

/* Animations */
#include <mechane/mech-animation.h>

/* Basic areas */
#include <mechane/mech-fixed-box.h>
#include <mechane/mech-floating-box.h>
#include <mechane/mech-linear-box.h>
#include <mechane/mech-button.h>
#include <mechane/mech-toggle-button.h>


G_END_DECLS

#endif /* __MECHANE_H__ */
