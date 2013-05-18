/* Mechane:
 * Copyright (C) 2013 Carlos Garnacho <carlosg@gnome.org>
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

#include "mech-marshal.h"
#include "mech-enum-types.h"
#include "mech-linear-box.h"
#include "mech-toggle-button.h"
#include "mech-toggle.h"
#include "mech-window-frame-private.h"

#define DRAG_CORNER_SIZE 20
#define DRAG_SIDE_SIZE 4

typedef struct _MechWindowFramePrivate MechWindowFramePrivate;

enum {
  PROP_MAXIMIZED = 1
};

enum {
  MOVE,
  RESIZE,
  CLOSE,
  LAST_SIGNAL
};

struct _MechWindowFramePrivate
{
  MechArea *close_button;
  MechArea *maximize_toggle_button;
  MechArea *container;
  guint resizable : 1;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechWindowFrame, mech_window_frame,
                            MECH_TYPE_FIXED_BOX)

static void
mech_window_frame_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MechWindowFramePrivate *priv;

  priv = mech_window_frame_get_instance_private ((MechWindowFrame *) object);

  switch (prop_id)
    {
    case PROP_MAXIMIZED:
      {
        gboolean active;

        active = mech_toggle_get_active (MECH_TOGGLE (priv->maximize_toggle_button));
        g_value_set_boolean (value, active);
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_window_frame_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  MechWindowFramePrivate *priv;

  priv = mech_window_frame_get_instance_private ((MechWindowFrame *) object);

  switch (prop_id)
    {
    case PROP_MAXIMIZED:
      mech_toggle_set_active (MECH_TOGGLE (priv->maximize_toggle_button),
                              g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_window_frame_add (MechArea *area,
                       MechArea *child)
{
  MechWindowFramePrivate *priv;

  priv = mech_window_frame_get_instance_private ((MechWindowFrame *) area);
  mech_area_add (priv->container, child);
}

static void
mech_window_frame_remove (MechArea *area,
                          MechArea *child)
{
  MechWindowFramePrivate *priv;

  priv = mech_window_frame_get_instance_private ((MechWindowFrame *) area);
  mech_area_remove (priv->container, child);
}

static MechSideFlags
_window_frame_dragged_side (MechWindowFrame *frame,
                            MechEvent       *event)
{
  cairo_rectangle_t rect;
  gdouble x, y;

  if (!mech_event_pointer_get_coords (event, &x, &y))
    return 0;

  mech_area_get_allocated_size ((MechArea *) frame, &rect);

  if (x < DRAG_CORNER_SIZE && y < DRAG_CORNER_SIZE)
    return MECH_SIDE_FLAG_CORNER_TOP_LEFT;
  else if (x > rect.width - DRAG_CORNER_SIZE && y < DRAG_CORNER_SIZE)
    return MECH_SIDE_FLAG_CORNER_TOP_RIGHT;
  else if (x < DRAG_CORNER_SIZE && y > rect.height - DRAG_CORNER_SIZE)
    return MECH_SIDE_FLAG_CORNER_BOTTOM_LEFT;
  else if (x > rect.width - DRAG_CORNER_SIZE &&
           y > rect.height - DRAG_CORNER_SIZE)
    return MECH_SIDE_FLAG_CORNER_BOTTOM_RIGHT;
  else if (x < DRAG_SIDE_SIZE)
    return MECH_SIDE_FLAG_LEFT;
  else if (x > rect.width - DRAG_SIDE_SIZE)
    return MECH_SIDE_FLAG_RIGHT;
  else if (y < DRAG_SIDE_SIZE)
    return MECH_SIDE_FLAG_TOP;
  else if (y > rect.height - DRAG_SIDE_SIZE)
    return MECH_SIDE_FLAG_BOTTOM;

  return 0;
}

static void
_mech_window_frame_update_cursor (MechWindowFrame *frame,
                                  MechEvent       *event)
{
  MechCursorType cursor_type = MECH_CURSOR_BLANK;
  MechCursor *cursor = NULL;
  MechSideFlags side;

  side = _window_frame_dragged_side (frame, event);

  if (side == MECH_SIDE_FLAG_TOP)
    cursor_type = MECH_CURSOR_DRAG_TOP;
  else if (side == MECH_SIDE_FLAG_BOTTOM)
    cursor_type = MECH_CURSOR_DRAG_BOTTOM;
  else if (side == MECH_SIDE_FLAG_LEFT)
    cursor_type = MECH_CURSOR_DRAG_LEFT;
  else if (side == MECH_SIDE_FLAG_RIGHT)
    cursor_type = MECH_CURSOR_DRAG_RIGHT;
  else if (side == MECH_SIDE_FLAG_CORNER_TOP_LEFT)
    cursor_type = MECH_CURSOR_DRAG_TOP_LEFT;
  else if (side == MECH_SIDE_FLAG_CORNER_TOP_RIGHT)
    cursor_type = MECH_CURSOR_DRAG_TOP_RIGHT;
  else if (side == MECH_SIDE_FLAG_CORNER_BOTTOM_LEFT)
    cursor_type = MECH_CURSOR_DRAG_BOTTOM_LEFT;
  else if (side == MECH_SIDE_FLAG_CORNER_BOTTOM_RIGHT)
    cursor_type = MECH_CURSOR_DRAG_BOTTOM_RIGHT;

  if (cursor_type != MECH_CURSOR_BLANK)
    cursor = mech_cursor_lookup (cursor_type);

  mech_area_set_cursor ((MechArea *) frame, cursor);
}

static gboolean
mech_window_frame_handle_event (MechArea  *area,
                                MechEvent *event)
{
  MechWindowFrame *frame = (MechWindowFrame *) area;
  MechWindowFramePrivate *priv;
  guint sides;

  priv = mech_window_frame_get_instance_private (frame);

  if (event->type == MECH_MOTION &&
      mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    {
      _mech_window_frame_update_cursor (frame, event);
    }
  else if (event->type == MECH_BUTTON_PRESS &&
           event->any.target == area &&
           !mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    {
      sides = _window_frame_dragged_side (frame, event);

      if (priv->resizable && sides != 0)
        g_signal_emit (area, signals[RESIZE], 0, event, sides);
      else if (sides == 0)
        g_signal_emit (area, signals[MOVE], 0, event);

      /* Fallback to returning FALSE so the button grab is
       * not set, as the window move/resize operations shall
       * swallow the matching button release for this button
       * press.
       */
    }

  return FALSE;
}

