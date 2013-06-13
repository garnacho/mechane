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

#include <gobject/gvaluecollector.h>
#include <string.h>
#include "mech-text-buffer.h"
#include "mech-marshal.h"

#define NODE_STRING(n) ((n)->stored->string->str + (n)->pos)
#define NODE_STRING_POS(n,p) (NODE_STRING(n) + (p))
#define ITER_STRING(i) (NODE_STRING_POS ((MechTextBufferNode*) g_sequence_get ((i)->iter), (i)->pos))
#define STRING_CHUNK_SOFT_LIMIT 65535

#define INITIALIZE_ITER(i,b,s,p) G_STMT_START { \
  (i)->buffer = (b);                            \
  (i)->iter = (s);                              \
  (i)->pos = (p);                               \
  } G_STMT_END

#define IS_VALID_ITER(i,b) ((b) ? i->buffer == (b) : MECH_IS_TEXT_BUFFER ((i)->buffer))

typedef struct _MechTextBufferPrivate MechTextBufferPrivate;
typedef struct _MechTextBufferNode MechTextBufferNode;
typedef struct _MechStoredString MechStoredString;
typedef struct _MechTextSeqIterator MechTextSeqIterator;

struct _MechStoredString
{
  GString *string;
  gint node_count;
};

struct _MechTextBufferNode
{
  MechStoredString *stored;
  gsize pos;
  gsize len;
};

struct _MechTextSeqIterator
{
  MechTextIter start;
  MechTextIter end;
  GSequenceIter *current;
  guint finished : 1;
};

struct _MechTextBufferPrivate
{
  GPtrArray *strings;
  GSequence *buffer;
};

enum {
  INSERT,
  DELETE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechTextBuffer, mech_text_buffer, G_TYPE_OBJECT)
G_DEFINE_BOXED_TYPE (MechTextIter, mech_text_iter,
                     mech_text_iter_copy, mech_text_iter_free);

MechTextIter *
mech_text_iter_copy (MechTextIter *iter)
{
  return g_slice_dup (MechTextIter, iter);
}

void
mech_text_iter_free (MechTextIter *iter)
{
  g_slice_free (MechTextIter, iter);
}

static MechStoredString *
_mech_stored_string_new (MechTextBuffer *buffer,
                         const gchar    *text,
                         gssize          len)
{
  MechStoredString *stored, *new = NULL;
  MechTextBufferPrivate *priv;
  guint i;

  priv = mech_text_buffer_get_instance_private (buffer);

  for (i = 0; i < priv->strings->len; i++)
    {
      stored = g_ptr_array_index (priv->strings, i);

      if (!stored->string)
        {
          new = stored;
          break;
        }
    }

  if (!new)
    {
      new = g_slice_new0 (MechStoredString);
      g_ptr_array_add (priv->strings, new);
    }

  new->string = g_string_new_len (text, len);

  return new;
}

static void
_mech_stored_string_append (MechStoredString  *stored,
                            const gchar       *text,
                            gssize             len,
                            const gchar      **string_start)
{
  gsize prev_len = stored->string->len;

  g_string_append_len (stored->string, text, len);

  if (string_start)
    *string_start = stored->string->str + prev_len;
}

static void
_mech_stored_string_free (MechStoredString *stored)
{
  if (stored->string)
    g_string_free (stored->string, TRUE);
  g_slice_free (MechStoredString, stored);
}

static MechTextBufferNode *
_mech_text_buffer_node_new (MechStoredString *stored,
                            gsize             pos,
                            gint              len)
{
  MechTextBufferNode *node;
  guint i;

  node = g_slice_new0 (MechTextBufferNode);
  node->stored = stored;
  node->pos = pos;
  node->len = len;

  g_atomic_int_inc (&stored->node_count);

  return node;
}

static void
_mech_text_buffer_node_free (MechTextBufferNode *node)
{
  MechStoredString *stored;
  guint i;

  stored = node->stored;

  g_slice_free (MechTextBufferNode, node);

  if (g_atomic_int_dec_and_test (&stored->node_count))
    {
      g_string_free (stored->string, TRUE);
      stored->string = NULL;
    }
}

