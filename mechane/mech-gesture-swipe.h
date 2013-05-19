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

#ifndef __MECH_GESTURE_SWIPE_H__
#define __MECH_GESTURE_SWIPE_H__

#include <mechane/mech-gesture.h>

G_BEGIN_DECLS

#define MECH_TYPE_GESTURE_SWIPE         (mech_gesture_swipe_get_type ())
#define MECH_GESTURE_SWIPE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_GESTURE_SWIPE, MechGestureSwipe))
#define MECH_GESTURE_SWIPE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_GESTURE_SWIPE, MechGestureSwipeClass))
#define MECH_IS_GESTURE_SWIPE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_GESTURE_SWIPE))
#define MECH_IS_GESTURE_SWIPE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_GESTURE_SWIPE))
#define MECH_GESTURE_SWIPE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_GESTURE_SWIPE, MechGestureSwipeClass))

typedef struct _MechGestureSwipe MechGestureSwipe;
typedef struct _MechGestureSwipeClass MechGestureSwipeClass;

struct _MechGestureSwipe
{
  MechGesture parent_instance;
};

struct _MechGestureSwipeClass
{
  MechGestureClass parent_class;

  void (* swipe) (MechGestureSwipe *gesture,
                  gdouble           velocity_x,
                  gdouble           velocity_y);
};

GType            mech_gesture_swipe_get_type  (void) G_GNUC_CONST;

MechController * mech_gesture_swipe_new       (void);

G_END_DECLS

#endif /* __MECH_GESTURE_SWIPE_H__ */
