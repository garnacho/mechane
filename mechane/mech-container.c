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

#include <cairo/cairo-gobject.h>

#include <mechane/mech-marshal.h>
#include <mechane/mech-stage-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-container.h>
#include <mechane/mechane.h>

enum {
  CROSSING_ENTER = 1 << 1,
  CROSSING_LEAVE = 1 << 2,
};

typedef struct _MechContainerPrivate MechContainerPrivate;
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

struct _MechContainerPrivate
{
  MechStage *stage;
  MechSurface *surface;
  MechArea *root;

  MechPointerInfo pointer_info;
  MechKeyboardInfo keyboard_info;
  GHashTable *touch_info;

  gint width;
  gint height;

  guint redraw_requested : 1;
  guint resize_requested : 1;
  guint size_initialized : 1;
};

enum {
  DRAW,
  UPDATE_NOTIFY,
  SIZE_CHANGED,
  GRAB_FOCUS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechContainer, mech_container,
                                     G_TYPE_OBJECT)

static void
mech_container_constructed (GObject *object)
{
  MechContainer *container = (MechContainer *) object;
  MechContainerPrivate *priv;
  MechContainerClass *klass;

  priv = mech_container_get_instance_private (container);
  klass = MECH_CONTAINER_GET_CLASS (container);

  if (!klass->create_root)
    g_warning (G_STRLOC
               ": %s has no MechContainerClass->create_root() implementation",
               G_OBJECT_CLASS_NAME (klass));
  else
    {
      priv->root = klass->create_root (container);
      _mech_area_set_container (priv->root, container);
      _mech_stage_set_root (priv->stage, priv->root);
    }
}

static void
mech_container_finalize (GObject *object)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private ((MechContainer *) object);

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

  g_object_unref (priv->stage);
  g_object_unref (priv->root);

  G_OBJECT_CLASS (mech_container_parent_class)->finalize (object);
}

static void
mech_container_draw (MechContainer  *container,
                     cairo_t        *cr)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private (container);
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
_mech_container_send_focus (MechContainer *container,
                            MechArea      *area,
                            MechArea      *target,
                            MechArea      *other,
                            MechSeat      *seat,
                            gboolean       in)
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
_mech_container_change_focus_stack (MechContainer *container,
                                    GPtrArray     *array,
                                    MechSeat      *seat)
{
  MechArea *area, *old_target, *new_target;
  MechContainerPrivate *priv;
  gint i, common_index;
  GPtrArray *old;

  priv = mech_container_get_instance_private (container);
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
          _mech_container_send_focus (container, area, old_target,
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
          _mech_container_send_focus (container, area, new_target,
                                      old_target, seat, TRUE);
        }

      priv->keyboard_info.focus_areas = array;
    }
}