static gboolean
_mech_text_buffer_node_split (MechTextBuffer  *buffer,
                              GSequenceIter   *iter,
                              gsize            split_pos,
                              GSequenceIter  **end)
{
  GSequenceIter *next, *end_iter;
  MechTextBufferNode *node, *new;

  if (end)
    *end = iter;

  if (g_sequence_iter_is_end (iter))
    return FALSE;

  next = g_sequence_iter_next (iter);
  node = g_sequence_get (iter);

  if (split_pos == 0 || split_pos > node->len)
    return FALSE;

  /* Create node for text after cut_end */
  new = _mech_text_buffer_node_new (node->stored,
                                    node->pos + split_pos,
                                    node->len - split_pos);
  end_iter = g_sequence_insert_before (next, new);

  /* Trim text after cur_start in original node */
  node->len = split_pos;

  if (end)
    *end = end_iter;

  return TRUE;
}

static void
_mech_text_buffer_delete_nodes (MechTextBuffer *buffer,
                                GSequenceIter  *start,
                                GSequenceIter  *end)
{
  g_sequence_remove_range (start, end);
}

static gboolean
_text_buffer_is_line_terminator (gunichar ch)
{
#define PARAGRAPH_SEPARATOR 0x2029
  return ch == '\n' || ch == '\r' || ch == PARAGRAPH_SEPARATOR;
#undef PARAGRAPH_SEPARATOR
}

static gboolean
_text_buffer_iter_node_terminates_paragraph (GSequenceIter *iter)
{
  MechTextBufferNode *node;
  gunichar ch;
  gchar *ptr;

  if (g_sequence_iter_is_end (iter))
    return TRUE;

  node = g_sequence_get (iter);
  ptr = NODE_STRING_POS (node, node->len);
  ch = g_utf8_get_char (g_utf8_prev_char (ptr));

  return _text_buffer_is_line_terminator (ch);
}

static MechStoredString *
_text_buffer_intern_string (MechTextBuffer  *buffer,
                            MechTextIter    *iter,
                            const gchar     *text,
                            gssize           len,
                            const gchar    **start)
{
  MechStoredString *stored = NULL;

  if (iter && iter->pos == 0 && !g_sequence_iter_is_begin (iter->iter))
    {
      MechTextBufferNode *node;
      GSequenceIter *prev;

      prev = g_sequence_iter_prev (iter->iter);
      node = g_sequence_get (prev);

      if (node->pos + node->len == node->stored->string->len &&
          node->stored->string->len < STRING_CHUNK_SOFT_LIMIT)
        {
          stored = node->stored;
          _mech_stored_string_append (stored, text, len, start);
        }
    }

  if (!stored)
    {
      stored = _mech_stored_string_new (buffer, text, len);

      if (start)
        *start = stored->string->str;
    }

  return stored;
}

static gboolean
_text_buffer_check_node_append (MechTextBuffer *buffer,
                                MechTextIter   *insert,
                                const gchar    *text,
                                gssize          len)
{
  MechTextBufferNode *node;
  GSequenceIter *prev;

  if (insert->pos != 0)
    return FALSE;

  if (g_sequence_iter_is_begin (insert->iter))
    return FALSE;

  prev = g_sequence_iter_prev (insert->iter);
  node = g_sequence_get (prev);
  g_assert (node != NULL);

  if (node->stored->string->str + node->pos + node->len != text)
    return FALSE;

  if (_text_buffer_iter_node_terminates_paragraph (prev) &&
      !_text_buffer_is_line_terminator (g_utf8_get_char (text)))
    return FALSE;

  INITIALIZE_ITER (insert, buffer, prev, node->len);
  node->len += len;

  return TRUE;
}

static const gchar *
_find_next_paragraph_break (const gchar *string)
{
  gboolean last_was_break = FALSE;
  const gchar *str = string;
  gunichar ch;

  if (!string || !*string)
    return NULL;

  while (str && *str &&
         (str = g_utf8_next_char (str)) != NULL)
    {
      ch = g_utf8_get_char (str);

      if (_text_buffer_is_line_terminator (ch))
        last_was_break = TRUE;
      else if (last_was_break)
        break;
    }

  return str;
}

