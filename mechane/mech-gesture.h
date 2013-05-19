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

#ifndef __MECH_GESTURE_H__
#define __MECH_GESTURE_H__

#include <mechane/mech-controller.h>

G_BEGIN_DECLS

#define MECH_TYPE_GESTURE         (mech_gesture_get_type ())
#define MECH_GESTURE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_GESTURE, MechGesture))
#define MECH_GESTURE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_GESTURE, MechGestureClass))
#define MECH_IS_GESTURE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_GESTURE))
#define MECH_IS_GESTURE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_GESTURE))
#define MECH_GESTURE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_GESTURE, MechGestureClass))

typedef struct _MechGesture MechGesture;
typedef struct _MechGestureClass MechGestureClass;

struct _MechGesture
{
  MechController parent_instance;
};

struct _MechGestureClass
{
  MechControllerClass parent_class;

  gboolean (* check)  (MechGesture *gesture);

  void     (* begin)  (MechGesture *gesture);
  void     (* update) (MechGesture *gesture,
                       gint         n_point);
  void     (* end)    (MechGesture *gesture);
};

GType      mech_gesture_get_type         (void) G_GNUC_CONST;

gboolean   mech_gesture_get_id           (MechGesture       *gesture,
                                          gint               n_point,
                                          guint32           *sequence);
gboolean   mech_gesture_get_point        (MechGesture       *gesture,
                                          gint               n_point,
                                          gdouble           *x,
                                          gdouble           *y);
gboolean   mech_gesture_get_update_time  (MechGesture       *gesture,
                                          gint               n_point,
                                          guint32           *evtime);
gboolean   mech_gesture_get_bounding_box (MechGesture       *gesture,
                                          cairo_rectangle_t *rect);
gboolean   mech_gesture_is_active        (MechGesture       *gesture);


G_END_DECLS

#endif /* __MECH_GESTURE_H__ */
