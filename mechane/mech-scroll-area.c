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

#include <mechane/mech-scroll-area.h>
#include <mechane/mech-enum-types.h>
#include <mechane/mechane.h>
#include <math.h>

#include "mech-area-private.h"

#define SCROLLBAR_WIDTH 6
#define OVERSHOOT_MAX_DISTANCE 100
#define DECELERATION 1500

typedef struct _MechScrollAreaPrivate MechScrollAreaPrivate;

enum {
  PROP_SCROLLBARS = 1
};

struct _MechScrollAreaPrivate
{
  MechArea *hbar;
  MechArea *vbar;
  MechController *drag;
  MechController *swipe;
  MechAcceleration *haccel;
  MechAcceleration *vaccel;
  gdouble tx;
  gdouble ty;
  guint scrollbars : 2;
};

static void _mech_scroll_area_check_acceleration (MechScrollArea *area,
                                                  MechAxis        axis,
                                                  gdouble         velocity);

G_DEFINE_TYPE_WITH_PRIVATE (MechScrollArea, mech_scroll_area,
                            MECH_TYPE_SCROLL_BOX)

static gboolean
_mech_scroll_area_clamp_transform (MechScrollArea *area,
                                   gdouble        *tx,
                                   gdouble        *ty)
{
  gdouble min, max, range, x, y, overshoot = 0;
  MechScrollAreaPrivate *priv;
  gboolean clamped = FALSE;

  priv = mech_scroll_area_get_instance_private (area);
  overshoot = OVERSHOOT_MAX_DISTANCE;
  x = y = 0;

  /* Not the drag gesture, but scroll events */
  if (!mech_gesture_is_active (MECH_GESTURE (priv->drag)))
    overshoot = 0;

  if (priv->hbar)
    {
      mech_adjustable_get_bounds (MECH_ADJUSTABLE (priv->hbar), &min, &max);
      range = mech_adjustable_get_selection_size (MECH_ADJUSTABLE (priv->hbar));

      if ((priv->scrollbars & MECH_AXIS_FLAG_X) && range < max - min)
        {
          x = CLAMP (*tx, min - overshoot, max - range + overshoot);
          clamped |= (x != *tx);
        }
    }

  if (priv->vbar)
    {
      mech_adjustable_get_bounds (MECH_ADJUSTABLE (priv->vbar), &min, &max);
      range = mech_adjustable_get_selection_size (MECH_ADJUSTABLE (priv->vbar));

      if ((priv->scrollbars & MECH_AXIS_FLAG_Y) && range < max - min)
        {
          y = CLAMP (*ty, min - overshoot, max - range + overshoot);
          clamped |= (y != *ty);
        }
    }

  *tx = x;
  *ty = y;

  return clamped;
}

static void
_mech_scroll_area_drag_begin (MechGesture    *gesture,
                              MechScrollArea *area)
{
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private (area);

  priv->tx = (priv->hbar) ?
    mech_adjustable_get_value (MECH_ADJUSTABLE (priv->hbar)) : 0;
  priv->ty = (priv->vbar) ?
    mech_adjustable_get_value (MECH_ADJUSTABLE (priv->vbar)) : 0;
}

static void
_mech_scroll_area_dragged (MechScrollArea  *area,
                           gdouble          offset_x,
                           gdouble          offset_y)
{
  MechScrollAreaPrivate *priv;
  gdouble tx, ty;

  priv = mech_scroll_area_get_instance_private (area);

  if (mech_gesture_is_active (MECH_GESTURE (priv->drag)))
    {
      tx = priv->tx;
      ty = priv->ty;
    }
  else
    {
      tx = (priv->hbar) ?
        mech_adjustable_get_value (MECH_ADJUSTABLE (priv->hbar)) : 0;
      ty = (priv->vbar) ?
        mech_adjustable_get_value (MECH_ADJUSTABLE (priv->vbar)) : 0;
    }

  tx -= offset_x;
  ty -= offset_y;
  _mech_scroll_area_clamp_transform (area, &tx, &ty);

  if (priv->hbar)
    mech_adjustable_set_value (MECH_ADJUSTABLE (priv->hbar), tx);

  if (priv->vbar)
    mech_adjustable_set_value (MECH_ADJUSTABLE (priv->vbar), ty);
}

