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
#include <mechane/mechane.h>

enum {
  CROSSING_ENTER = 1 << 1,
  CROSSING_LEAVE = 1 << 2,
};

typedef struct _MechWindowPrivate MechWindowPrivate;
typedef struct _MechTouchInfo MechTouchInfo;
typedef struct _MechPointerInfo MechPointerInfo;
typedef struct _MechKeyboardInfo MechKeyboardInfo;
typedef struct _MechStateData MechStateData;

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

struct _MechStateData
{
  MechMonitor *monitor;
  guint state;
  gint previous_width;
  gint previous_height;
};

struct _MechWindowPrivate
{
  MechStage *stage;
  MechMonitor *monitor;
  MechClock *clock;

  MechPointerInfo pointer_info;
  MechKeyboardInfo keyboard_info;
  GHashTable *touch_info;

  GArray *state;
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
  PROP_MONITOR,
  PROP_STATE
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
    case PROP_STATE:
      g_value_set_enum (value, mech_window_get_state (window));
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
  if (priv->state)
    g_array_unref (priv->state);

  g_object_unref (priv->stage);

  G_OBJECT_CLASS (mech_window_parent_class)->finalize (object);
}

static gint
_find_common_parent_index (GPtrArray *a,
                           GPtrArray *b)
{
  if (a && b)
    {
      MechArea *pa, *pb;
      gint i;

      for (i = 0; i < MIN (a->len, b->len); i++)
        {
          pa = g_ptr_array_index (a, i);
          pb = g_ptr_array_index (b, i);

          if (pa != pb)
            break;
        }

      return i;
    }

  return 0;
}

static GHashTable *
_window_diff_crossing_stack (GPtrArray *old,
                             GPtrArray *new,
                             gint       _index)
{
  MechArea *area, *old_target, *new_target;
  GHashTable *change_map;
  guint flags;
  gint i;

  change_map = g_hash_table_new (NULL, NULL);
  old_target = (old) ? g_ptr_array_index (old, old->len - 1) : NULL;
  new_target = (new) ? g_ptr_array_index (new, new->len - 1) : NULL;

  if (old)
    {
      for (i = MIN (_index, old->len - 1); i < old->len; i++)
        {
          area = g_ptr_array_index (old, i);
          g_hash_table_insert (change_map, area,
                               GUINT_TO_POINTER (CROSSING_LEAVE));
        }
    }

  if (new)
    {
      for (i = MIN (_index, new->len - 1); i < new->len; i++)
        {
          area = g_ptr_array_index (new, i);
          flags = GPOINTER_TO_UINT (g_hash_table_lookup (change_map, area));

          if (flags != 0)
            {
              /* If the old target appears on both lists but is
               * no longer the target, send 2 events to notify
               * about the obscure state change
               */
              if ((area == old_target) != (area == new_target))
                flags = CROSSING_ENTER | CROSSING_LEAVE;
              else
                {
                  g_hash_table_remove (change_map, area);
                  continue;
                }
            }
          else
            flags = CROSSING_ENTER;

          g_hash_table_insert (change_map, area, GUINT_TO_POINTER (flags));
        }
    }

  return change_map;
}

static void
_mech_window_send_crossing (MechWindow *window,
                            MechArea   *area,
                            MechArea   *target,
                            MechArea   *other,
                            MechEvent  *base,
                            gboolean    in,
                            gboolean    obscured)
{
  MechEvent *event;

  event = mech_event_new ((in) ? MECH_ENTER : MECH_LEAVE);

  event->any.area = g_object_ref (area);
  event->any.target = g_object_ref (target);
  event->any.seat = mech_event_get_seat (base);
  event->crossing.mode = MECH_CROSSING_NORMAL;

  if (other)
    event->crossing.other_area = g_object_ref (other);

  if (obscured)
    event->any.flags |= MECH_EVENT_FLAG_CROSSING_OBSCURED;

  _mech_area_handle_event (area, event);
  mech_event_free (event);
}

