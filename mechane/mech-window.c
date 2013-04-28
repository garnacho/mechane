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

#include <mechane/mech-marshal.h>
#include <mechane/mech-stage-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-window-private.h>

enum {
  CROSSING_ENTER = 1 << 1,
  CROSSING_LEAVE = 1 << 2,
};

typedef struct _MechWindowPrivate MechWindowPrivate;
typedef struct _MechTouchInfo MechTouchInfo;
typedef struct _MechPointerInfo MechPointerInfo;
typedef struct _MechKeyboardInfo MechKeyboardInfo;

struct _MechTouchInfo
{
  GPtrArray *event_areas;
};

struct _MechPointerInfo
{
  GPtrArray *crossing_areas;
  GPtrArray *grab_areas;
};

struct _MechKeyboardInfo
{
  GPtrArray *focus_areas;
};

struct _MechWindowPrivate
{
  MechStage *stage;
  MechMonitor *monitor;
  MechClock *clock;

  MechPointerInfo pointer_info;
  MechKeyboardInfo keyboard_info;
  GHashTable *touch_info;

  gchar *title;

  gint width;
  gint height;

  guint resizable        : 1;
  guint visible          : 1;
};

enum {
  SIZE_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_TITLE = 1,
  PROP_RESIZABLE,
  PROP_VISIBLE,
  PROP_MONITOR
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechWindow, mech_window, G_TYPE_OBJECT)

static void
mech_window_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MechWindow *window = (MechWindow *) object;
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  switch (param_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_RESIZABLE:
      g_value_set_boolean (value, priv->resizable);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visible);
      break;
    case PROP_MONITOR:
      g_value_set_object (value, priv->monitor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
mech_window_set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MechWindow *window = (MechWindow *) object;

  switch (param_id)
    {
    case PROP_TITLE:
      mech_window_set_title (window, g_value_get_string (value));
      break;
    case PROP_RESIZABLE:
      mech_window_set_resizable (window, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE:
      mech_window_set_visible (window, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
mech_window_finalize (GObject *object)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private ((MechWindow *) object);

  g_free (priv->title);
  g_object_unref (priv->clock);

  if (priv->pointer_info.crossing_areas)
    g_ptr_array_unref (priv->pointer_info.crossing_areas);
  if (priv->pointer_info.grab_areas)
    g_ptr_array_unref (priv->pointer_info.grab_areas);
  if (priv->keyboard_info.focus_areas)
    g_ptr_array_unref (priv->keyboard_info.focus_areas);
  if (priv->touch_info)
    g_hash_table_unref (priv->touch_info);

  g_object_unref (priv->stage);

  G_OBJECT_CLASS (mech_window_parent_class)->finalize (object);
}

static gboolean
_mech_window_propagate_event (MechWindow *window,
                              GPtrArray  *areas,
                              MechEvent  *event)
{
  gboolean handled = FALSE;
  MechEvent *area_event;
  MechArea *area;
  gint i;

  for (i = 0; i < areas->len; i++)
    {
      area = g_ptr_array_index (areas, i);
      area_event = mech_event_translate (event, area);
      area_event->any.target =
        g_object_ref (g_ptr_array_index (areas, areas->len - 1));
      area_event->any.flags |= MECH_EVENT_FLAG_CAPTURE_PHASE;
      handled = _mech_area_handle_event (area, area_event);
      mech_event_free (area_event);

      if (handled)
        break;
    }

  if (i >= areas->len)
    i = areas->len - 1;

  while (i >= 0)
    {
      area = g_ptr_array_index (areas, i);
      area_event = mech_event_translate (event, area);
      area_event->any.target =
        g_object_ref (g_ptr_array_index (areas, areas->len - 1));
      handled |= _mech_area_handle_event (area, area_event);
      mech_event_free (area_event);
      i--;
    }

  return handled;
}

static void
_mech_window_unset_pointer_grab (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  if (priv->pointer_info.grab_areas)
    {
      g_ptr_array_unref (priv->pointer_info.grab_areas);
      priv->pointer_info.grab_areas = NULL;
    }
}

static void
_mech_window_update_seat_state (MechWindow *window,
                                MechEvent  *event,
                                GPtrArray  *areas,
                                gboolean    handled)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  if (handled && event->type == MECH_BUTTON_PRESS)
    priv->pointer_info.grab_areas = (handled && areas) ? g_ptr_array_ref (areas) : NULL;
  else if (event->type == MECH_BUTTON_RELEASE &&
           priv->pointer_info.grab_areas)
    {
      _mech_window_unset_pointer_grab (window);
    }
  else if (handled && event->type == MECH_TOUCH_DOWN)
    {
      if (!priv->touch_info)
        priv->touch_info =
          g_hash_table_new_full (NULL, NULL, NULL,
                                 (GDestroyNotify) g_ptr_array_unref);

      g_hash_table_insert (priv->touch_info, GINT_TO_POINTER (event->touch.id),
                           g_ptr_array_ref (areas));
    }
  else if (event->type == MECH_TOUCH_UP && priv->touch_info)
    g_hash_table_remove (priv->touch_info, GINT_TO_POINTER (event->touch.id));
}

static gboolean
_mech_window_handle_event_internal (MechWindow *window,
                                    MechEvent  *event)
{
  MechWindowPrivate *priv;
  GPtrArray *areas = NULL;
  gboolean retval = FALSE;

  priv = mech_window_get_instance_private (window);

  if (priv->keyboard_info.focus_areas &&
      (event->type == MECH_KEY_PRESS ||
       event->type == MECH_KEY_RELEASE))
    areas = g_ptr_array_ref (priv->keyboard_info.focus_areas);
  else if (priv->touch_info &&
           (event->type == MECH_TOUCH_MOTION || event->type == MECH_TOUCH_UP))
    {
      areas = g_hash_table_lookup (priv->touch_info,
                                   GINT_TO_POINTER (event->touch.id));
      if (areas)
        g_ptr_array_ref (areas);
    }
  else if (priv->pointer_info.grab_areas)
    areas = g_ptr_array_ref (priv->pointer_info.grab_areas);
  else
    areas = _mech_stage_pick_for_event (priv->stage, NULL, event->type,
                                        event->pointer.x, event->pointer.y);

  if (!areas)
    {
      _mech_window_update_seat_state (window, event, NULL, retval);
      return FALSE;
    }
  else
    {
      retval = _mech_window_propagate_event (window, areas, event);
      _mech_window_update_seat_state (window, event, areas, retval);
      g_ptr_array_unref (areas);

      return retval;
    }
}

static void
mech_window_class_init (MechWindowClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->get_property = mech_window_get_property;
  object_class->set_property = mech_window_set_property;
  object_class->finalize = mech_window_finalize;

  klass->handle_event = _mech_window_handle_event_internal;

  signals[SIZE_CHANGED] =
    g_signal_new ("size-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowClass, size_changed),
                  NULL, NULL,
                  _mech_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "window title",
                                                        "The window title",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_RESIZABLE,
                                   g_param_spec_boolean ("resizable",
                                                         "resizable",
                                                         "Whether the window can be resized by the user",
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visible",
                                                         "visible",
                                                         "Whether the window is visible",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_MONITOR,
                                   g_param_spec_object ("monitor",
                                                        "Monitor",
                                                        "Monitor displaying the window",
                                                        MECH_TYPE_MONITOR,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
mech_window_init (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  priv->resizable = TRUE;
  priv->stage = _mech_stage_new ();
}

gboolean
mech_window_handle_event (MechWindow *window,
                          MechEvent  *event)
{
  MechWindowClass *window_class;

  window_class = MECH_WINDOW_GET_CLASS (window);

  if (!window_class->handle_event)
    return FALSE;

  return window_class->handle_event (window, event);
}

void
mech_window_set_resizable (MechWindow *window,
                           gboolean    resizable)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  priv->resizable = (resizable == TRUE);
  g_object_notify ((GObject *) window, "resizable");
}

gboolean
mech_window_get_resizable (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);

  priv = mech_window_get_instance_private (window);
  return priv->resizable;
}

void
mech_window_set_visible (MechWindow *window,
                         gboolean    visible)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);

  if (MECH_WINDOW_GET_CLASS (window)->set_visible)
    MECH_WINDOW_GET_CLASS (window)->set_visible (window, visible);
  else
    g_warning ("%s: no vmethod implementation", G_STRFUNC);

  priv->visible = (visible == TRUE);
  g_object_notify ((GObject *) window, "visible");
}

gboolean
mech_window_get_visible (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);

  priv = mech_window_get_instance_private (window);
  return priv->visible;
}

void
mech_window_set_title (MechWindow  *window,
                       const gchar *title)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  g_free (priv->title);
  priv->title = g_strdup (title);

  if (!MECH_WINDOW_GET_CLASS (window)->set_title)
    {
      g_warning ("%s: no vmethod implementation", G_STRFUNC);
      return;
    }

  MECH_WINDOW_GET_CLASS (window)->set_title (window, priv->title);
}

