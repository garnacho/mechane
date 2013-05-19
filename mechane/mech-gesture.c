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

#include "mech-gesture.h"

typedef struct _MechGesturePrivate MechGesturePrivate;
typedef struct _PointData PointData;

enum {
  PROP_N_POINTS = 1
};

enum {
  BEGIN,
  END,
  UPDATE,
  N_SIGNALS
};

struct _PointData
{
  MechPoint point;
  guint32 evtime;
  gint32 id;
};

struct _MechGesturePrivate
{
  GArray *points;
  gint n_points;
  guint recognized : 1;
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechGesture, mech_gesture,
                                     MECH_TYPE_CONTROLLER)

static void
mech_gesture_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MechGesturePrivate *priv;

  priv = mech_gesture_get_instance_private ((MechGesture *) object);

  switch (prop_id)
    {
    case PROP_N_POINTS:
      g_value_set_int (value, priv->n_points);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_gesture_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  MechGesturePrivate *priv;

  priv = mech_gesture_get_instance_private ((MechGesture *) object);

  switch (prop_id)
    {
    case PROP_N_POINTS:
      priv->n_points = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_gesture_constructed (GObject *object)
{
  MechGesturePrivate *priv;

  priv = mech_gesture_get_instance_private ((MechGesture *) object);
  priv->points = g_array_new (FALSE, FALSE, sizeof (PointData));
}

static void
mech_gesture_finalize (GObject *object)
{
  MechGesturePrivate *priv;

  priv = mech_gesture_get_instance_private ((MechGesture *) object);
  g_array_free (priv->points, TRUE);

  G_OBJECT_CLASS (mech_gesture_parent_class)->finalize (object);
}

static PointData *
_mech_gesture_find_point (MechGesture *gesture,
                          gint32       id,
                          guint       *pos)
{
  MechGesturePrivate *priv;
  guint i;

  priv = mech_gesture_get_instance_private (gesture);

  for (i = 0; i < priv->points->len; i++)
    {
      PointData *data;

      data = &g_array_index (priv->points, PointData, i);

      if (data->id == id)
        {
          if (pos)
            *pos = i;

          return data;
        }
    }

  return NULL;
}

static void
_mech_gesture_set_recognized (MechGesture *gesture,
                              gboolean     recognized)
{
  MechGesturePrivate *priv;

  priv = mech_gesture_get_instance_private (gesture);

  if (priv->recognized == recognized)
    return;

  priv->recognized = recognized;

  if (recognized)
    g_signal_emit (gesture, signals[BEGIN], 0);
  else
    g_signal_emit (gesture, signals[END], 0);
}

static void
_mech_gesture_check_recognized (MechGesture *gesture)
{
  MechGestureClass *gesture_class = MECH_GESTURE_GET_CLASS (gesture);
  MechGesturePrivate *priv;

  priv = mech_gesture_get_instance_private (gesture);

  if (priv->points->len != priv->n_points && priv->recognized)
    _mech_gesture_set_recognized (gesture, FALSE);
  else if (!priv->recognized &&
           priv->points->len == priv->n_points &&
           gesture_class->check (gesture))
    _mech_gesture_set_recognized (gesture, TRUE);
}

static gboolean
_mech_gesture_update_point (MechGesture *gesture,
                            MechEvent   *event,
                            gboolean     add,
                            gint        *position)
{
  MechGesturePrivate *priv;
  PointData *data, new;
  guint32 evtime;
  gdouble x, y;
  guint pos;
  gint id;

  if (!mech_event_pointer_get_coords (event, &x, &y))
    return FALSE;
  if (!mech_event_input_get_time (event, &evtime))
    return FALSE;

  priv = mech_gesture_get_instance_private (gesture);

  if (!mech_event_touch_get_id (event, &id))
    id = 0;

  data = _mech_gesture_find_point (gesture, id, &pos);

  if (data)
    {
      data->point.x = x;
      data->point.y = y;
      data->evtime = evtime;

      if (position)
        *position = pos;

      return TRUE;
    }
  else if (add)
    {
      new.point.x = x;
      new.point.y = y;
      new.id = id;
      new.evtime = evtime;
      g_array_append_val (priv->points, new);

      if (position)
        *position = priv->points->len - 1;

      return TRUE;
    }

  return FALSE;
}

static gboolean
mech_gesture_handle_event (MechController *controller,
                           MechEvent      *event)
{
  MechGesture *gesture = MECH_GESTURE (controller);
  MechGesturePrivate *priv;
  gint pos;

  priv = mech_gesture_get_instance_private (gesture);

  switch (event->type)
    {
    case MECH_TOUCH_DOWN:
    case MECH_BUTTON_PRESS:
      _mech_gesture_update_point (gesture, event, TRUE, NULL);
      _mech_gesture_check_recognized (gesture);
      break;
    case MECH_TOUCH_UP:
    case MECH_BUTTON_RELEASE:
      if (_mech_gesture_update_point (gesture, event, FALSE, &pos))
        {
          if (priv->recognized)
            {
              g_signal_emit (gesture, signals[UPDATE], 0, pos);
              _mech_gesture_set_recognized (gesture, FALSE);
            }

          g_array_remove_index (priv->points, pos);
        }
      break;
    case MECH_TOUCH_MOTION:
    case MECH_MOTION:
      if (_mech_gesture_update_point (gesture, event, FALSE, &pos))
        {
          _mech_gesture_check_recognized (gesture);

          if (priv->recognized)
            g_signal_emit (gesture, signals[UPDATE], 0, pos);
        }
      break;
    default:
      break;
    }

  return FALSE;
}

static void
mech_gesture_class_init (MechGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechControllerClass *controller_class = MECH_CONTROLLER_CLASS (klass);

  object_class->get_property = mech_gesture_get_property;
  object_class->set_property = mech_gesture_set_property;
  object_class->constructed = mech_gesture_constructed;
  object_class->finalize = mech_gesture_finalize;

  controller_class->handle_event = mech_gesture_handle_event;

  g_object_class_install_property (object_class,
                                   PROP_N_POINTS,
                                   g_param_spec_int ("n-points",
                                                     "Number of points",
                                                     "Number of points needed "
                                                     "to trigger the gesture",
                                                     1, G_MAXINT, 1,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));

  signals[BEGIN] = g_signal_new ("begin",
                                 G_TYPE_FROM_CLASS (klass),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (MechGestureClass, begin),
                                 NULL, NULL,
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE, 0);
  signals[END] = g_signal_new ("end",
                               G_TYPE_FROM_CLASS (klass),
                               G_SIGNAL_RUN_LAST,
                               G_STRUCT_OFFSET (MechGestureClass, end),
                               NULL, NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);
  signals[UPDATE] = g_signal_new ("update",
                                  G_TYPE_FROM_CLASS (klass),
                                  G_SIGNAL_RUN_LAST,
                                  G_STRUCT_OFFSET (MechGestureClass, update),
                                  NULL, NULL,
                                  g_cclosure_marshal_VOID__INT,
                                  G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
mech_gesture_init (MechGesture *gesture)
{
}

gboolean
mech_gesture_get_id (MechGesture *gesture,
                     gint         n_point,
                     guint32     *id)
{
  MechGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (MECH_IS_GESTURE (gesture), FALSE);

  priv = mech_gesture_get_instance_private (gesture);

  g_return_val_if_fail (n_point >= 0 && n_point < priv->points->len, FALSE);

  data = &g_array_index (priv->points, PointData, n_point);

  if (data->id == 0)
    return FALSE;

  if (id)
    *id = data->id;

  return TRUE;
}

gboolean
mech_gesture_get_point (MechGesture *gesture,
                        gint         n_point,
                        gdouble     *x,
                        gdouble     *y)
{
  MechGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (MECH_IS_GESTURE (gesture), FALSE);

  priv = mech_gesture_get_instance_private (gesture);

  g_return_val_if_fail (n_point >= 0 && n_point < priv->points->len, FALSE);

  data = &g_array_index (priv->points, PointData, n_point);

  if (x)
    *x = data->point.x;

  if (y)
    *y = data->point.y;

  return TRUE;
}

gboolean
mech_gesture_get_update_time (MechGesture *gesture,
                              gint         n_point,
                              guint32     *evtime)
{
  MechGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (MECH_IS_GESTURE (gesture), FALSE);

  priv = mech_gesture_get_instance_private (gesture);
  g_return_val_if_fail (n_point >= 0 && n_point < priv->points->len, FALSE);

  data = &g_array_index (priv->points, PointData, n_point);

  if (evtime)
    *evtime = data->evtime;

  return TRUE;
};

gboolean
mech_gesture_get_bounding_box (MechGesture       *gesture,
                               cairo_rectangle_t *rect)
{
  MechGesturePrivate *priv;
  gdouble x1, y1, x2, y2;
  guint i;

  g_return_val_if_fail (MECH_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  priv = mech_gesture_get_instance_private (gesture);

  if (priv->points->len == 0)
    return FALSE;

  x1 = y1 = G_MAXDOUBLE;
  x2 = y2 = -G_MAXDOUBLE;

  for (i = 0; i < priv->points->len; i++)
    {
      PointData *data;

      data = &g_array_index (priv->points, PointData, i);

      x1 = MIN (x1, data->point.x);
      y1 = MIN (y1, data->point.y);
      x2 = MAX (x2, data->point.x);
      y2 = MAX (y2, data->point.y);
    }

  rect->x = x1;
  rect->y = y1;
  rect->width = x2 - x1;
  rect->height = y2 - y1;

  return TRUE;
}

gboolean
mech_gesture_is_active (MechGesture *gesture)
{
  MechGesturePrivate *priv;

  g_return_val_if_fail (MECH_IS_GESTURE (gesture), FALSE);

  priv = mech_gesture_get_instance_private (gesture);
  return priv->recognized;
}