static void
_scroll_area_acceleration_displaced (MechAcceleration *acceleration,
                                     gdouble           offset,
                                     gdouble           velocity,
                                     MechScrollArea   *area)
{
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private (area);

  if (acceleration == priv->haccel)
    mech_adjustable_set_value (MECH_ADJUSTABLE (priv->hbar), offset);
  else if (acceleration == priv->vaccel)
    mech_adjustable_set_value (MECH_ADJUSTABLE (priv->vbar), offset);
}

static void
_scroll_area_acceleration_stopped (MechAcceleration *acceleration,
                                   gint64            _time,
                                   gdouble           offset,
                                   gdouble           velocity,
                                   MechScrollArea   *area)
{
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private (area);

  if (acceleration == priv->haccel)
    {
      mech_adjustable_set_value (MECH_ADJUSTABLE (priv->hbar), offset);
      _mech_scroll_area_check_acceleration (area, MECH_AXIS_X, 0);
    }
  else if (acceleration == priv->vaccel)
    {
      mech_adjustable_set_value (MECH_ADJUSTABLE (priv->vbar), offset);
      _mech_scroll_area_check_acceleration (area, MECH_AXIS_Y, 0);
    }
}

static void
_mech_scroll_area_check_acceleration (MechScrollArea *area,
                                      MechAxis        axis,
                                      gdouble         velocity)
{
  gdouble accel, boundary, value, len, min, max;
  MechScrollAreaPrivate *priv;
  cairo_rectangle_t alloc;
  MechAnimation **anim;
  MechWindow *window;

  priv = mech_scroll_area_get_instance_private (area);
  mech_area_get_allocated_size ((MechArea *) area, &alloc);
  window = mech_area_get_window ((MechArea *) area);

  if (priv->hbar && axis == MECH_AXIS_X)
    {
      mech_adjustable_get_bounds (MECH_ADJUSTABLE (priv->hbar), &min, &max);

      if (max - min <= alloc.width)
        return;

      value = mech_adjustable_get_value (MECH_ADJUSTABLE (priv->hbar));
      len = max - alloc.width;
      anim = (MechAnimation **) &priv->haccel;
    }
  else if (priv->vbar && axis == MECH_AXIS_Y)
    {
      mech_adjustable_get_bounds (MECH_ADJUSTABLE (priv->vbar), &min, &max);

      if (max - min <= alloc.height)
        return;

      value = mech_adjustable_get_value (MECH_ADJUSTABLE (priv->vbar));
      len = max - alloc.height;
      anim = (MechAnimation **) &priv->vaccel;
    }
  else
    return;

  if (*anim)
    {
      mech_animation_stop (*anim);
      g_object_unref (*anim);
      *anim = NULL;
    }

  if (!window)
    return;

  if (velocity < 0)
    {
      accel = DECELERATION;
      boundary = min - OVERSHOOT_MAX_DISTANCE;
    }
  else if (velocity > 0)
    {
      accel = -DECELERATION;
      boundary = len + OVERSHOOT_MAX_DISTANCE;
    }
  else if (value < min)
    {
      /* Overshooting on top/left */
      boundary = min;
      accel = DECELERATION;
    }
  else if (value > len)
    {
      /* Overshooting on bottom/right */
      boundary = len;
      accel = -DECELERATION;
    }
  else
    return;

  *anim = mech_acceleration_new (value, velocity, accel);
  mech_acceleration_set_boundary ((MechAcceleration *) *anim, boundary);

  g_signal_connect (*anim, "displaced",
                    G_CALLBACK (_scroll_area_acceleration_displaced), area);
  g_signal_connect (*anim, "stopped",
                    G_CALLBACK (_scroll_area_acceleration_stopped), area);
  mech_animation_run (*anim, window);
}

