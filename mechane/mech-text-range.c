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

#include <mechane/mech-text-buffer.h>
#include <mechane/mech-text-range.h>
#include "mech-marshal.h"

typedef struct _MechTextRangePrivate MechTextRangePrivate;

enum {
  PROP_START = 1,
  PROP_END
};

enum {
  ADDED,
  REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _MechTextRangePrivate
{
  MechTextBuffer *buffer;
  guint start_mark_id;
  guint end_mark_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechTextRange, mech_text_range, G_TYPE_OBJECT)

static void
mech_text_range_class_init (MechTextRangeClass *klass)
{
  signals[ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechTextRangeClass, added),
                  NULL, NULL,
                  _mech_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE, 2,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
  signals[REMOVED] =
    g_signal_new ("removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechTextRangeClass, removed),
                  NULL, NULL,
                  _mech_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE, 2,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
mech_text_range_init (MechTextRange *range)
{
}

MechTextRange *
mech_text_range_new (void)
{
  return g_object_new (MECH_TYPE_TEXT_RANGE, NULL);
}

static void
_mech_text_range_emit (MechTextRange *range,
                       MechTextIter  *old_start,
                       MechTextIter  *old_end,
                       MechTextIter  *start,
                       MechTextIter  *end)
{
  if (mech_text_buffer_iter_compare (start, old_start) == 0 &&
      mech_text_buffer_iter_compare (end, old_end) == 0)
    return;

  if (mech_text_buffer_iter_compare (old_start, old_end) == 0 &&
      mech_text_buffer_iter_compare (start, end) != 0)
    g_signal_emit (range, signals[ADDED], 0, start, end);

  if (mech_text_buffer_iter_compare (end, old_start) <= 0 ||
      mech_text_buffer_iter_compare (start, old_end) >= 0)
    {
      /* Old and new ranges are disconnected sets */
      g_signal_emit (range, signals[REMOVED], 0, old_start, old_end);
      g_signal_emit (range, signals[ADDED], 0, start, end);
    }
  else
    {
      /* Both sets intersect */
      if (mech_text_buffer_iter_compare (old_start, start) < 0 &&
          mech_text_buffer_iter_compare (old_end, start) > 0)
        g_signal_emit (range, signals[REMOVED], 0, old_start, start);

      if (mech_text_buffer_iter_compare (old_start, end) < 0 &&
          mech_text_buffer_iter_compare (old_end, end) > 0)
        g_signal_emit (range, signals[REMOVED], 0, end, old_end);

      if (mech_text_buffer_iter_compare (start, old_start) < 0 &&
          mech_text_buffer_iter_compare (end, old_start) > 0)
        g_signal_emit (range, signals[ADDED], 0, start, old_start);

      if (mech_text_buffer_iter_compare (start, old_end) < 0 &&
          mech_text_buffer_iter_compare (end, old_end) > 0)
        g_signal_emit (range, signals[ADDED], 0, old_end, end);
    }
}

void
mech_text_range_set_bounds (MechTextRange      *range,
                            const MechTextIter *start,
                            const MechTextIter *end)
{
  MechTextIter old_start, old_end, new_start, new_end;
  MechTextRangePrivate *priv;

  g_return_if_fail (MECH_IS_TEXT_RANGE (range));
  g_return_if_fail (start && end);
  g_return_if_fail (start->buffer == end->buffer);

  /* Ensure iter ordering */
  if (mech_text_buffer_iter_compare (start, end) <= 0)
    {
      new_start = *start;
      new_end = *end;
    }
  else
    {
      new_start = *end;
      new_end = *start;
    }

  priv = mech_text_range_get_instance_private (range);

  if (priv->buffer &&
      (priv->buffer != new_end.buffer ||
       priv->buffer != new_start.buffer))
    {
      g_warning ("%s: switching buffer on a range is not allowed", G_STRFUNC);
      return;
    }
  else if (!priv->buffer)
    priv->buffer = new_start.buffer;

  if (!priv->start_mark_id && !priv->start_mark_id)
    {
      priv->start_mark_id = mech_text_buffer_create_mark (priv->buffer,
                                                          &new_start);
      priv->end_mark_id = mech_text_buffer_create_mark (priv->buffer, &new_end);
      old_start = old_end = new_start;
    }
  else
    {
      mech_text_buffer_get_iter_at_mark (priv->buffer,
                                         priv->start_mark_id, &old_start);
      mech_text_buffer_get_iter_at_mark (priv->buffer,
                                         priv->end_mark_id, &old_end);

      mech_text_buffer_update_mark (priv->buffer, priv->start_mark_id, &new_start);
      mech_text_buffer_update_mark (priv->buffer, priv->end_mark_id, &new_end);
    }

  _mech_text_range_emit (range, &old_start, &old_end, &new_start, &new_end);
}

gboolean
mech_text_range_get_bounds (MechTextRange *range,
                            MechTextIter  *start,
                            MechTextIter  *end)
{
  MechTextIter start_iter, end_iter;
  MechTextRangePrivate *priv;

  g_return_val_if_fail (MECH_IS_TEXT_RANGE (range), FALSE);

  priv = mech_text_range_get_instance_private (range);

  if (!priv->start_mark_id || !priv->end_mark_id)
    return FALSE;

  mech_text_buffer_get_iter_at_mark (priv->buffer,
                                     priv->start_mark_id, &start_iter);
  mech_text_buffer_get_iter_at_mark (priv->buffer,
                                     priv->end_mark_id, &end_iter);
  if (start)
    *start = start_iter;

  if (end)
    *end = end_iter;

  return mech_text_buffer_iter_compare (&start_iter, &end_iter) != 0;
}