static void
mech_text_buffer_finalize (GObject *object)
{
  MechTextBufferPrivate *priv;

  priv = mech_text_buffer_get_instance_private ((MechTextBuffer *) object);
  g_sequence_free (priv->buffer);
  g_ptr_array_unref (priv->strings);

  G_OBJECT_CLASS (mech_text_buffer_parent_class)->finalize (object);
}

static void
_mech_text_buffer_insert_impl (MechTextBuffer *buffer,
                               MechTextIter   *start,
                               MechTextIter   *end,
                               const gchar    *text,
                               gulong          len)
{
  GSequenceIter *insert_pos, *last = NULL;
  MechTextBufferNode *new, *node = NULL;
  MechStoredString *stored = NULL;
  MechTextBufferPrivate *priv;
  gboolean is_first = TRUE;
  const gchar *str, *next;
  MechTextIter insert;

  priv = mech_text_buffer_get_instance_private (buffer);
  insert = *start;

  if (!g_sequence_iter_is_end (insert.iter))
    {
      node = g_sequence_get (insert.iter);
      g_return_if_fail (insert.pos <= node->len);
    }

  stored = _text_buffer_intern_string (buffer, &insert, text,
                                       len, &str);

  /* Find insertion point */
  if (!node || insert.pos == 0)
    {
      insert_pos = insert.iter;
      next = _find_next_paragraph_break (str);

      /* Check whether part/all of the string
       * can be appended to the previous node
       */
      if (_text_buffer_check_node_append (buffer, start, str, next - str))
        {
          is_first = FALSE;
          last = insert_pos;
          str = next;
        }
    }
  else if (insert.pos == node->len)
    {
      insert_pos = g_sequence_iter_next (insert.iter);
    }
  else
    {
      _mech_text_buffer_node_split (buffer, insert.iter,
				    insert.pos, &insert_pos);
    }

  INITIALIZE_ITER (end, buffer, insert_pos, 0);
}

static void
_mech_text_buffer_delete_impl (MechTextBuffer *buffer,
                               MechTextIter   *start,
                               MechTextIter   *end)
{
  GSequenceIter *delete_start, *delete_end;

  _mech_text_buffer_node_split (buffer, end->iter, end->pos, &delete_end);
  _mech_text_buffer_node_split (buffer, start->iter, start->pos, &delete_start);
  _mech_text_buffer_delete_nodes (buffer, delete_start, delete_end);

  /* Both iters point now to the same position */
  INITIALIZE_ITER (start, buffer, delete_end, 0);
  INITIALIZE_ITER (end, buffer, delete_end, 0);
  _text_buffer_check_trailing_paragraph (buffer, end, end);
}

static void
mech_text_buffer_class_init (MechTextBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_text_buffer_finalize;

  klass->insert = _mech_text_buffer_insert_impl;
  klass->delete = _mech_text_buffer_delete_impl;

  signals[INSERT] =
    g_signal_new ("insert",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechTextBufferClass, insert),
                  NULL, NULL,
                  _mech_marshal_VOID__BOXED_BOXED_STRING_ULONG,
                  G_TYPE_NONE, 4,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_STRING, G_TYPE_ULONG);
  signals[DELETE] =
    g_signal_new ("delete",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechTextBufferClass, delete),
                  NULL, NULL,
                  _mech_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE, 2,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  MECH_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
mech_text_buffer_init (MechTextBuffer *buffer)
{
  MechTextBufferPrivate *priv;

  priv = mech_text_buffer_get_instance_private (buffer);
  priv->strings =
    g_ptr_array_new_with_free_func ((GDestroyNotify) _mech_stored_string_free);
  priv->buffer = g_sequence_new ((GDestroyNotify) _mech_text_buffer_node_free);
}

MechTextBuffer *
mech_text_buffer_new (void)
{
  return g_object_new (MECH_TYPE_TEXT_BUFFER, NULL);
}

