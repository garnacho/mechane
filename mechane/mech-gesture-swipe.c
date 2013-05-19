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

#include "mech-gesture-swipe.h"
#include "mech-marshal.h"

#define CAPTURE_THRESHOLD_MS 150

typedef struct _MechGestureSwipePrivate MechGestureSwipePrivate;
typedef struct _EventData EventData;

struct _EventData
{
  guint32 evtime;
  MechPoint point;
};

struct _MechGestureSwipePrivate
{
  GArray *events;
};

enum {
  SWIPE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechGestureSwipe, mech_gesture_swipe,
                            MECH_TYPE_GESTURE)

static void
mech_gesture_swipe_finalize (GObject *object)
{
  MechGestureSwipePrivate *priv;

  priv = mech_gesture_swipe_get_instance_private ((MechGestureSwipe *) object);
  g_array_free (priv->events, TRUE);

  G_OBJECT_CLASS (mech_gesture_swipe_parent_class)->finalize (object);
}

static gboolean
mech_gesture_swipe_check (MechGesture *gesture)
{
  return TRUE;
}

static void
_mech_gesture_swipe_clear_backlog (MechGestureSwipe *gesture,
                                   guint32           evtime)
{
  MechGestureSwipePrivate *priv;
  gint i, length = 0;

  priv = mech_gesture_swipe_get_instance_private (gesture);

  for (i = 0; i < priv->events->len; i++)
    {
      EventData *data;

      data = &g_array_index (priv->events, EventData, i);

      if (data->evtime >= evtime - CAPTURE_THRESHOLD_MS)
        {
          length = i - 1;
          break;
        }
    }

  if (length > 0)
    g_array_remove_range (priv->events, 0, length);
}

static void
mech_gesture_swipe_update (MechGesture *gesture,
                           gint         n_point)
{
  MechGestureSwipe *swipe = MECH_GESTURE_SWIPE (gesture);
  MechGestureSwipePrivate *priv;
  EventData new;

  priv = mech_gesture_swipe_get_instance_private (swipe);
  mech_gesture_get_update_time (gesture, n_point, &new.evtime);
  mech_gesture_get_point (gesture, n_point, &new.point.x, &new.point.y);

  _mech_gesture_swipe_clear_backlog (swipe, new.evtime);
  g_array_append_val (priv->events, new);
}

static void
_mech_gesture_swipe_calculate_velocity (MechGestureSwipe *gesture,
                                        gdouble          *velocity_x,
                                        gdouble          *velocity_y)
{
  MechGestureSwipePrivate *priv;
  EventData *start, *end;
  gdouble diff_x, diff_y;
  guint32 diff_time;

  priv = mech_gesture_swipe_get_instance_private (gesture);
  *velocity_x = *velocity_y = 0;

  if (priv->events->len == 0)
    return;

  start = &g_array_index (priv->events, EventData, 0);
  end = &g_array_index (priv->events, EventData, priv->events->len - 1);

  diff_time = end->evtime - start->evtime;
  diff_x = end->point.x - start->point.x;
  diff_y = end->point.y - start->point.y;

  if (diff_time == 0)
    return;

  /* Velocity in units/sec */
  *velocity_x = diff_x * 1000 / diff_time;
  *velocity_y = diff_y * 1000 / diff_time;
}

static void
mech_gesture_swipe_end (MechGesture *gesture)
{
  MechGestureSwipe *swipe = MECH_GESTURE_SWIPE (gesture);
  gdouble velocity_x, velocity_y;
  MechGestureSwipePrivate *priv;
  guint32 evtime;

  priv = mech_gesture_swipe_get_instance_private (swipe);
  mech_gesture_get_update_time (gesture, 0, &evtime);
  _mech_gesture_swipe_clear_backlog (swipe, evtime);
  _mech_gesture_swipe_calculate_velocity (swipe, &velocity_x, &velocity_y);

  g_signal_emit (gesture, signals[SWIPE], 0, velocity_x, velocity_y);

  if (priv->events->len > 0)
    g_array_remove_range (priv->events, 0, priv->events->len);
}

static void
mech_gesture_swipe_class_init (MechGestureSwipeClass *klass)
{
  MechGestureClass *gesture_class = MECH_GESTURE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_gesture_swipe_finalize;

  gesture_class->check = mech_gesture_swipe_check;
  gesture_class->update = mech_gesture_swipe_update;
  gesture_class->end = mech_gesture_swipe_end;

  signals[SWIPE] =
    g_signal_new ("swipe",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechGestureSwipeClass, swipe),
                  NULL, NULL,
                  _mech_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}

static void
mech_gesture_swipe_init (MechGestureSwipe *gesture)
{
  MechGestureSwipePrivate *priv;

  priv = mech_gesture_swipe_get_instance_private (gesture);
  priv->events = g_array_new (FALSE, FALSE, sizeof (EventData));
}

MechController *
mech_gesture_swipe_new (void)
{
  return g_object_new (MECH_TYPE_GESTURE_SWIPE, NULL);
}