static void
_mech_window_change_crossing_stack (MechWindow *window,
                                    GPtrArray  *areas,
                                    MechEvent  *base_event)
{
  MechArea *area, *old_target, *new_target;
  GHashTable *diff_map = NULL;
  MechWindowPrivate *priv;
  gint i, _index = 0;
  gboolean obscured;
  GPtrArray *old;
  guint flags;

  priv = mech_window_get_instance_private (window);
  old = priv->pointer_info.crossing_areas;
  _index = _find_common_parent_index (old, areas);

  if (old && areas && old->len == areas->len && _index == old->len)
    return;

  /* Topmost areas need to be checked for obscure state changes */
  if (old && areas && (_index == old->len || _index == areas->len))
    _index--;

  if (old && areas)
    diff_map = _window_diff_crossing_stack (old, areas, _index);

  old_target = (old) ? g_ptr_array_index (old, old->len - 1) : NULL;
  new_target = (areas) ? g_ptr_array_index (areas, areas->len - 1) : NULL;

  if (old)
    {
      for (i = old->len - 1; i >= _index; i--)
        {
          area = g_ptr_array_index (old, i);

          if (diff_map)
            {
              flags = GPOINTER_TO_UINT (g_hash_table_lookup (diff_map, area));

              if ((flags & CROSSING_LEAVE) == 0)
                continue;
            }

          obscured = area != old_target;
          _mech_window_send_crossing (window, area, old_target, new_target,
                                      base_event, FALSE, obscured);
        }
    }

  if (areas)
    {
      for (i = 0; i < areas->len; i++)
        {
          area = g_ptr_array_index (areas, i);

          if (diff_map)
            {
              flags = GPOINTER_TO_UINT (g_hash_table_lookup (diff_map, area));

              if ((flags & CROSSING_ENTER) == 0)
                continue;
            }

          obscured = area != new_target;
          _mech_window_send_crossing (window, area, new_target, old_target,
                                      base_event, TRUE, obscured);
        }
    }

  if (diff_map)
    g_hash_table_destroy (diff_map);

  if (priv->pointer_info.crossing_areas)
    {
      g_ptr_array_unref (priv->pointer_info.crossing_areas);
      priv->pointer_info.crossing_areas = NULL;
    }

  if (areas)
    priv->pointer_info.crossing_areas = g_ptr_array_ref (areas);
}

static gboolean
_mech_window_handle_crossing (MechWindow *window,
                              MechEvent  *event)
{
  GPtrArray *crossing_areas = NULL;
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  /* A grab is in effect */
  if (priv->pointer_info.grab_areas)
    return FALSE;

  if (event->type == MECH_ENTER ||
      event->type == MECH_MOTION ||
      event->type == MECH_BUTTON_RELEASE)
    crossing_areas = _mech_stage_pick_for_event (priv->stage,
                                                 NULL, MECH_ENTER,
                                                 event->pointer.x,
                                                 event->pointer.y);
  else if (event->type != MECH_LEAVE)
    return FALSE;

  _mech_window_change_crossing_stack (window, crossing_areas, event);

  if (crossing_areas)
    g_ptr_array_unref (crossing_areas);

  return TRUE;
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
      _mech_window_handle_crossing (window, event);
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

  if (_mech_window_handle_crossing (window, event))
    {
      if (event->type == MECH_ENTER ||
          event->type == MECH_LEAVE)
        return TRUE;
    }

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
  g_object_class_install_property (object_class,
                                   PROP_STATE,
                                   g_param_spec_enum ("state",
                                                      "State",
                                                      "Window state",
                                                      MECH_TYPE_WINDOW_STATE,
                                                      MECH_WINDOW_STATE_NORMAL,
                                                      G_PARAM_READWRITE |
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
mech_window_push_state (MechWindow      *window,
                        MechWindowState  state,
                        MechMonitor     *monitor)
{
  MechWindowPrivate *priv;
  MechStateData data;

  g_return_if_fail (MECH_IS_WINDOW (window));
  g_return_if_fail (!monitor || MECH_IS_MONITOR (monitor));

  priv = mech_window_get_instance_private (window);

  if (!priv->state)
    priv->state = g_array_new (FALSE, FALSE, sizeof (MechStateData));

  data.state = state;
  data.monitor = monitor;
  mech_window_get_size (window, &data.previous_width, &data.previous_height);
  g_array_append_val (priv->state, data);

  MECH_WINDOW_GET_CLASS (window)->apply_state (window, state, monitor);
}

void
mech_window_pop_state (MechWindow *window)
{
  MechWindowState state = MECH_WINDOW_STATE_NORMAL;
  gint previous_width, previous_height;
  MechMonitor *monitor = NULL;
  MechWindowPrivate *priv;
  MechStateData *data;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);

  if (!priv->state || priv->state->len == 0)
    return;

  data = &g_array_index (priv->state, MechStateData,
                         priv->state->len - 1);
  previous_width = data->previous_width;
  previous_height = data->previous_height;

  g_array_remove_index (priv->state, priv->state->len - 1);

  if (priv->state->len != 0)
    {
      data = &g_array_index (priv->state, MechStateData,
                             priv->state->len - 1);
      state = data->state;
      monitor = data->monitor;
    }

  MECH_WINDOW_GET_CLASS (window)->apply_state (window, state, monitor);

  if (state == MECH_WINDOW_STATE_NORMAL)
    mech_window_set_size (window, previous_width, previous_height);
}

MechWindowState
mech_window_get_state (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), MECH_WINDOW_STATE_NORMAL);

  priv = mech_window_get_instance_private (window);

  if (!priv->state || priv->state->len == 0)
    return MECH_WINDOW_STATE_NORMAL;

  return g_array_index (priv->state, MechStateData,
                        priv->state->len - 1).state;
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