void
mech_text_buffer_set_text (MechTextBuffer *buffer,
                           const gchar    *text,
                           gint            len)
{
  MechTextIter start, end;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (text != NULL);

  mech_text_buffer_get_bounds (buffer, &start, &end);
  mech_text_buffer_delete (buffer, &start, &end);

  mech_text_buffer_get_bounds (buffer, &start, NULL);
  mech_text_buffer_insert (buffer, &start, text, len);
}

gint
mech_text_buffer_iter_compare (const MechTextIter *a,
                               const MechTextIter *b)
{
  if (a->iter == b->iter)
    return a->pos - b->pos;
  else
    return g_sequence_iter_compare (a->iter, b->iter);
}

static void
_mech_text_iter_ensure_order (MechTextBuffer     *buffer,
                              const MechTextIter *a,
                              const MechTextIter *b,
                              MechTextIter       *minor_ret,
                              MechTextIter       *major_ret)
{
  if (!a || !b)
    mech_text_buffer_get_bounds (buffer, minor_ret, major_ret);

  if (a)
    *minor_ret = *a;

  if (b)
    *major_ret = *b;

  if (mech_text_buffer_iter_compare (minor_ret, major_ret) > 0)
    {
      MechTextIter tmp;

      tmp = *minor_ret;
      *minor_ret = *major_ret;
      *major_ret = tmp;
    }
}

static void
_mech_text_seq_iterator_init (MechTextSeqIterator *seq_iter,
                              MechTextBuffer      *buffer,
                              const MechTextIter  *start,
                              const MechTextIter  *end)
{
  _mech_text_iter_ensure_order (buffer, start, end,
                                &seq_iter->start, &seq_iter->end);
  seq_iter->current = seq_iter->start.iter;
  seq_iter->finished = FALSE;
}

static gboolean
_mech_text_seq_iterator_next (MechTextSeqIterator  *seq_iter,
                              GSequenceIter       **current,
                              gchar               **str,
                              gssize               *len)
{
  if (seq_iter->finished || g_sequence_iter_is_end (seq_iter->current))
    return FALSE;

  if (current)
    *current = seq_iter->current;

  if (seq_iter->start.iter == seq_iter->end.iter)
    {
      if (g_sequence_iter_is_end (seq_iter->start.iter))
        return FALSE;

      /* Both iters are on the same sequence node */
      if (str)
        *str = ITER_STRING (&seq_iter->start);
      if (len)
        *len = seq_iter->end.pos - seq_iter->start.pos;
    }
  else
    {
      MechTextBufferNode *node;

      node = g_sequence_get (seq_iter->current);

      if (str)
        {
          if (seq_iter->current == seq_iter->start.iter)
            *str = ITER_STRING (&seq_iter->start);
          else
            *str = NODE_STRING (node);
        }

      if (len)
        {
          if (seq_iter->current == seq_iter->start.iter)
            *len = node->len - seq_iter->start.pos;
          else if (seq_iter->current == seq_iter->end.iter)
            *len = seq_iter->end.pos;
          else
            *len = node->len;
        }
    }

  if (seq_iter->current == seq_iter->end.iter)
    seq_iter->finished = TRUE;

  seq_iter->current = g_sequence_iter_next (seq_iter->current);

  return TRUE;
}

gchar *
mech_text_buffer_get_text (MechTextBuffer *buffer,
                           MechTextIter   *start,
                           MechTextIter   *end)
{
  MechTextSeqIterator iterator;
  GString *retval = NULL;
  gchar *str;
  gssize len;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (!start || IS_VALID_ITER (start, buffer), NULL);
  g_return_val_if_fail (!end || IS_VALID_ITER (end, buffer), NULL);

  retval = g_string_new ("");
  _mech_text_seq_iterator_init (&iterator, buffer, start, end);

  while (_mech_text_seq_iterator_next (&iterator, NULL, &str, &len))
    g_string_append_len (retval, str, len);

  return g_string_free (retval, FALSE);
}

void
mech_text_buffer_get_bounds (MechTextBuffer *buffer,
                             MechTextIter   *start,
                             MechTextIter   *end)
{
  MechTextBufferPrivate *priv;

  g_return_if_fail (MECH_TEXT_BUFFER (buffer));

  priv = mech_text_buffer_get_instance_private (buffer);

  if (start)
    INITIALIZE_ITER (start, buffer,
                     g_sequence_get_begin_iter (priv->buffer), 0);

  if (end)
    INITIALIZE_ITER (end, buffer,
                     g_sequence_get_end_iter (priv->buffer), 0);
}