static void
mech_window_frame_class_init (MechWindowFrameClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_window_frame_set_property;
  object_class->get_property = mech_window_frame_get_property;

  area_class->add = mech_window_frame_add;
  area_class->remove = mech_window_frame_remove;
  area_class->handle_event = mech_window_frame_handle_event;

  g_object_class_install_property (object_class,
                                   PROP_MAXIMIZED,
                                   g_param_spec_boolean ("maximized",
                                                         "maximized",
                                                         "Whether the window is maximized",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_MAXIMIZED,
                                   g_param_spec_boolean ("resizable",
                                                         "resizable",
                                                         "Whether the window is resizable",
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  signals[MOVE] =
    g_signal_new ("move",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowFrameClass, move),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  MECH_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  signals[RESIZE] =
    g_signal_new ("resize",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowFrameClass, resize),
                  NULL, NULL,
                  _mech_marshal_VOID__BOXED_FLAGS,
                  G_TYPE_NONE, 2,
                  MECH_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
                  MECH_TYPE_SIDE_FLAGS);
  signals[CLOSE] =
    g_signal_new ("close",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowFrameClass, close),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
_close_button_activated (MechWindowFrame *frame)
{
  g_signal_emit (frame, signals[CLOSE], 0);
}

static void
_maximize_button_toggled (MechWindowFrame *frame)
{
  g_object_notify ((GObject *) frame, "maximized");
}

static void
mech_window_frame_init (MechWindowFrame *frame)
{
  MechArea *area = (MechArea *) frame;
  MechWindowFramePrivate *priv;
  MechArea *box;

  priv = mech_window_frame_get_instance_private (frame);
  priv->resizable = TRUE;

  mech_area_set_name (area, "window-frame");
  mech_area_set_events (area,
                        MECH_BUTTON_MASK | MECH_CROSSING_MASK |
                        MECH_MOTION_MASK);

  /* Children container */
  priv->container = mech_area_new (NULL, MECH_BUTTON_MASK);
  mech_area_set_clip (priv->container, TRUE);
  mech_area_set_name (priv->container, "window-content");
  mech_area_set_parent (priv->container, area);

  mech_fixed_box_attach (MECH_FIXED_BOX (frame), priv->container,
                         MECH_SIDE_TOP, MECH_SIDE_TOP,
                         MECH_UNIT_PX, DRAG_CORNER_SIZE);
  mech_fixed_box_attach (MECH_FIXED_BOX (frame), priv->container,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT,
                         MECH_UNIT_PX, 0);
  mech_fixed_box_attach (MECH_FIXED_BOX (frame), priv->container,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT,
                         MECH_UNIT_PX, 0);
  mech_fixed_box_attach (MECH_FIXED_BOX (frame), priv->container,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM,
                         MECH_UNIT_PX, DRAG_SIDE_SIZE);

  /* Button box */
  box = mech_linear_box_new (MECH_ORIENTATION_HORIZONTAL);
  mech_area_set_parent (box, area);
  mech_fixed_box_attach (MECH_FIXED_BOX (area), box,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 0);
  mech_fixed_box_attach (MECH_FIXED_BOX (area), box,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 0);

  /* Maximize toggle button */
  priv->maximize_toggle_button = mech_toggle_button_new ();
  mech_area_set_name (priv->maximize_toggle_button, "maximize-button");
  mech_area_add (box, priv->maximize_toggle_button);
  g_signal_connect_swapped (priv->maximize_toggle_button, "notify::active",
                            G_CALLBACK (_maximize_button_toggled), frame);
  /* FIXME: should not be needed here */
  mech_area_set_preferred_size (priv->maximize_toggle_button,
                                MECH_AXIS_X, MECH_UNIT_PX, 16);
  mech_area_set_preferred_size (priv->maximize_toggle_button,
                                MECH_AXIS_Y, MECH_UNIT_PX, 16);

  /* Close button */
  priv->close_button = mech_button_new ();
  mech_area_set_name (priv->close_button, "close-button");
  mech_area_add (box, priv->close_button);
  g_signal_connect_swapped (priv->close_button, "activated",
                            G_CALLBACK (_close_button_activated), frame);
}

MechArea *
mech_window_frame_new (void)
{
  return g_object_new (MECH_TYPE_WINDOW_FRAME, NULL);
}

void
mech_window_frame_set_maximized (MechWindowFrame *frame,
                                 gboolean         maximized)
{
  MechWindowFramePrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW_FRAME (frame));

  priv = mech_window_frame_get_instance_private (frame);
  mech_toggle_set_active (MECH_TOGGLE (priv->maximize_toggle_button),
                          maximized);
}

gboolean
mech_window_frame_get_maximized (MechWindowFrame *frame)
{
  MechWindowFramePrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW_FRAME (frame), FALSE);

  priv = mech_window_frame_get_instance_private (frame);
  return mech_toggle_get_active (MECH_TOGGLE (priv->maximize_toggle_button));
}

void
mech_window_frame_set_resizable (MechWindowFrame *frame,
                                 gboolean         resizable)
{
  MechWindowFramePrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW_FRAME (frame));

  priv = mech_window_frame_get_instance_private (frame);
  priv->resizable = (resizable == TRUE);
  mech_area_set_visible (priv->maximize_toggle_button, resizable);
  g_object_notify ((GObject *) frame, "resizable");
}

gboolean
mech_window_frame_get_resizable (MechWindowFrame *frame)
{
  MechWindowFramePrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW_FRAME (frame), FALSE);

  priv = mech_window_frame_get_instance_private (frame);
  return priv->resizable;
}
