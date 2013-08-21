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

#include <cairo/cairo-gobject.h>

#include <mechane/mech-marshal.h>
#include <mechane/mech-backend-private.h>
#include <mechane/mech-stage-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-window-frame-private.h>
#include <mechane/mech-clock-private.h>
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
  MechSurface *surface;
  MechClock *clock;
  MechArea *frame;
  MechStyle *style;

  MechPointerInfo pointer_info;
  MechKeyboardInfo keyboard_info;
  GHashTable *touch_info;

  GArray *state;
  gchar *title;

  gint width;
  gint height;

  guint resizable        : 1;
  guint visible          : 1;
  guint redraw_requested : 1;
  guint resize_requested : 1;
  guint size_initialized : 1;
};

enum {
  DRAW,
  SIZE_CHANGED,
  CLOSE_REQUEST,
  LAST_SIGNAL
};

enum {
  PROP_TITLE = 1,
  PROP_RESIZABLE,
  PROP_VISIBLE,
  PROP_MONITOR,
  PROP_STATE,
  PROP_STYLE
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechWindow, mech_window,
                                     MECH_TYPE_CONTAINER)

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
    case PROP_STYLE:
      g_value_set_object (value, priv->style);
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
    case PROP_STYLE:
      mech_window_set_style (window, g_value_get_object (value));
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

  if (priv->surface)
    g_object_unref (priv->surface);

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

  g_object_unref (priv->style);
  g_object_unref (priv->stage);
  g_signal_handlers_disconnect_by_data (priv->frame, object);
  _mech_area_set_window (priv->frame, NULL);
  g_object_unref (priv->frame);

  G_OBJECT_CLASS (mech_window_parent_class)->finalize (object);
}

static void
mech_window_draw (MechWindow *window,
                  cairo_t    *cr)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  _mech_stage_render (priv->stage, cr);
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

static void
_mech_window_send_focus (MechWindow *window,
                         MechArea   *area,
                         MechArea   *target,
                         MechArea   *other,
                         MechSeat   *seat,
                         gboolean    in)
{
  MechEvent *event;

  event = mech_event_new ((in) ? MECH_FOCUS_IN : MECH_FOCUS_OUT);
  event->any.area = g_object_ref (area);
  event->any.target = g_object_ref (target);
  event->any.seat = seat;

  if (other)
    event->crossing.other_area = g_object_ref (other);

  _mech_area_handle_event (area, event);
  mech_event_free (event);
}