static GHashTable *
_container_diff_crossing_stack (GPtrArray *old,
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
_mech_container_send_crossing (MechContainer *container,
                               MechArea      *area,
                               MechArea      *target,
                               MechArea      *other,
                               MechEvent     *base,
                               gboolean       in,
                               gboolean       obscured)
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
_mech_container_change_crossing_stack (MechContainer *container,
                                       GPtrArray     *areas,
                                       MechEvent     *base_event)
{
  MechArea *area, *old_target, *new_target;
  GHashTable *diff_map = NULL;
  MechContainerPrivate *priv;
  gint i, _index = 0;
  gboolean obscured;
  GPtrArray *old;
  guint flags;

  priv = mech_container_get_instance_private (container);
  old = priv->pointer_info.crossing_areas;
  _index = _find_common_parent_index (old, areas);

  if (old && areas && old->len == areas->len && _index == old->len)
    return;

  /* Topmost areas need to be checked for obscure state changes */
  if (old && areas && (_index == old->len || _index == areas->len))
    _index--;

  if (old && areas)
    diff_map = _container_diff_crossing_stack (old, areas, _index);

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
          _mech_container_send_crossing (container, area,
                                         old_target, new_target,
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
          _mech_container_send_crossing (container, area,
                                         new_target, old_target,
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
_mech_container_handle_crossing (MechContainer *container,
                                 MechEvent     *event)
{
  GPtrArray *crossing_areas = NULL;
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private (container);

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

  _mech_container_change_crossing_stack (container, crossing_areas, event);

  if (crossing_areas)
    g_ptr_array_unref (crossing_areas);

  return TRUE;
}

static gboolean
_mech_container_propagate_event (MechContainer *container,
                                 GPtrArray     *areas,
                                 MechEvent     *event)
{
  gboolean handled = FALSE;
  MechEvent *area_event;
  MechArea *area;
  gint i;

  for (i = 0; i < areas->len; i++)
    {
      area = g_ptr_array_index (areas, i);
      area_event = mech_event_translate (event, area);
      mech_event_set_target (area_event,
                             g_ptr_array_index (areas, areas->len - 1));
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
      mech_event_set_target (area_event,
                             g_ptr_array_index (areas, areas->len - 1));
      handled |= _mech_area_handle_event (area, area_event);
      mech_event_free (area_event);
      i--;
    }

  return handled;
}

static void
_mech_container_unset_pointer_grab (MechContainer *container)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private (container);

  if (priv->pointer_info.grab_areas)
    {
      g_ptr_array_unref (priv->pointer_info.grab_areas);
      priv->pointer_info.grab_areas = NULL;
    }
}

static void
_mech_container_update_seat_state (MechContainer *container,
                                   MechEvent     *event,
                                   GPtrArray     *areas,
                                   gboolean       handled)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private (container);

  if (handled && event->type == MECH_BUTTON_PRESS)
    priv->pointer_info.grab_areas = (handled && areas) ? g_ptr_array_ref (areas) : NULL;
  else if (event->type == MECH_BUTTON_RELEASE &&
           priv->pointer_info.grab_areas)
    {
      _mech_container_unset_pointer_grab (container);
      _mech_container_handle_crossing (container, event);
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
_mech_container_handle_event_internal (MechContainer *container,
                                       MechEvent     *event)
{
  MechContainerPrivate *priv;
  GPtrArray *areas = NULL;
  gboolean retval = FALSE;

  priv = mech_container_get_instance_private (container);

  if (_mech_container_handle_crossing (container, event))
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
      _mech_container_update_seat_state (container, event, NULL, retval);
      return FALSE;
    }
  else
    {
      retval = _mech_container_propagate_event (container, areas, event);
      _mech_container_update_seat_state (container, event, areas, retval);
      g_ptr_array_unref (areas);

      return retval;
    }
}

static void
mech_container_class_init (MechContainerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructed = mech_container_constructed;
  object_class->finalize = mech_container_finalize;

  klass->draw = mech_container_draw;
  klass->handle_event = _mech_container_handle_event_internal;

  signals[DRAW] =
    g_signal_new ("draw",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechContainerClass, draw),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CAIRO_GOBJECT_TYPE_CONTEXT | G_SIGNAL_TYPE_STATIC_SCOPE);
  signals[UPDATE_NOTIFY] =
    g_signal_new ("update-notify",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechContainerClass, update_notify),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  signals[SIZE_CHANGED] =
    g_signal_new ("size-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechContainerClass, size_changed),
                  NULL, NULL,
                  _mech_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  signals[GRAB_FOCUS] =
    g_signal_new ("grab-focus",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechContainerClass, grab_focus),
                  NULL, NULL,
                  _mech_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2, MECH_TYPE_AREA, MECH_TYPE_SEAT);
}

static void
mech_container_init (MechContainer *container)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private (container);
  priv->stage = _mech_stage_new ();
}

void
_mech_container_set_surface (MechContainer *container,
                             MechSurface   *surface)
{
  MechContainerPrivate *priv;
  gint width, height;

  priv = mech_container_get_instance_private (container);

  if (priv->surface)
    {
      g_object_unref (priv->surface);
      priv->surface = NULL;
    }

  if (surface)
    {
      priv->surface = g_object_ref (surface);
      g_object_set (surface, "area", priv->root, NULL);
      _mech_stage_set_root_surface (priv->stage, surface);
    }
}

MechSurface *
_mech_container_get_surface (MechContainer *container)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private (container);

  return priv->surface;
}

MechCursor *
_mech_container_get_current_cursor (MechContainer *container)
{
  MechContainerPrivate *priv;
  GPtrArray *areas = NULL;
  MechArea *area;

  priv = mech_container_get_instance_private (container);

  if (priv->pointer_info.grab_areas)
    areas = priv->pointer_info.grab_areas;
  else
    areas = priv->pointer_info.crossing_areas;

  if (!areas)
    return NULL;

  area = g_ptr_array_index (areas, areas->len - 1);
  return mech_area_get_cursor (area);
}