void
mech_text_buffer_delete (MechTextBuffer *buffer,
                         MechTextIter   *start,
                         MechTextIter   *end)
{
  MechTextIter minor, major;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (!start || IS_VALID_ITER (start, buffer));
  g_return_if_fail (!end || IS_VALID_ITER (end, buffer));

  _mech_text_iter_ensure_order (buffer, start, end, &minor, &major);

  if (g_sequence_iter_is_end (minor.iter) ||
      mech_text_buffer_iter_compare (&minor, &major) == 0)
    return;

  g_signal_emit (buffer, signals[DELETE], 0, &minor, &major);

  if (start)
    *start = minor;

  if (end)
    *end = major;
}

void
mech_text_buffer_insert (MechTextBuffer *buffer,
                         MechTextIter   *iter,
                         const gchar    *text,
                         gssize          len)
{
  MechTextIter start, end;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (IS_VALID_ITER (iter, buffer));
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  if (len == 0)
    return;

  start = end = *iter;
  g_signal_emit (buffer, signals[INSERT], 0, &start, &end, text, (gulong) len);
  *iter = end;
}

gboolean
mech_text_buffer_find_forward (MechTextIter     *iter,
                               MechTextFindFunc  func,
                               gpointer          user_data)
{
  MechTextBufferNode *node;
  gchar *str;
  gsize pos;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (func != NULL, FALSE);

  if (g_sequence_iter_is_end (iter->iter))
    return FALSE;

  node = g_sequence_get (iter->iter);

  while (!g_sequence_iter_is_end (iter->iter))
    {
      str = g_utf8_next_char (NODE_STRING_POS (node, iter->pos));
      iter->pos = str - NODE_STRING (node);

      if (iter->pos >= node->len)
        {
          iter->iter = g_sequence_iter_next (iter->iter);
          iter->pos = pos = 0;

          if (!g_sequence_iter_is_end (iter->iter))
            node = g_sequence_get (iter->iter);
        }

      if (func (iter, g_utf8_get_char (str), user_data))
        return TRUE;
    }

  return FALSE;
}

gboolean
mech_text_buffer_find_backward (MechTextIter     *iter,
                                MechTextFindFunc  func,
                                gpointer          user_data)
{
  MechTextBufferNode *node;
  gchar *str;
  gsize pos;

  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);
  g_return_val_if_fail (func != NULL, FALSE);

  if (g_sequence_iter_is_begin (iter->iter) &&
      (iter->pos == 0 || g_sequence_iter_is_end (iter->iter)))
    return FALSE;

  while (!g_sequence_iter_is_begin (iter->iter) || iter->pos != 0)
    {
      if (iter->pos == 0 || g_sequence_iter_is_end (iter->iter))
        {
          iter->iter = g_sequence_iter_prev (iter->iter);
          node = g_sequence_get (iter->iter);
          pos = node->len;
        }
      else
        {
          node = g_sequence_get (iter->iter);
          pos = iter->pos;
        }

      str = g_utf8_prev_char (NODE_STRING_POS (node, pos));
      iter->pos = str - NODE_STRING (node);

      if (func (iter, g_utf8_get_char (str), user_data))
        return TRUE;
    }

  return FALSE;
}

