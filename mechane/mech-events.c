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

#include <mechane/mech-events.h>
#include <mechane/mechane.h>

#define IS_CROSSING_ENTER(t) ((t) == MECH_ENTER || (t) == MECH_FOCUS_IN)
#define IS_CROSSING_LEAVE(t) ((t) == MECH_LEAVE || (t) == MECH_FOCUS_OUT)

#define IS_EVENT_TYPE(t)     ((t) >= MECH_FOCUS_IN && (t) <= MECH_TOUCH_UP)
#define IS_INPUT_TYPE(t)     ((t) >= MECH_KEY_PRESS && (t) <= MECH_TOUCH_UP)
#define IS_KEY_TYPE(t)       ((t) == MECH_KEY_PRESS || (t) == MECH_KEY_RELEASE)
#define IS_POINTER_TYPE(t)   ((t) >= MECH_MOTION && (t) <= MECH_TOUCH_UP)
#define IS_CROSSING_TYPE(t)  (IS_CROSSING_ENTER (t) || IS_CROSSING_LEAVE (t))
#define IS_SCROLL_TYPE(t)    ((t) == MECH_SCROLL)
#define IS_TOUCH_TYPE(t)     ((t) >= MECH_TOUCH_DOWN && (t) <= MECH_TOUCH_UP)

G_DEFINE_BOXED_TYPE (MechEvent, mech_event, mech_event_copy, mech_event_free)

MechEvent *
mech_event_new (MechEventType type)
{
  MechEvent *event;

  g_return_val_if_fail (IS_EVENT_TYPE (type), NULL);

  event = g_slice_new0 (MechEvent);
  event->type = type;

  return event;
}

MechEvent *
mech_event_copy (MechEvent *event)
{
  MechEvent *copy;

  if (!event)
    return NULL;

  copy = mech_event_new (event->type);
  *copy = *event;

  if (copy->any.area)
    g_object_ref (copy->any.area);
  if (copy->any.target)
    g_object_ref (copy->any.target);
  if (IS_CROSSING_TYPE (copy->type) &&
      copy->crossing.other_area)
    g_object_ref (copy->crossing.other_area);

  return copy;
}

void
mech_event_free (MechEvent *event)
{
  g_return_if_fail (event != NULL);

  if (event->any.area)
    g_object_unref (event->any.area);
  if (event->any.target)
    g_object_unref (event->any.target);
  if (IS_CROSSING_TYPE (event->type) &&
      event->crossing.other_area)
    g_object_unref (event->crossing.other_area);

  g_slice_free (MechEvent, event);
}

MechArea *
mech_event_get_area (MechEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);
  g_return_val_if_fail (IS_EVENT_TYPE (event->type), NULL);

  return event->any.area;
}

void
mech_event_set_area (MechEvent *event,
                     MechArea  *area)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (IS_EVENT_TYPE (event->type));
  g_return_if_fail (MECH_IS_AREA (area));

  if (area)
    g_object_ref (area);

  if (event->any.area)
    g_object_unref (event->any.area);

  event->any.area = area;
}

gboolean
mech_event_has_flags (MechEvent *event,
                      guint      flags)
{
  g_return_val_if_fail (event != NULL, FALSE);
  g_return_val_if_fail (IS_EVENT_TYPE (event->type), FALSE);

  return ((event->any.flags & flags) == flags);
}

gboolean
mech_event_input_get_time (MechEvent *event,
                           guint32   *evtime)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (!IS_INPUT_TYPE (event->type))
    return FALSE;

  if (evtime)
    *evtime = event->input.evtime;

  return TRUE;
}

gboolean
mech_event_pointer_get_coords (MechEvent *event,
                               gdouble   *x,
                               gdouble   *y)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (!IS_POINTER_TYPE (event->type))
    return FALSE;

  if (x)
    *x = event->pointer.x;
  if (y)
    *y = event->pointer.y;

  return TRUE;
}

gboolean
mech_event_scroll_get_deltas (MechEvent *event,
                              gdouble   *dx,
                              gdouble   *dy)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (!IS_SCROLL_TYPE (event->type))
    return FALSE;

  if (dx)
    *dx = event->scroll.dx;
  if (dy)
    *dy = event->scroll.dy;

  return TRUE;
}

gboolean
mech_event_crossing_get_areas (MechEvent  *event,
                               MechArea  **from,
                               MechArea  **to)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (!IS_CROSSING_TYPE (event->type))
    return FALSE;

  if (from)
    *from = (IS_CROSSING_LEAVE (event->type)) ?
      event->any.area : event->crossing.other_area;

  if (to)
    *from = (IS_CROSSING_ENTER (event->type)) ?
      event->any.area : event->crossing.other_area;

  return TRUE;
}

gboolean
mech_event_crossing_get_mode (MechEvent *event,
                              guint     *mode)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (!IS_CROSSING_TYPE (event->type))
    return FALSE;

  if (mode)
    *mode = event->crossing.mode;

  return TRUE;
}

gboolean
mech_event_touch_get_id (MechEvent *event,
                         gint32    *id)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (!IS_TOUCH_TYPE (event->type))
    return FALSE;

  if (id)
    *id = event->touch.id;

  return TRUE;
}