static void
_mech_window_change_focus_stack (MechWindow *window,
                                 GPtrArray  *array,
                                 MechSeat   *seat)
{
  MechArea *area, *old_target, *new_target;
  MechWindowPrivate *priv;
  gint i, common_index;
  GPtrArray *old;

  priv = mech_window_get_instance_private (window);
  old_target = new_target = NULL;
  old = priv->keyboard_info.focus_areas;
  common_index = _find_common_parent_index (old, array);

  if (old)
    old_target = g_ptr_array_index (old, old->len - 1);
  if (array)
    new_target = g_ptr_array_index (array, array->len - 1);

  if (old)
    {
      for (i = old->len - 1; i >= common_index; i--)
        {
          area = g_ptr_array_index (old, i);
          _mech_window_send_focus (window, area, old_target,
                                   new_target, seat, FALSE);
        }

      g_ptr_array_unref (priv->keyboard_info.focus_areas);
      priv->keyboard_info.focus_areas = NULL;
    }

  if (array)
    {
      for (i = common_index; i < array->len; i++)
        {
          area = g_ptr_array_index (array, i);
          _mech_window_send_focus (window, area, new_target,
                                   old_target, seat, TRUE);
        }

      priv->keyboard_info.focus_areas = array;
    }
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
window_frame_close (MechWindowFrame *frame,
                    MechWindow      *window)
{
  gboolean retval;

  g_object_ref (window);
  g_signal_emit (window, signals[CLOSE_REQUEST], 0, &retval);

  if (!retval)
    mech_window_set_visible (window, FALSE);

  g_object_unref (window);
}

static void
window_frame_maximize_toggle (MechWindowFrame *frame,
                              GParamSpec      *pspec,
                              MechWindow      *window)
{
  if (mech_window_frame_get_maximized (frame))
    mech_window_push_state (window, MECH_WINDOW_STATE_MAXIMIZED, NULL);
  else
    mech_window_pop_state (window);
}

static MechArea *
mech_window_create_root (MechContainer *container)
{
  MechWindow *window = (MechWindow *) container;
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  priv->frame = mech_window_frame_new ();

  _mech_area_set_window (priv->frame, window);
  _mech_stage_set_root (priv->stage, priv->frame);

  g_signal_connect_swapped (priv->frame, "move",
                            G_CALLBACK (mech_window_move), window);
  g_signal_connect_swapped (priv->frame, "resize",
                            G_CALLBACK (mech_window_resize), window);
  g_signal_connect (priv->frame, "close",
                    G_CALLBACK (window_frame_close), window);
  g_signal_connect (priv->frame, "notify::maximized",
                    G_CALLBACK (window_frame_maximize_toggle), window);

  return g_object_ref (priv->frame);
}

static void
mech_window_class_init (MechWindowClass *klass)
{
  MechContainerClass *container_class = (MechContainerClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->get_property = mech_window_get_property;
  object_class->set_property = mech_window_set_property;
  object_class->finalize = mech_window_finalize;

  container_class->create_root = mech_window_create_root;

  klass->draw = mech_window_draw;
  klass->handle_event = _mech_window_handle_event_internal;

  signals[DRAW] =
    g_signal_new ("draw",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowClass, draw),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CAIRO_GOBJECT_TYPE_CONTEXT);
  signals[SIZE_CHANGED] =
    g_signal_new ("size-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowClass, size_changed),
                  NULL, NULL,
                  _mech_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  signals[CLOSE_REQUEST] =
    g_signal_new ("close-request",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowClass, close_request),
                  g_signal_accumulator_true_handled, NULL,
                  _mech_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

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
  g_object_class_install_property (object_class,
                                   PROP_STYLE,
                                   g_param_spec_object ("style",
                                                        "Style",
                                                        "Style used to render "
                                                        "the window contents",
                                                        MECH_TYPE_STYLE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static MechStyle *
_load_default_style (void)
{
  GError *error = NULL;
  MechTheme *theme;
  MechStyle *style;
  GFile *file;

  style = mech_style_new ();
  theme = mech_theme_new ();

  file = g_file_new_for_uri ("resource://org/mechane/libmechane/DefaultTheme/style");
  mech_theme_load_style_from_file (theme, style, file, &error);
  g_object_unref (file);
  g_object_unref (theme);

  if (error)
    {
      g_warning ("Error loading default theme: %s\n", error->message);
      g_object_unref (style);
      g_error_free (error);
      return NULL;
    }

  return style;
}

static void
mech_window_init (MechWindow *window)
{
  MechWindowPrivate *priv;
  MechStyle *style;

  priv = mech_window_get_instance_private (window);
  priv->resizable = TRUE;
  priv->stage = _mech_stage_new ();

  style = _load_default_style ();

  if (style)
    {
      mech_window_set_style (window, style);
      g_object_unref (style);
    }
}

MechWindow *
mech_window_new (void)
{
  MechBackend *backend;

  backend = mech_backend_get ();
  g_assert (backend != NULL);

  return mech_backend_create_window (backend);
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
  mech_window_frame_set_resizable (MECH_WINDOW_FRAME (priv->frame), resizable);
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

static void
_mech_window_queue_resize (MechWindow *window,
                           gint        width,
                           gint        height)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  if (priv->resize_requested)
    return;

  priv->size_initialized = TRUE;
  priv->resize_requested = TRUE;
  priv->width = width;
  priv->height = height;

  if (priv->clock)
    _mech_clock_tick (priv->clock);
}

void
mech_window_set_visible (MechWindow *window,
                         gboolean    visible)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);

  if (!priv->visible && visible &&
      !priv->size_initialized)
    {
      gint width, height;

      _mech_stage_get_size (priv->stage, &width, &height);
      _mech_window_queue_resize (window, width, height);
    }

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
  mech_text_set_string (MECH_TEXT (priv->frame), title);
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

  if (priv->size_initialized)
    {
      if (width)
        *width = priv->width;
      if (height)
        *height = priv->height;
    }
  else
    _mech_stage_get_size (priv->stage, width, height);
}

void
mech_window_set_size (MechWindow *window,
                      gint        width,
                      gint        height)
{
  gint stage_width, stage_height;
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  _mech_stage_get_size (priv->stage, &stage_width, &stage_height);

  width = MAX (stage_width, width);
  height = MAX (stage_height, height);

  if (priv->width != width || priv->height != height)
    {
      _mech_window_queue_resize (window, width, height);
      g_signal_emit ((GObject *) window, signals[SIZE_CHANGED],
                     0, width, height);
    }
}

void
mech_window_queue_draw (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  priv->redraw_requested = TRUE;

  if (priv->clock)
    _mech_clock_tick (priv->clock);
}

void
_mech_window_process_updates (MechWindow *window)
{
  MechWindowPrivate *priv;
  cairo_region_t *region;
  cairo_t *cr;

  priv = mech_window_get_instance_private (window);

  if (!priv->visible ||
      (!priv->resize_requested && !priv->redraw_requested))
    return;

  /* This call may modify the underlying surfaces */
  if (priv->resize_requested)
    _mech_stage_set_size (priv->stage, &priv->width, &priv->height);

  cr = _mech_surface_cairo_create (priv->surface);
  _mech_surface_acquire (priv->surface);

  if (priv->resize_requested)
    {
      cairo_save (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);
      cairo_restore (cr);
    }

  g_signal_emit (window, signals[DRAW], 0, cr);

  region = _mech_surface_get_clip (priv->surface);
  MECH_WINDOW_GET_CLASS (window)->push_update (window, region);
  cairo_region_destroy (region);

  if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
    g_warning ("Cairo context got error '%s'",
               cairo_status_to_string (cairo_status (cr)));

  _mech_surface_release (priv->surface);
  priv->resize_requested = FALSE;
  priv->redraw_requested = FALSE;
  cairo_destroy (cr);
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

      if (priv->resize_requested || priv->redraw_requested)
        _mech_clock_tick (clock);
    }
}

MechClock *
_mech_window_get_clock (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  return priv->clock;
}

MechCursor *
_mech_window_get_current_cursor (MechWindow *window)
{
  MechWindowPrivate *priv;
  GPtrArray *areas = NULL;
  MechArea *area;

  priv = mech_window_get_instance_private (window);

  if (priv->pointer_info.grab_areas)
    areas = priv->pointer_info.grab_areas;
  else
    areas = priv->pointer_info.crossing_areas;

  if (!areas)
    return NULL;

  area = g_ptr_array_index (areas, areas->len - 1);
  return mech_area_get_cursor (area);
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

void
_mech_window_set_surface (MechWindow  *window,
                          MechSurface *surface)
{
  MechWindowPrivate *priv;
  gint width, height;

  priv = mech_window_get_instance_private (window);

  if (priv->surface)
    {
      g_object_unref (priv->surface);
      priv->surface = NULL;
    }

  priv->surface = g_object_ref (surface);
  g_object_set (surface, "area", priv->frame, NULL);
  mech_window_get_size (window, &width, &height);
  _mech_surface_set_size (surface, width, height);

  _mech_stage_set_root_surface (priv->stage, surface);
}

MechStage *
_mech_window_get_stage (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  return priv->stage;
}

void
_mech_window_grab_focus (MechWindow *window,
			 MechArea   *area,
                         MechSeat   *seat)
{
  GPtrArray *focus_areas;
  guint n_elements;
  GNode *node;

  g_return_if_fail (MECH_IS_WINDOW (window));
  g_return_if_fail (MECH_IS_AREA (area));

  if (area == mech_window_get_focus (window))
    return;

  node = _mech_area_get_node (area);
  n_elements = g_node_depth (node);
  focus_areas = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  g_ptr_array_set_size (focus_areas, n_elements);

  while (node && n_elements > 0)
    {
      gpointer *elem;

      n_elements--;
      elem = &g_ptr_array_index (focus_areas, n_elements);
      *elem = g_object_ref (node->data);
      node = node->parent;
    }

  g_assert (!node && n_elements == 0);
  _mech_window_change_focus_stack (window, focus_areas, seat);
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

void
mech_window_set_style (MechWindow *window,
                       MechStyle  *style)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));
  g_return_if_fail (MECH_IS_STYLE (style));

  priv = mech_window_get_instance_private (window);

  if (style)
    g_object_ref (style);

  if (priv->style)
    g_object_unref (priv->style);

  priv->style = style;
  g_object_notify ((GObject *) window, "style");
}

MechStyle *
mech_window_get_style (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), NULL);

  priv = mech_window_get_instance_private (window);
  return priv->style;
}