const gchar *
mech_window_get_title (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), NULL);

  priv = mech_window_get_instance_private (window);
  return priv->title;
}

void
mech_window_get_size (MechWindow *window,
                      gint       *width,
                      gint       *height)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);

  _mech_stage_get_size (priv->stage, width, height);
}

void
_mech_window_set_monitor (MechWindow  *window,
                          MechMonitor *monitor)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  priv->monitor = monitor;
}

MechMonitor *
mech_window_get_monitor (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), NULL);

  priv = mech_window_get_instance_private (window);
  return priv->monitor;
}

void
_mech_window_set_clock (MechWindow *window,
                        MechClock  *clock)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  if (priv->clock)
    {
      g_object_unref (priv->clock);
      priv->clock = NULL;
    }

  if (clock)
    {
      priv->clock = g_object_ref (clock);
    }
}

MechClock *
_mech_window_get_clock (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  return priv->clock;
}

MechArea *
mech_window_get_focus (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), NULL);

  priv = mech_window_get_instance_private (window);

  if (!priv->keyboard_info.focus_areas)
    return NULL;

  return g_ptr_array_index (priv->keyboard_info.focus_areas,
                            priv->keyboard_info.focus_areas->len - 1);
}

MechStage *
_mech_window_get_stage (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  return priv->stage;
}

gboolean
mech_window_move (MechWindow *window,
                  MechEvent  *event)
{
  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != MECH_KEY_PRESS && event->type == MECH_KEY_PRESS &&
      event->type != MECH_BUTTON_PRESS && event->type == MECH_BUTTON_RELEASE)
    return FALSE;

  return MECH_WINDOW_GET_CLASS (window)->move (window, event);
}

gboolean
mech_window_resize (MechWindow    *window,
                    MechEvent     *event,
                    MechSideFlags  side)
{
  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != MECH_KEY_PRESS && event->type == MECH_KEY_PRESS &&
      event->type != MECH_BUTTON_PRESS && event->type == MECH_BUTTON_RELEASE)
    return FALSE;

  return MECH_WINDOW_GET_CLASS (window)->resize (window, event, side);
}