static void
_mech_scroll_area_check_acceleration_bound (MechScrollArea *area,
                                            MechAxis        axis)
{
  MechScrollAreaPrivate *priv;
  MechAcceleration *accel;
  cairo_rectangle_t alloc;
  gdouble vel, min, max;

  mech_area_get_allocated_size ((MechArea *) area, &alloc);
  priv = mech_scroll_area_get_instance_private (area);

  if (axis == MECH_AXIS_X && priv->haccel)
    {
      mech_adjustable_get_bounds (MECH_ADJUSTABLE (priv->hbar), &min, &max);
      accel = priv->haccel;
      max -= alloc.width;
    }
  else if (axis == MECH_AXIS_Y && priv->vaccel)
    {
      mech_adjustable_get_bounds (MECH_ADJUSTABLE (priv->vbar), &min, &max);
      accel = priv->vaccel;
      max -= alloc.height;
    }
  else
    return;

  g_object_get (accel, "initial-velocity", &vel, NULL);

  if (vel > 0)
    mech_acceleration_set_boundary (accel, max + OVERSHOOT_MAX_DISTANCE);
  else if (vel < 0)
    mech_acceleration_set_boundary (accel, min - OVERSHOOT_MAX_DISTANCE);
}

static void
_mech_scroll_area_swipe (MechGestureSwipe *gesture,
                         gdouble           velocity_x,
                         gdouble           velocity_y,
                         MechScrollArea   *area)
{
  _mech_scroll_area_check_acceleration (area, MECH_AXIS_X, -velocity_x);
  _mech_scroll_area_check_acceleration (area, MECH_AXIS_Y, -velocity_y);
}

static void
mech_scroll_area_init (MechScrollArea *area)
{
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private (area);
  mech_area_add_events (MECH_AREA (area),
                        MECH_TOUCH_MASK |
                        MECH_BUTTON_MASK |
                        MECH_MOTION_MASK |
                        MECH_SCROLL_MASK);
  mech_area_set_clip (MECH_AREA (area), TRUE);

  priv->drag = mech_gesture_drag_new ();
  g_signal_connect (priv->drag, "begin",
                    G_CALLBACK (_mech_scroll_area_drag_begin), area);
  g_signal_connect_swapped (priv->drag, "dragged",
                            G_CALLBACK (_mech_scroll_area_dragged), area);

  priv->swipe = mech_gesture_swipe_new ();
  g_signal_connect (priv->swipe, "swipe",
                    G_CALLBACK (_mech_scroll_area_swipe), area);
}

