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

#ifndef __MECH_ENUMS_H__
#define __MECH_ENUMS_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  MECH_SIDE_LEFT = 0,
  MECH_SIDE_RIGHT,
  MECH_SIDE_TOP,
  MECH_SIDE_BOTTOM
} MechSide;

typedef enum {
  MECH_SIDE_FLAG_LEFT   = 1 << MECH_SIDE_LEFT,
  MECH_SIDE_FLAG_RIGHT  = 1 << MECH_SIDE_RIGHT,
  MECH_SIDE_FLAG_TOP    = 1 << MECH_SIDE_TOP,
  MECH_SIDE_FLAG_BOTTOM = 1 << MECH_SIDE_BOTTOM,
  MECH_SIDE_FLAG_CORNER_TOP_LEFT     = (MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_TOP),
  MECH_SIDE_FLAG_CORNER_TOP_RIGHT    = (MECH_SIDE_FLAG_RIGHT | MECH_SIDE_FLAG_TOP),
  MECH_SIDE_FLAG_CORNER_BOTTOM_LEFT  = (MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_BOTTOM),
  MECH_SIDE_FLAG_CORNER_BOTTOM_RIGHT = (MECH_SIDE_FLAG_RIGHT | MECH_SIDE_FLAG_BOTTOM),
  MECH_SIDE_FLAG_ALL    = (MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_RIGHT | MECH_SIDE_FLAG_TOP | MECH_SIDE_FLAG_BOTTOM)
} MechSideFlags;

typedef enum {
  MECH_FOCUS_IN       = 1,
  MECH_FOCUS_OUT      = 2,
  MECH_ENTER          = 3,
  MECH_LEAVE          = 4,
  MECH_KEY_PRESS      = 5,
  MECH_KEY_RELEASE    = 6,
  MECH_MOTION         = 7,
  MECH_BUTTON_PRESS   = 8,
  MECH_BUTTON_RELEASE = 9,
  MECH_SCROLL         = 10,
  MECH_TOUCH_DOWN     = 11,
  MECH_TOUCH_MOTION   = 12,
  MECH_TOUCH_UP       = 13
} MechEventType;

typedef enum {
  MECH_NONE           = 0,
  MECH_KEY_MASK       = 1 << MECH_KEY_PRESS,
  MECH_FOCUS_MASK     = 1 << MECH_FOCUS_IN,
  MECH_MOTION_MASK    = 1 << MECH_MOTION,
  MECH_BUTTON_MASK    = 1 << MECH_BUTTON_PRESS,
  MECH_CROSSING_MASK  = 1 << MECH_ENTER,
  MECH_SCROLL_MASK    = 1 << MECH_SCROLL,
  MECH_TOUCH_MASK     = 1 << MECH_TOUCH_DOWN
} MechEventMask;

typedef enum {
  MECH_AXIS_X,
  MECH_AXIS_Y
} MechAxis;

typedef enum {
  MECH_WINDOW_STATE_NORMAL,
  MECH_WINDOW_STATE_FULLSCREEN,
  MECH_WINDOW_STATE_MAXIMIZED
} MechWindowState;

G_END_DECLS

#endif /* __MECH_ENUMS_H__ */