MechStage *
_mech_container_get_stage (MechContainer *container)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private (container);
  return priv->stage;
}

gboolean
mech_container_handle_event (MechContainer *container,
                             MechEvent     *event)
{
  MechContainerClass *container_class;

  container_class = MECH_CONTAINER_GET_CLASS (container);

  if (!container_class->handle_event)
    return FALSE;

  return container_class->handle_event (container, event);
}

void
mech_container_queue_redraw (MechContainer *container)
{
  MechContainerPrivate *priv;

  g_return_val_if_fail (MECH_IS_CONTAINER (container), FALSE);

  priv = mech_container_get_instance_private (container);
  priv->redraw_requested = TRUE;
  g_signal_emit (container, signals[UPDATE_NOTIFY], 0);
}

void
mech_container_queue_resize (MechContainer *container,
                             gint           width,
                             gint           height)
{
  gint stage_width, stage_height;
  MechContainerPrivate *priv;

  g_return_if_fail (MECH_IS_CONTAINER (container));

  priv = mech_container_get_instance_private (container);
  _mech_stage_get_size (priv->stage, &stage_width, &stage_height);

  width = MAX (stage_width, width);
  height = MAX (stage_height, height);

  if (priv->width != width || priv->height != height)
    {
      priv->size_initialized = TRUE;
      priv->resize_requested = TRUE;
      priv->width = width;
      priv->height = height;

      g_signal_emit (container, signals[UPDATE_NOTIFY], 0);
    }
}

void
mech_container_process_updates (MechContainer *container)
{
  MechContainerPrivate *priv;
  cairo_t *cr;

  priv = mech_container_get_instance_private (container);

  if (!priv->surface || (!priv->resize_requested && !priv->redraw_requested))
    return;

  /* This call may modify the underlying surfaces */
  if (priv->resize_requested)
    {
      _mech_stage_set_size (priv->stage, &priv->width, &priv->height);
      g_signal_emit (container, signals[SIZE_CHANGED],
                     0, priv->width, priv->height);
    }

  cr = _mech_surface_cairo_create (priv->surface);

  g_signal_emit (container, signals[DRAW], 0, cr);

  if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
    g_warning ("Cairo context got error '%s'",
               cairo_status_to_string (cairo_status (cr)));

  priv->resize_requested = FALSE;
  priv->redraw_requested = FALSE;

  cairo_destroy (cr);
}

gboolean
mech_container_get_size (MechContainer *container,
                         gint          *width,
                         gint          *height)
{
  MechContainerPrivate *priv;

  g_return_if_fail (MECH_IS_CONTAINER (container));

  priv = mech_container_get_instance_private (container);

  if (priv->size_initialized)
    {
      if (width)
        *width = priv->width;
      if (height)
        *height = priv->height;
    }
  else
    _mech_stage_get_size (priv->stage, width, height);

  return priv->size_initialized;
}

MechArea *
mech_container_get_root (MechContainer *container)
{
  MechContainerPrivate *priv;

  g_return_val_if_fail (MECH_IS_CONTAINER (container), NULL);

  priv = mech_container_get_instance_private (container);

  return priv->root;
}

MechArea *
mech_container_get_focus (MechContainer *container)
{
  MechContainerPrivate *priv;

  g_return_val_if_fail (MECH_IS_CONTAINER (container), NULL);

  priv = mech_container_get_instance_private (container);

  if (!priv->keyboard_info.focus_areas)
    return NULL;

  return g_ptr_array_index (priv->keyboard_info.focus_areas,
                            priv->keyboard_info.focus_areas->len - 1);
}

void
mech_container_grab_focus (MechContainer *container,
                           MechArea      *area,
                           MechSeat      *seat)
{
  GPtrArray *focus_areas;
  guint n_elements;
  GNode *node;

  g_return_if_fail (MECH_IS_CONTAINER (container));
  g_return_if_fail (MECH_IS_AREA (area));

  if (area == mech_container_get_focus (container))
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
  _mech_container_change_focus_stack (container, focus_areas, seat);

  g_signal_emit (container, signals[GRAB_FOCUS], 0,
                 mech_container_get_focus (container),
                 seat);
}
