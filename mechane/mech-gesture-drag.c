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

#include "mech-gesture-drag.h"
#include "mech-marshal.h"

typedef struct _MechGestureDragPrivate MechGestureDragPrivate;

enum {
  PROP_MINIMAL_DISTANCE = 1
};

enum {
  DRAGGED,
  DRAG_END,
  N_SIGNALS
};

struct _MechGestureDragPrivate
{
  gdouble minimal_distance;
  gdouble start_x;
  gdouble start_y;
  guint has_start_point;
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechGestureDrag, mech_gesture_drag, MECH_TYPE_GESTURE)

static void
mech_gesture_drag_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MechGestureDragPrivate *priv;

  priv = mech_gesture_drag_get_instance_private ((MechGestureDrag *) object);

  switch (prop_id)
    {
    case PROP_MINIMAL_DISTANCE:
      g_value_set_double (value, priv->minimal_distance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_gesture_drag_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MechGestureDragPrivate *priv;

  priv = mech_gesture_drag_get_instance_private ((MechGestureDrag *) object);

  switch (prop_id)
    {
    case PROP_MINIMAL_DISTANCE:
      priv->minimal_distance = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
_mech_gesture_drag_get_hotspot (MechGestureDrag *gesture,
                                gdouble         *hotspot_x,
                                gdouble         *hotspot_y)
{
  cairo_rectangle_t rect;

  if (!mech_gesture_get_bounding_box (MECH_GESTURE (gesture), &rect))
    return FALSE;

  *hotspot_x = rect.x + (rect.width / 2);
  *hotspot_y = rect.y + (rect.height / 2);

  return TRUE;
}

static void
mech_gesture_drag_update (MechGesture *gesture,
                          gint         n_point)
{
  MechGestureDragPrivate *priv;
  gdouble hotspot_x, hotspot_y;

  priv = mech_gesture_drag_get_instance_private ((MechGestureDrag *) gesture);

  if (!priv->has_start_point)
    return;

  if (!_mech_gesture_drag_get_hotspot (MECH_GESTURE_DRAG (gesture),
                                       &hotspot_x, &hotspot_y))
    return;

  g_signal_emit (gesture, signals[DRAGGED], 0,
                 hotspot_x - priv->start_x,
                 hotspot_y - priv->start_y);
}

static void
mech_gesture_drag_end (MechGesture *gesture)
{
  MechGestureDragPrivate *priv;
  gdouble hotspot_x, hotspot_y;

  if (!_mech_gesture_drag_get_hotspot (MECH_GESTURE_DRAG (gesture),
                                       &hotspot_x, &hotspot_y))
    return;

  priv = mech_gesture_drag_get_instance_private ((MechGestureDrag *) gesture);
  g_signal_emit (gesture, signals[DRAG_END], 0,
                 hotspot_x - priv->start_x,
                 hotspot_y - priv->start_y);

  priv->has_start_point = FALSE;
}

static gboolean
mech_gesture_drag_check (MechGesture *gesture)
{
  MechGestureDragPrivate *priv;
  gdouble hotspot_x, hotspot_y;

  if (!_mech_gesture_drag_get_hotspot (MECH_GESTURE_DRAG (gesture),
                                       &hotspot_x, &hotspot_y))
    return FALSE;

  priv = mech_gesture_drag_get_instance_private ((MechGestureDrag *) gesture);

  if (!priv->has_start_point)
    {
      priv->has_start_point = TRUE;
      priv->start_x = hotspot_x;
      priv->start_y = hotspot_y;
    }

  if (priv->has_start_point &&
      (ABS (priv->start_x - hotspot_x) >= priv->minimal_distance ||
       ABS (priv->start_y - hotspot_y) >= priv->minimal_distance))
    return TRUE;

  return FALSE;
}

static gboolean
mech_gesture_drag_handle_event (MechController *controller,
                                MechEvent      *event)
{
  MechGestureDragPrivate *priv;

  priv = mech_gesture_drag_get_instance_private ((MechGestureDrag *) controller);

  if (priv->has_start_point &&
      (event->type == MECH_TOUCH_UP ||
       event->type == MECH_BUTTON_RELEASE) &&
      !mech_gesture_drag_check (MECH_GESTURE (controller)))
    priv->has_start_point = FALSE;

  return MECH_CONTROLLER_CLASS (mech_gesture_drag_parent_class)->handle_event (controller, event);
}

static void
mech_gesture_drag_class_init (MechGestureDragClass *klass)
{
  MechControllerClass *controller_class = MECH_CONTROLLER_CLASS (klass);
  MechGestureClass *gesture_class = MECH_GESTURE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mech_gesture_drag_get_property;
  object_class->set_property = mech_gesture_drag_set_property;

  controller_class->handle_event = mech_gesture_drag_handle_event;

  gesture_class->update = mech_gesture_drag_update;
  gesture_class->end = mech_gesture_drag_end;
  gesture_class->check = mech_gesture_drag_check;

  g_object_class_install_property (object_class,
                                   PROP_MINIMAL_DISTANCE,
                                   g_param_spec_double ("minimal-distance",
                                                        "Minimal distance",
                                                        "Minimal distance at which "
                                                        "the gesture is triggered",
                                                        0, G_MAXDOUBLE, 1,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  signals[DRAGGED] =
    g_signal_new ("dragged",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechGestureDragClass, dragged),
                  NULL, NULL,
                  _mech_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  signals[DRAG_END] =
    g_signal_new ("drag-end",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechGestureDragClass, drag_end),
                  NULL, NULL,
                  _mech_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}

static void
mech_gesture_drag_init (MechGestureDrag *gesture)
{
}

MechController *
mech_gesture_drag_new (void)
{
  return g_object_new (MECH_TYPE_GESTURE_DRAG, NULL);
}