gboolean
mech_text_buffer_iter_move_bytes (MechTextIter *iter,
                                  gssize        bytes)
{
  MechTextBufferNode *node;
  gssize remaining;

  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);

  remaining = ABS (bytes);

  if (bytes == 0)
    return TRUE;

  if (bytes > 0)
    {
      while (remaining > 0 &&
             !g_sequence_iter_is_end (iter->iter))
        {
          node = g_sequence_get (iter->iter);

          if (node->len > iter->pos + remaining)
            {
              iter->pos += remaining;
              remaining = 0;
            }
          else
            {
              remaining -= node->len - iter->pos;
              iter->iter = g_sequence_iter_next (iter->iter);
              iter->pos = 0;
            }
        }
    }
  else
    {
      while (TRUE)
        {
          if (g_sequence_iter_is_end (iter->iter))
            {
              iter->iter = g_sequence_iter_prev (iter->iter);
              continue;
            }

          node = g_sequence_get (iter->iter);

          if (node->len <= remaining)
            {
              remaining -= node->len;
              iter->pos = 0;

              if (g_sequence_iter_is_begin (iter->iter))
                  break;

              iter->iter = g_sequence_iter_prev (iter->iter);
            }
          else
            {
              iter->pos = node->len - remaining;
              remaining = 0;
              break;
            }
        }
    }

  if (iter->pos != 0 &&
      !g_sequence_iter_is_end (iter->iter))
    {
      const gchar *str;

      node = g_sequence_get (iter->iter);

      /* Ensure iter points to the beginning of a character */
      str = NODE_STRING_POS (node, iter->pos);
      str = g_utf8_next_char (g_utf8_prev_char (str));
      iter->pos = NODE_STRING (node) - str;
    }

  return remaining == 0;
}

typedef struct
{
  guint count;
  guint cur;
} CountCharsData;

static gboolean
_iter_find_nth_char (const MechTextIter *iter,
                     gunichar            ch,
                     gpointer            user_data)
{
  CountCharsData *data = user_data;

  data->cur++;
  return data->cur == data->count;
}

gboolean
mech_text_buffer_iter_next (MechTextIter *iter,
                            guint         count)
{
  CountCharsData data = { count, 0 };

  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);

  if (count == 0)
    return TRUE;

  if (g_sequence_iter_is_end (iter->iter))
    return FALSE;

  return mech_text_buffer_find_forward (iter, _iter_find_nth_char, &data);
}

gboolean
mech_text_buffer_iter_previous (MechTextIter *iter,
                                guint         count)
{
  CountCharsData data = { count, 0 };

  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);

  if (count == 0)
    return TRUE;

  if (g_sequence_iter_is_begin (iter->iter) && iter->pos == 0)
    return FALSE;

  return mech_text_buffer_find_backward (iter, _iter_find_nth_char, &data);
}

gboolean
mech_text_buffer_iter_is_start (MechTextIter *iter)
{
  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);

  return g_sequence_iter_is_begin (iter->iter) && iter->pos == 0;
}

gboolean
mech_text_buffer_iter_is_end (MechTextIter *iter)
{
  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);

  return g_sequence_iter_is_end (iter->iter);
}

gunichar
mech_text_buffer_iter_get_char (MechTextIter *iter)
{
  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), 0);

  if (mech_text_buffer_iter_is_end (iter))
    return 0;

  return g_utf8_get_char (ITER_STRING (iter));
}

gssize
mech_text_buffer_get_char_count (MechTextBuffer     *buffer,
                                 const MechTextIter *start,
                                 const MechTextIter *end)
{
  MechTextSeqIterator iterator;
  gssize len, count = 0;
  gchar *str;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), 0);
  g_return_val_if_fail (!start || IS_VALID_ITER (start, buffer), 0);
  g_return_val_if_fail (!end || IS_VALID_ITER (end, buffer), 0);

  _mech_text_seq_iterator_init (&iterator, buffer, start, end);

  while (_mech_text_seq_iterator_next (&iterator, NULL, &str, &len))
    count += g_utf8_strlen (str, len);

  return count;
}

gssize
mech_text_buffer_get_byte_offset (MechTextBuffer     *buffer,
                                  const MechTextIter *start,
                                  const MechTextIter *end)
{
  MechTextSeqIterator iterator;
  gssize len, count = 0;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), 0);
  g_return_val_if_fail (!start || IS_VALID_ITER (start, buffer), 0);
  g_return_val_if_fail (!end || IS_VALID_ITER (end, buffer), 0);

  _mech_text_seq_iterator_init (&iterator, buffer, start, end);

  while (_mech_text_seq_iterator_next (&iterator, NULL, NULL, &len))
    count += len;

  if (start && end && mech_text_buffer_iter_compare (start, end) > 0)
    count *= -1;

  return count;
}
