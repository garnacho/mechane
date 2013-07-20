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

#include <xkbcommon/xkbcommon-keysyms.h>
#include "mech-text.h"
#include "mech-text-input.h"

typedef struct _MechTextInputPrivate MechTextInputPrivate;

struct _MechTextInputPrivate
{
  guint insertion_mark_id;
  gdouble last_ins_x;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechTextInput, mech_text_input, MECH_TYPE_TEXT_VIEW)

static void
mech_text_input_constructed (GObject *object)
{
  mech_area_add_events (MECH_AREA (object),
                        MECH_KEY_MASK | MECH_FOCUS_MASK |
                        MECH_CROSSING_MASK | MECH_BUTTON_MASK);
}

static void
mech_text_input_notify (GObject    *object,
                        GParamSpec *pspec)
{
  if (pspec->value_type == MECH_TYPE_TEXT_BUFFER)
    {
      MechTextInputPrivate *priv;
      MechTextBuffer *buffer;

      priv = mech_text_input_get_instance_private ((MechTextInput *) object);
      buffer = mech_text_get_buffer (MECH_TEXT (object));

      if (buffer && !priv->insertion_mark_id)
        {
          MechTextIter end;

          mech_text_buffer_get_bounds (buffer, NULL, &end);
          priv->insertion_mark_id = mech_text_buffer_create_mark (buffer, &end);
        }
      else if (!buffer)
        priv->insertion_mark_id = 0;
    }
}

static void
mech_text_input_draw (MechArea *area,
                      cairo_t  *cr)
{
  MechTextView *view = MECH_TEXT_VIEW (area);
  cairo_rectangle_t strong, weak;
  MechTextInputPrivate *priv;
  MechTextBuffer *buffer;
  MechTextIter ins;

  priv = mech_text_input_get_instance_private ((MechTextInput *) area);
  MECH_AREA_CLASS (mech_text_input_parent_class)->draw (area, cr);

  buffer = mech_text_get_buffer (MECH_TEXT (area));

  if (!mech_text_buffer_get_iter_at_mark (buffer,
                                          priv->insertion_mark_id,
                                          &ins))
    return;

  mech_text_view_get_cursor_locations (view, &ins, &strong, &weak);

  cairo_save (cr);
  cairo_set_line_width (cr, 1);
  cairo_rectangle (cr, strong.x + 0.5, strong.y, strong.width, strong.height);
  cairo_stroke (cr);
  cairo_restore (cr);
}

static gboolean
mech_text_input_handle_event (MechArea  *area,
                              MechEvent *event)
{
  MechTextInputPrivate *priv;
  MechTextBuffer *buffer;
  cairo_rectangle_t rect;
  gboolean update_ins_x;
  MechTextView *view;
  MechTextIter ins;
  MechPoint point;

  if (mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  priv = mech_text_input_get_instance_private ((MechTextInput *) area);
  view = MECH_TEXT_VIEW (area);
  buffer = mech_text_get_buffer (MECH_TEXT (area));

  switch (event->type)
    {
    case MECH_KEY_PRESS:
      if (!buffer || !priv->insertion_mark_id)
        return TRUE;

      if (!mech_text_buffer_get_iter_at_mark (buffer,
                                              priv->insertion_mark_id,
                                              &ins))
        return TRUE;

      update_ins_x = TRUE;

      if (event->key.keyval == XKB_KEY_Return ||
               event->key.keyval == XKB_KEY_KP_Enter)
        mech_text_buffer_insert (buffer, &ins, "\n", 1);
      else if (event->key.keyval == XKB_KEY_BackSpace)
        {
          MechTextIter prev;

          prev = ins;
          mech_text_buffer_iter_previous (&prev, 1);
          mech_text_buffer_delete (buffer, &prev, &ins);
        }
      else if (event->key.keyval == XKB_KEY_Left)
        mech_text_buffer_iter_previous (&ins, 1);
      else if (event->key.keyval == XKB_KEY_Right)
        mech_text_buffer_iter_next (&ins, 1);
      else if (event->key.keyval == XKB_KEY_Up)
        {
          mech_text_view_get_cursor_locations (view, &ins, &rect, NULL);
          point.x = priv->last_ins_x;
          point.y = rect.y - 1;
          mech_text_view_get_iter_at_point (view, &point, &ins);
          update_ins_x = FALSE;
        }
      else if (event->key.keyval == XKB_KEY_Down)
        {
          mech_text_view_get_cursor_locations (view, &ins, &rect, NULL);
          point.x = priv->last_ins_x;
          point.y = rect.y + rect.height + 1;
          mech_text_view_get_iter_at_point (view, &point, &ins);
          update_ins_x = FALSE;
        }
      else if (event->key.unicode_char != 0)
        {
          gchar buf[6] = { 0 };
          gint len;

          len = g_unichar_to_utf8 (event->key.unicode_char, buf);
          mech_text_buffer_insert (buffer, &ins, buf, len);
        }

      else
        return FALSE;

      if (update_ins_x)
        {
          mech_text_view_get_cursor_locations (view, &ins, &rect, NULL);
          priv->last_ins_x = rect.x;
        }

      mech_text_buffer_update_mark (buffer, priv->insertion_mark_id, &ins);
      mech_area_redraw (area, NULL);
      break;
    case MECH_BUTTON_PRESS:
      if (buffer && priv->insertion_mark_id)
        {
          MechTextIter iter;

          mech_area_grab_focus (area, event->any.seat);
          point.x = event->pointer.x;
          point.y = event->pointer.y;

          if (mech_text_view_get_iter_at_point (view, &point, &iter))
            {
              mech_text_buffer_update_mark (buffer, priv->insertion_mark_id, &iter);
              mech_text_view_get_cursor_locations (view, &iter, &rect, NULL);
              priv->last_ins_x = rect.x;
              mech_area_redraw (area, NULL);
            }
        }
      break;
    default:
      break;
    }

  return TRUE;
}

static void
mech_text_input_class_init (MechTextInputClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = mech_text_input_constructed;
  object_class->notify = mech_text_input_notify;

  area_class->draw = mech_text_input_draw;
  area_class->handle_event = mech_text_input_handle_event;
}

static void
mech_text_input_init (MechTextInput *input)
{
  MechCursor *cursor;

  cursor = mech_cursor_lookup (MECH_CURSOR_TEXT_EDIT);
  mech_area_set_cursor ((MechArea *) input, cursor);
}

MechArea *
mech_text_input_new (void)
{
  return g_object_new (MECH_TYPE_TEXT_INPUT, NULL);
}