static void
mech_scroll_area_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_SCROLLBARS:
      mech_scroll_area_set_scrollbars (MECH_SCROLL_AREA (object),
                                       g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_scroll_area_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private ((MechScrollArea *) object);

  switch (prop_id)
    {
    case PROP_SCROLLBARS:
      g_value_set_flags (value, priv->scrollbars);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_scroll_area_finalize (GObject *object)
{
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private ((MechScrollArea *) object);

  if (priv->haccel)
    g_object_unref (priv->haccel);
  if (priv->vaccel)
    g_object_unref (priv->vaccel);

  g_object_unref (priv->drag);
  g_object_unref (priv->swipe);

  G_OBJECT_CLASS (mech_scroll_area_parent_class)->finalize (object);
}

static gboolean
mech_scroll_area_handle_event (MechArea  *area,
                               MechEvent *event)
{
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private ((MechScrollArea *) area);

  if (mech_area_get_children (area, NULL) == 0)
    return TRUE;

  if (mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  if (priv->hbar &&
      (event->any.target == priv->hbar ||
       mech_area_is_ancestor (event->any.target, priv->hbar)))
    return FALSE;

  if (priv->vbar &&
      (event->any.target == priv->vbar ||
       mech_area_is_ancestor (event->any.target, priv->vbar)))
    return FALSE;

  if (event->type == MECH_SCROLL)
    {
      _mech_scroll_area_dragged (MECH_SCROLL_AREA (area),
                                 -event->scroll.dx, -event->scroll.dy);
      return FALSE;
    }

  if (event->type == MECH_BUTTON_PRESS)
    {
      if (priv->vaccel)
        {
          g_object_unref (priv->vaccel);
          priv->vaccel = NULL;
        }

      if (priv->haccel)
        {
          g_object_unref (priv->haccel);
          priv->haccel = NULL;
        }
    }

  mech_controller_handle_event (priv->drag, event);
  mech_controller_handle_event (priv->swipe, event);

  return TRUE;
}

static void
_update_child_attachments (MechScrollArea *area,
                           MechArea       *child)
{
  MechFixedBox *box = (MechFixedBox *) area;
  MechScrollAreaPrivate *priv;

  priv = mech_scroll_area_get_instance_private (area);

  if (priv->scrollbars & MECH_AXIS_FLAG_X)
    {
      mech_fixed_box_detach (box, child, MECH_SIDE_LEFT);
      mech_fixed_box_detach (box, child, MECH_SIDE_RIGHT);
    }
  else
    {
      mech_fixed_box_attach (box, child, MECH_SIDE_LEFT,
                             MECH_SIDE_LEFT, MECH_UNIT_PX, 0);
      mech_fixed_box_attach (box, child, MECH_SIDE_RIGHT,
                             MECH_SIDE_RIGHT, MECH_UNIT_PX, 0);
    }

  if (priv->scrollbars & MECH_AXIS_FLAG_Y)
    {
      mech_fixed_box_detach (box, child, MECH_SIDE_TOP);
      mech_fixed_box_detach (box, child, MECH_SIDE_BOTTOM);
    }
  else
    {
      mech_fixed_box_attach (box, child, MECH_SIDE_TOP,
                             MECH_SIDE_TOP, MECH_UNIT_PX, 0);
      mech_fixed_box_attach (box, child, MECH_SIDE_BOTTOM,
                             MECH_SIDE_BOTTOM, MECH_UNIT_PX, 0);
    }
}

static void
mech_scroll_area_add (MechArea *area,
                      MechArea *child)
{
  MECH_AREA_CLASS (mech_scroll_area_parent_class)->add (area, child);
  _update_child_attachments (MECH_SCROLL_AREA (area), child);
}

static gboolean
mech_scroll_area_child_resize (MechArea *area,
                               MechArea *child)
{
  MechScrollArea *scroll = (MechScrollArea *) area;
  gboolean retval;

  retval = MECH_AREA_CLASS (mech_scroll_area_parent_class)->child_resize (area,
                                                                          child);
  _mech_scroll_area_check_acceleration_bound (scroll, MECH_AXIS_X);
  _mech_scroll_area_check_acceleration_bound (scroll, MECH_AXIS_Y);

  return retval;
}

static void
mech_scroll_area_class_init (MechScrollAreaClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_scroll_area_set_property;
  object_class->get_property = mech_scroll_area_get_property;
  object_class->finalize = mech_scroll_area_finalize;

  area_class->add = mech_scroll_area_add;
  area_class->handle_event = mech_scroll_area_handle_event;
  area_class->child_resize = mech_scroll_area_child_resize;

  g_object_class_install_property (object_class,
                                   PROP_SCROLLBARS,
                                   g_param_spec_flags ("scrollbars",
                                                       "Scrollbars",
                                                       "Axes where scrollbars will be shown",
                                                       MECH_TYPE_AXIS_FLAGS,
                                                       MECH_AXIS_FLAG_BOTH,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS));
}

MechArea *
mech_scroll_area_new (MechAxisFlags scrollbars)
{
  return g_object_new (MECH_TYPE_SCROLL_AREA,
                       "scrollbars", scrollbars,
                       NULL);
}

void
mech_scroll_area_set_scrollbars (MechScrollArea *area,
                                 MechAxisFlags   flags)
{
  MechFixedBox *box = (MechFixedBox *) area;
  MechScrollAreaPrivate *priv;
  MechArea **children;
  guint n_children, i;

  g_return_if_fail (MECH_IS_SCROLL_AREA (area));
  g_return_if_fail (flags <= MECH_AXIS_FLAG_BOTH);

  priv = mech_scroll_area_get_instance_private (area);

  if (flags == priv->scrollbars)
    return;

  priv->scrollbars = flags;
  n_children = mech_area_get_children (MECH_AREA (area), &children);

  for (i = 0; i < n_children; i++)
    _update_child_attachments (area, children[i]);

  g_free (children);

  if (flags & MECH_AXIS_FLAG_X)
    {
      if (!priv->hbar)
        {
          priv->hbar = mech_slider_new (MECH_ORIENTATION_HORIZONTAL);
          mech_area_set_preferred_size (priv->hbar, MECH_AXIS_Y,
                                        MECH_UNIT_PX, SCROLLBAR_WIDTH);
          mech_area_set_depth (priv->hbar, 1);
          mech_area_set_parent (priv->hbar, MECH_AREA (area));

          mech_fixed_box_attach (box, priv->hbar, MECH_SIDE_BOTTOM,
                                 MECH_SIDE_BOTTOM, MECH_UNIT_PX, 0);
          mech_fixed_box_attach (box, priv->hbar, MECH_SIDE_LEFT,
                                 MECH_SIDE_LEFT, MECH_UNIT_PX, 0);
          mech_scrollable_set_adjustable (MECH_SCROLLABLE (area), MECH_AXIS_X,
                                          MECH_ADJUSTABLE (priv->hbar));
        }

      mech_fixed_box_attach (box, priv->hbar, MECH_SIDE_RIGHT,
                             MECH_SIDE_RIGHT, MECH_UNIT_PX,
                             (flags & MECH_AXIS_FLAG_Y) ? SCROLLBAR_WIDTH : 0);
      mech_area_set_visible (priv->hbar, TRUE);
    }
  else if (priv->hbar)
    mech_area_set_visible (priv->hbar, FALSE);

  if (flags & MECH_AXIS_FLAG_Y)
    {
      if (!priv->vbar)
        {
          priv->vbar = mech_slider_new (MECH_ORIENTATION_VERTICAL);
          mech_area_set_preferred_size (priv->vbar, MECH_AXIS_X,
                                        MECH_UNIT_PX, SCROLLBAR_WIDTH);
          mech_area_set_depth (priv->vbar, 1);
          mech_area_set_parent (priv->vbar, MECH_AREA (area));

          mech_fixed_box_attach (box, priv->vbar, MECH_SIDE_RIGHT,
                                 MECH_SIDE_RIGHT, MECH_UNIT_PX, 0);
          mech_fixed_box_attach (box, priv->vbar, MECH_SIDE_TOP,
                                 MECH_SIDE_TOP, MECH_UNIT_PX, 0);
          mech_scrollable_set_adjustable (MECH_SCROLLABLE (area), MECH_AXIS_Y,
                                          MECH_ADJUSTABLE (priv->vbar));
        }

      mech_fixed_box_attach (box, priv->vbar, MECH_SIDE_BOTTOM,
                             MECH_SIDE_BOTTOM, MECH_UNIT_PX,
                             (flags & MECH_AXIS_FLAG_X) ? SCROLLBAR_WIDTH : 0);
      mech_area_set_visible (priv->vbar, TRUE);
    }
  else if (priv->vbar)
    mech_area_set_visible (priv->vbar, FALSE);
}

MechAxisFlags
mech_scroll_area_get_scrollbars (MechScrollArea *area)
{
  MechScrollAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_SCROLL_AREA (area), MECH_AXIS_FLAG_NONE);

  priv = mech_scroll_area_get_instance_private (area);
  return priv->scrollbars;
}
