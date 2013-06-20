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
typedef struct _MechTextRegisteredData MechTextRegisteredData;
typedef struct _MechTextNodeData MechTextNodeData;
typedef struct _MechStoredString MechStoredString;
typedef struct _MechTextBufferMark MechTextBufferMark;
typedef struct _MechTextSeqIterator MechTextSeqIterator;
typedef struct _MechTextUserData MechTextUserData;

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

  GArray *data;
};

struct _MechTextNodeData
{
  guint id;
  MechTextUserData *data;
};

struct _MechTextRegisteredData
{
  gpointer instance;
  GType type;
  guint id;
};

struct _MechTextBufferMark
{
  guint id;
  MechTextIter iter;
};

struct _MechTextSeqIterator
{
  MechTextIter start;
  MechTextIter end;
  GSequenceIter *current;
  guint finished : 1;
};

struct _MechTextUserData
{
  GValue value;
  gint ref_count;
};

struct _MechTextBufferPrivate
{
  GPtrArray *strings;
  GSequence *buffer;
  GArray *registered_data;
  GArray *marks;

  guint last_registered_id;
  guint paragraph_break_id;

  guint paragraph;
  guint mark;
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

static MechTextRegisteredData *
_mech_text_buffer_lookup_registered_data (MechTextBuffer *buffer,
                                          guint           id)
{
  MechTextBufferPrivate *priv;
  guint i;

  priv = mech_text_buffer_get_instance_private (buffer);

  for (i = 0; i < priv->registered_data->len; i++)
    {
      MechTextRegisteredData *data;

      data = &g_array_index (priv->registered_data, MechTextRegisteredData, i);

      if (data->id == id)
        return data;
    }

  return NULL;
}

static MechTextUserData *
_mech_text_user_data_new (GValue *value)
{
  MechTextUserData *data;

  data = g_slice_new0 (MechTextUserData);
  g_value_init (&data->value, G_VALUE_TYPE (value));
  g_value_copy (value, &data->value);
  data->ref_count = 1;

  return data;
}

static MechTextUserData *
_mech_text_user_data_ref (MechTextUserData *data)
{
  g_atomic_int_inc (&data->ref_count);
  return data;
}

static void
_mech_text_user_data_unref (MechTextUserData *data)
{
  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      g_value_unset (&data->value);
      g_slice_free (MechTextUserData, data);
    }
}

static guint
_mech_text_mark_get_insert_position (MechTextBuffer *buffer,
                                     GSequenceIter  *iter,
                                     gsize           pos)
{
  MechTextBufferPrivate *priv;
  MechTextBufferMark *mark;
  gint min, max, mid;

  priv = mech_text_buffer_get_instance_private (buffer);
  min = 0;
  max = priv->marks->len - 1;
  mid = (min + max) / 2;

  while (max >= min)
    {
      mid = (min + max) / 2;
      mark = &g_array_index (priv->marks, MechTextBufferMark, mid);

      if (iter == mark->iter.iter)
        {
          if (pos == mark->iter.pos)
            return mid;
          else if (pos > mark->iter.pos)
            min = mid + 1;
          else
            max = mid - 1;
        }
      else if (g_sequence_iter_compare (iter, mark->iter.iter) > 0)
        min = mid + 1;
      else
        max = mid - 1;
    }

  return MAX (min, max);
}

static MechTextBufferMark *
_mech_text_mark_find (MechTextBuffer *buffer,
                      guint           mark_id,
                      guint          *pos)
{
  MechTextBufferPrivate *priv;
  MechTextBufferMark *mark;
  guint i;

  priv = mech_text_buffer_get_instance_private (buffer);

  for (i = 0; i < priv->marks->len; i++)
    {
      mark = &g_array_index (priv->marks, MechTextBufferMark, i);

      if (mark->id == mark_id)
        {
          if (pos)
            *pos = i;

          return mark;
        }
    }

  return NULL;
}

static void
_mech_text_buffer_split_marks (MechTextBuffer *buffer,
                               GSequenceIter  *iter,
                               gssize          pos)
{
  MechTextBufferPrivate *priv;
  MechTextBufferMark *mark;
  GSequenceIter *next;
  gint mark_pos;

  if (g_sequence_iter_is_end (iter))
    return;

  /* Assume the node has been already split, so the next iter
   * already points to the second half of the previous data.
   */
  priv = mech_text_buffer_get_instance_private (buffer);
  next = g_sequence_iter_next (iter);
  mark_pos = _mech_text_mark_get_insert_position (buffer, iter, pos);

  while (pos < priv->marks->len)
    {
      mark = &g_array_index (priv->marks,
                             MechTextBufferMark, mark_pos);

      if (mark->iter.iter != iter)
        break;

      INITIALIZE_ITER (&mark->iter, buffer, next, 0);
      mark_pos++;
    }
}

static void
_mech_text_buffer_collapse_marks (MechTextBuffer *buffer,
                                  GSequenceIter  *start,
                                  GSequenceIter  *end)
{
  MechTextBufferPrivate *priv;
  MechTextBufferMark *mark;
  gint mark_pos;

  if (g_sequence_iter_is_end (start))
    return;

  priv = mech_text_buffer_get_instance_private (buffer);
  mark_pos = _mech_text_mark_get_insert_position (buffer, start, 0);

  while (mark_pos < priv->marks->len)
    {
      mark = &g_array_index (priv->marks,
                             MechTextBufferMark, mark_pos);

      if (g_sequence_iter_compare (mark->iter.iter, end) >= 0)
        break;

      INITIALIZE_ITER (&mark->iter, buffer, end, 0);
      mark_pos++;
    }
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
                            gint              len,
                            GArray           *data)
{
  MechTextBufferNode *node;
  guint i;

  node = g_slice_new0 (MechTextBufferNode);
  node->stored = stored;
  node->pos = pos;
  node->len = len;

  if (data)
    {
      node->data = g_array_new (FALSE, FALSE, sizeof (MechTextNodeData));
      g_array_insert_vals (node->data, 0, data->data, data->len);

      for (i = 0; i < node->data->len; i++)
        {
          MechTextNodeData *node_data;
          node_data = &g_array_index (node->data, MechTextNodeData, i);
          _mech_text_user_data_ref (node_data->data);
        }
    }

  g_atomic_int_inc (&stored->node_count);

  return node;
}

static void
_mech_text_buffer_node_set_data (MechTextBufferNode *node,
                                 MechTextBuffer     *buffer,
                                 guint               id,
                                 MechTextUserData   *data)
{
  MechTextNodeData new = { 0 };
  guint i;

  if (!node->data && data)
    node->data = g_array_new (FALSE, FALSE, sizeof (MechTextNodeData));

  /* FIXME: binary insertion should help here */
  for (i = 0; i < node->data->len; i++)
    {
      MechTextNodeData *node_data;

      node_data = &g_array_index (node->data, MechTextNodeData, i);

      if (node_data->id == id)
        {
          if (node_data->data == data)
            return;

          if (node_data->data)
            {
              _mech_text_user_data_unref (node_data->data);
              node_data->data = NULL;
            }

          if (data)
            node_data->data = _mech_text_user_data_ref (data);
          else
            g_array_remove_index_fast (node->data, i);
          return;
        }
      else if (data && node_data->id > id)
        break;
    }

  if (data)
    {
      new.id = id;
      new.data = _mech_text_user_data_ref (data);

      if (i < node->data->len)
        g_array_insert_val (node->data, i, new);
      else
        g_array_append_val (node->data, new);
    }
}

static MechTextUserData *
_mech_text_buffer_node_get_data (MechTextBufferNode *node,
                                 guint               id)
{
  guint i;

  if (!node->data)
    return NULL;

  for (i = 0; i < node->data->len; i++)
    {
      MechTextNodeData *node_data;

      node_data = &g_array_index (node->data, MechTextNodeData, i);

      if (node_data->id == id)
        return node_data->data;
    }

  return NULL;
}

static void
_mech_text_buffer_node_free (MechTextBufferNode *node)
{
  MechStoredString *stored;
  guint i;

  stored = node->stored;

  if (node->data)
    {
      for (i = 0; i < node->data->len; i++)
        {
          MechTextNodeData *node_data;

          node_data = &g_array_index (node->data, MechTextNodeData, i);
          _mech_text_user_data_unref (node_data->data);
        }

      g_array_unref (node->data);
    }

  g_slice_free (MechTextBufferNode, node);

  if (g_atomic_int_dec_and_test (&stored->node_count))
    {
      g_string_free (stored->string, TRUE);
      stored->string = NULL;
    }
}

static GArray *
_mech_text_buffer_node_intersect_data (GSequenceIter *minor,
                                       GSequenceIter *major)
{
  MechTextBufferNode *minor_node, *major_node;
  GArray *data;
  guint i, j;

  if (g_sequence_iter_is_begin (major) ||
      g_sequence_iter_is_end (minor))
    return NULL;

  minor_node = g_sequence_get (minor);
  major_node = g_sequence_get (major);
  data = g_array_new (FALSE, FALSE, sizeof (MechTextNodeData));
  i = j = 0;

  while (i < minor_node->data->len &&
         j < major_node->data->len)
    {
      MechTextNodeData *minor_data, *major_data;

      minor_data = &g_array_index (minor_node->data, MechTextNodeData, i);
      major_data = &g_array_index (major_node->data, MechTextNodeData, j);

      if (minor_data->id == major_data->id)
        {
          if (minor_data->data == major_data->data)
            g_array_append_val (data, *minor_data);
          i++;
          j++;
        }
      else if (minor_data->id < major_data->id)
        i++;
      else
        j++;
    }

  return data;
}

static gboolean
_mech_text_buffer_node_data_equals (MechTextBufferNode *node1,
                                    MechTextBufferNode *node2)
{
  if ((node1->data != NULL) != (node2->data != NULL))
    return FALSE;

  if (node1->data->len != node2->data->len)
    return FALSE;

  return memcmp (node1->data->data, node2->data->data,
                 g_array_get_element_size (node1->data) * node1->data->len) == 0;
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
                                    node->len - split_pos,
                                    node->data);
  end_iter = g_sequence_insert_before (next, new);

  /* Trim text after cur_start in original node */
  node->len = split_pos;

  if (end)
    *end = end_iter;

  /* Split marks on this node, if any */
  _mech_text_buffer_split_marks (buffer, iter, split_pos);

  return TRUE;
}

static void
_mech_text_buffer_delete_nodes (MechTextBuffer *buffer,
                                GSequenceIter  *start,
                                GSequenceIter  *end)
{
  _mech_text_buffer_collapse_marks (buffer, start, end);
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

static guint
_mech_text_buffer_node_get_paragraph (MechTextBuffer     *buffer,
                                      MechTextBufferNode *node)
{
  MechTextBufferPrivate *priv;
  MechTextUserData *data;

  priv = mech_text_buffer_get_instance_private (buffer);
  data = _mech_text_buffer_node_get_data (node, priv->paragraph_break_id);

  return g_value_get_uint (&data->value);
}

static MechTextUserData *
_text_buffer_find_paragraph (MechTextBuffer *buffer,
                             MechTextIter   *iter)
{
  MechTextBufferPrivate *priv;
  MechTextIter prev;
  gunichar ch;

  if (!iter)
    return NULL;

  priv = mech_text_buffer_get_instance_private (buffer);
  prev = *iter;

  if (!mech_text_buffer_iter_previous (&prev, 1))
    return NULL;

  ch = mech_text_buffer_iter_get_char (&prev);

  if (_text_buffer_is_line_terminator (ch))
    return NULL;

  return _mech_text_buffer_node_get_data (g_sequence_get (prev.iter),
                                          priv->paragraph_break_id);
}

static MechTextUserData *
_text_buffer_next_paragraph (MechTextBuffer *buffer)
{
  MechTextBufferPrivate *priv;
  MechTextUserData *paragraph;
  GValue value = { 0 };

  priv = mech_text_buffer_get_instance_private (buffer);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, ++priv->paragraph);
  paragraph = _mech_text_user_data_new (&value);
  g_value_unset (&value);

  return paragraph;
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

gboolean
_mech_text_buffer_store_user_data (MechTextBuffer   *buffer,
                                   MechTextIter     *start,
                                   MechTextIter     *end,
                                   guint             id,
                                   MechTextUserData *user_data)
{
  GSequenceIter *iter, *update_start, *update_end;

  if (g_sequence_iter_is_end (start->iter) ||
      mech_text_buffer_iter_compare (start, end) == 0)
    return FALSE;

  _mech_text_buffer_node_split (buffer, end->iter, end->pos, &update_end);
  _mech_text_buffer_node_split (buffer, start->iter, start->pos, &update_start);
  iter = update_start;

  while (iter != update_end)
    {
      MechTextBufferNode *node;

      g_assert (!g_sequence_iter_is_end (iter));

      node = g_sequence_get (iter);
      _mech_text_buffer_node_set_data (node, buffer, id, user_data);
      iter = g_sequence_iter_next (iter);
    }

  INITIALIZE_ITER (start, buffer, update_start, 0);
  INITIALIZE_ITER (end, buffer, update_end, 0);
  return TRUE;
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
_text_buffer_check_trailing_paragraph (MechTextBuffer *buffer,
                                       MechTextIter   *start,
                                       MechTextIter   *end)
{
  MechTextIter prev_start, prev_end, para_end;
  MechTextUserData *paragraph = NULL;
  MechTextBufferPrivate *priv;
  gunichar ch;

  priv = mech_text_buffer_get_instance_private (buffer);
  prev_start = *start;

  if (mech_text_buffer_iter_is_end (end) ||
      !mech_text_buffer_iter_previous (&prev_start, 1))
    return;

  prev_end = *end;
  mech_text_buffer_iter_previous (&prev_end, 1);
  ch = mech_text_buffer_iter_get_char (end);

  if (!_text_buffer_is_line_terminator (ch) &&
      _text_buffer_iter_node_terminates_paragraph (prev_end.iter))
    {
      MechTextUserData *end_para, *prev_start_para;

      end_para = _mech_text_buffer_node_get_data (g_sequence_get (end->iter),
                                                  priv->paragraph_break_id);
      prev_start_para =
        _mech_text_buffer_node_get_data (g_sequence_get (prev_start.iter),
                                         priv->paragraph_break_id);

      if (end_para == prev_start_para)
        {
          /* Insertion happened within a paragraph, so ensure
           * the trailing chunk gets a new paragraph number
           */
          paragraph = _text_buffer_next_paragraph (buffer);
        }
    }

  if (!paragraph)
    {
      MechTextBufferNode *node;

      /* The trailing chunk of the splitted paragraph is now
       * a continuation of the just inserted piece of text
       */
      node = g_sequence_get (prev_end.iter);
      paragraph = _mech_text_buffer_node_get_data (node,
                                                   priv->paragraph_break_id);
    }

  /* Replace paragraph number on all the subsequent paragraph text */
  mech_text_buffer_paragraph_extents (buffer, end, NULL, &para_end);
  _mech_text_buffer_store_user_data (buffer, end, &para_end,
                                     priv->paragraph_break_id, paragraph);
}

void
_mech_text_buffer_check_reunite_nodes (MechTextBuffer *buffer,
                                       MechTextIter   *start,
                                       MechTextIter   *end)
{
  MechTextBufferNode *node, *check;
  GSequenceIter *check_iter;
  guint new_iter_pos;

  g_assert (start->pos == 0);
  g_assert (end->pos == 0);

  /* Check start/end nodes with the outer sequences, if both
   * contiguous nodes point to contiguous points of a same
   * string and have the same data, they can be reunited.
   */

  if (!g_sequence_iter_is_end (start->iter) &&
      !g_sequence_iter_is_begin (start->iter))
    {
      check_iter = g_sequence_iter_prev (start->iter);
      node = g_sequence_get (start->iter);
      check = g_sequence_get (check_iter);

      if (node != check &&
          NODE_STRING_POS (check, check->len + 1) == NODE_STRING (node) &&
          _mech_text_buffer_node_data_equals (node, check))
        {
          gboolean same_position;

          same_position = start->iter == end->iter;
          new_iter_pos = check->len + 1;
          check->len += node->len;
          g_sequence_remove (start->iter);
          INITIALIZE_ITER (start, buffer, check_iter, new_iter_pos);

          /* No need to check the end iter */
          if (same_position)
            return;
        }
    }

  if (!g_sequence_iter_is_end (end->iter) &&
      !g_sequence_iter_is_begin (end->iter))
    {
      check_iter = g_sequence_iter_prev (end->iter);
      node = g_sequence_get (end->iter);
      check = g_sequence_get (check_iter);

      if (node != check &&
          NODE_STRING_POS (check, check->len + 1) == NODE_STRING (node) &&
          _mech_text_buffer_node_data_equals (node, check))
        {
          new_iter_pos = check->len + 1;
          check->len += node->len;
          g_sequence_remove (end->iter);
          INITIALIZE_ITER (end, buffer, check_iter, new_iter_pos);
        }
    }
}

static void
mech_text_buffer_finalize (GObject *object)
{
  MechTextBufferPrivate *priv;

  priv = mech_text_buffer_get_instance_private ((MechTextBuffer *) object);
  mech_text_buffer_unregister_data ((MechTextBuffer *) object,
                                    object, priv->paragraph_break_id);

  g_array_unref (priv->registered_data);
  g_array_unref (priv->marks);
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
  MechTextUserData *paragraph;
  MechTextBufferPrivate *priv;
  GArray *data_array = NULL;
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

      data_array =
        _mech_text_buffer_node_intersect_data (insert_pos,
                                               g_sequence_iter_prev (insert_pos));
    }
  else if (insert.pos == node->len)
    {
      insert_pos = g_sequence_iter_next (insert.iter);
      data_array = _mech_text_buffer_node_intersect_data (insert.iter,
							  insert_pos);
    }
  else
    {
      _mech_text_buffer_node_split (buffer, insert.iter,
				    insert.pos, &insert_pos);
      data_array = g_array_ref (node->data);
    }

  paragraph = _text_buffer_find_paragraph (buffer, &insert);

  while ((next = _find_next_paragraph_break (str)) != NULL)
    {
      if (paragraph)
        _mech_text_user_data_ref (paragraph);
      else
        paragraph = _text_buffer_next_paragraph (buffer);

      new = _mech_text_buffer_node_new (stored, str - stored->string->str,
                                        next - str, data_array);
      last = g_sequence_insert_before (insert_pos, new);

      if (is_first)
        {
          INITIALIZE_ITER (start, buffer, last, 0);
          is_first = FALSE;
        }

      _mech_text_buffer_node_set_data (new, buffer,
                                       priv->paragraph_break_id,
                                       paragraph);

      _mech_text_user_data_unref (paragraph);
      paragraph = NULL;
      str = next;
    }

  INITIALIZE_ITER (end, buffer, insert_pos, 0);

  /* Check paragraph number on trailing text after
   * the inserted text, as it might have changed
   */
  _text_buffer_check_trailing_paragraph (buffer, start, end);

  if (data_array)
    g_array_unref (data_array);
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
  priv->marks = g_array_new (FALSE, FALSE, sizeof (MechTextBufferMark));
  priv->registered_data =
    g_array_new (FALSE, FALSE, sizeof (MechTextRegisteredData));
  priv->paragraph_break_id =
    mech_text_buffer_register_data (buffer, buffer, G_TYPE_UINT);
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

guint
mech_text_buffer_register_data (MechTextBuffer *buffer,
                                gpointer        instance,
                                GType           type)
{
  MechTextBufferPrivate *priv;
  MechTextRegisteredData new;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), 0);
  g_return_val_if_fail (instance != NULL, 0);

  priv = mech_text_buffer_get_instance_private (buffer);
  new.id = ++priv->last_registered_id;
  new.instance = instance;
  new.type = type;

  g_array_append_val (priv->registered_data, new);

  return new.id;
}

static void
_mech_text_buffer_unregister (MechTextBuffer *buffer,
                              gpointer        instance,
                              guint           id)
{
  MechTextBufferPrivate *priv;
  guint i = 0;

  priv = mech_text_buffer_get_instance_private (buffer);

  while (i < priv->registered_data->len)
    {
      MechTextRegisteredData *data;

      data = &g_array_index (priv->registered_data,
                             MechTextRegisteredData, i);

      if (data->instance == instance && (id == 0 || data->id == id))
        g_array_remove_index_fast (priv->registered_data, i);
      else
        i++;
    }
}

void
mech_text_buffer_unregister_data (MechTextBuffer *buffer,
                                  gpointer        instance,
                                  guint           id)
{
  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (instance != NULL);
  g_return_if_fail (id > 0);

  _mech_text_buffer_unregister (buffer, instance, id);
}

void
mech_text_buffer_unregister_instance (MechTextBuffer *buffer,
                                      gpointer        instance)
{
  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (instance != NULL);

  _mech_text_buffer_unregister (buffer, instance, 0);
}

static MechTextBufferNode *
_mech_text_buffer_iter_get_node (const MechTextIter *iter)
{
  GSequenceIter *seq_iter;

  if (g_sequence_iter_is_end (iter->iter))
    {
      if (g_sequence_iter_is_begin (iter->iter))
        return NULL;

      seq_iter = g_sequence_iter_prev (iter->iter);
    }
  else
    seq_iter = iter->iter;

  return g_sequence_get (seq_iter);
}

void
mech_text_buffer_get_datav (MechTextBuffer     *buffer,
                            const MechTextIter *iter,
                            guint               id,
                            GValue             *value)
{
  MechTextRegisteredData *registered;
  MechTextBufferNode *node;
  MechTextUserData *data;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (IS_VALID_ITER (iter, buffer));
  g_return_if_fail (id != 0);

  node = _mech_text_buffer_iter_get_node (iter);
  registered = _mech_text_buffer_lookup_registered_data (buffer, id);
  g_value_init (value, registered->type);

  if (node)
    {
      data = _mech_text_buffer_node_get_data (node, id);
      g_value_copy (&data->value, value);
    }
}

gboolean
mech_text_buffer_set_datav (MechTextBuffer *buffer,
                            MechTextIter   *start,
                            MechTextIter   *end,
                            guint           id,
                            GValue         *value)
{
  MechTextUserData *user_data = NULL;
  MechTextIter minor, major;
  gboolean retval;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (IS_VALID_ITER (start, buffer), FALSE);
  g_return_val_if_fail (IS_VALID_ITER (end, buffer), FALSE);

  if (mech_text_buffer_iter_compare (start, end) == 0)
    return TRUE;

  _mech_text_iter_ensure_order (buffer, start, end, &minor, &major);

  if (value)
    user_data = _mech_text_user_data_new (value);

  retval = _mech_text_buffer_store_user_data (buffer, &minor, &major,
                                              id, user_data);
  if (user_data)
    _mech_text_user_data_unref (user_data);

  _mech_text_buffer_check_reunite_nodes (buffer, &minor, &major);

  if (start)
    *start = minor;

  if (end)
    *end = major;

  return retval;
}

void
mech_text_buffer_get_data (MechTextBuffer     *buffer,
                           const MechTextIter *iter,
                           ...)
{
  MechTextBufferNode *node;
  MechTextUserData *data;
  va_list varargs;
  gchar *error;
  guint id;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (IS_VALID_ITER (iter, buffer));

  va_start (varargs, iter);
  id = va_arg (varargs, gint);
  node = _mech_text_buffer_iter_get_node (iter);

  while (id)
    {
      data = _mech_text_buffer_node_get_data (node, id);

      if (!data)
        {
          MechTextRegisteredData *registered;
          GValue empty = { 0 };

          registered = _mech_text_buffer_lookup_registered_data (buffer, id);

          if (!registered)
            {
              g_warning ("%s: ID %d does not correspond to any registered type",
                         G_STRFUNC, id);
              break;
            }

          g_value_init (&empty, registered->type);
          G_VALUE_LCOPY (&empty, varargs, G_VALUE_NOCOPY_CONTENTS, &error);
        }
      else
        G_VALUE_LCOPY (&data->value, varargs, G_VALUE_NOCOPY_CONTENTS, &error);

      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      id = va_arg (varargs, gint);
    }
}

void
mech_text_buffer_set_data (MechTextBuffer *buffer,
                           MechTextIter   *start,
                           MechTextIter   *end,
                           ...)
{
  MechTextIter minor, major;
  va_list varargs;
  gboolean retval;
  guint id;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (IS_VALID_ITER (start, buffer));
  g_return_if_fail (IS_VALID_ITER (end, buffer));

  if (mech_text_buffer_iter_compare (start, end) == 0)
    return;

  va_start (varargs, end);
  id = va_arg (varargs, gint);
  _mech_text_iter_ensure_order (buffer, start, end, &minor, &major);

  while (id)
    {
      MechTextRegisteredData *registered;
      MechTextUserData *user_data;
      GValue value = { 0 };
      gchar *error;

      registered = _mech_text_buffer_lookup_registered_data (buffer, id);

      if (!registered)
        {
          g_warning ("%s: ID %d does not correspond to any registered type",
                     G_STRFUNC, id);
          break;
        }

      G_VALUE_COLLECT_INIT (&value, registered->type,
                            varargs, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRFUNC, error);
	  g_free (error);
	  break;
	}

      /* FIXME: should we better precompute a user
       * data array and store it in one go? */
      user_data = _mech_text_user_data_new (&value);
      retval = _mech_text_buffer_store_user_data (buffer, &minor, &major,
                                                  id, user_data);
      _mech_text_user_data_unref (user_data);
      g_value_unset (&value);

      if (!retval)
        break;

      id = va_arg (varargs, gint);
    }

  _mech_text_buffer_check_reunite_nodes (buffer, &minor, &major);
  va_end (varargs);

  if (start)
    *start = minor;

  if (end)
    *end = major;
}

void
mech_text_buffer_paragraph_extents (MechTextBuffer     *buffer,
                                    const MechTextIter *iter,
                                    MechTextIter       *paragraph_start,
                                    MechTextIter       *paragraph_end)
{
  MechTextBufferNode *iter_node, *node;
  GSequenceIter *check_iter;
  guint para, check;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (IS_VALID_ITER (iter, buffer));

  if (g_sequence_iter_is_end (iter->iter))
    {
      if (paragraph_start)
        *paragraph_start = *iter;
      if (paragraph_end)
        *paragraph_end = *iter;
      return;
    }

  iter_node = g_sequence_get (iter->iter);
  para = _mech_text_buffer_node_get_paragraph (buffer, iter_node);

  if (paragraph_start)
    {
      check_iter = g_sequence_iter_prev (iter->iter);
      INITIALIZE_ITER (paragraph_start, buffer, iter->iter, 0);

      while (!g_sequence_iter_is_begin (paragraph_start->iter))
        {
          node = g_sequence_get (check_iter);
          check = _mech_text_buffer_node_get_paragraph (buffer, node);

          if (check != para)
            break;

          paragraph_start->iter = check_iter;
          check_iter = g_sequence_iter_prev (check_iter);
        }
    }

  if (paragraph_end)
    {
      INITIALIZE_ITER (paragraph_end, buffer, iter->iter, 0);

      while (!g_sequence_iter_is_end (paragraph_end->iter))
        {
          node = g_sequence_get (paragraph_end->iter);
          check = _mech_text_buffer_node_get_paragraph (buffer, node);

          if (check != para)
            break;

          paragraph_end->iter = g_sequence_iter_next (paragraph_end->iter);
        }
    }
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

gboolean
mech_text_buffer_iter_next_paragraph (MechTextBuffer *buffer,
                                      MechTextIter   *iter)
{
  MechTextBufferPrivate *priv;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (IS_VALID_ITER (iter, buffer), FALSE);

  priv = mech_text_buffer_get_instance_private (buffer);
  return mech_text_buffer_iter_next_section (iter, NULL,
                                             priv->paragraph_break_id);
}

gboolean
mech_text_buffer_iter_previous_paragraph (MechTextBuffer *buffer,
                                          MechTextIter   *iter)
{
  MechTextBufferPrivate *priv;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (IS_VALID_ITER (iter, buffer), FALSE);

  priv = mech_text_buffer_get_instance_private (buffer);
  return mech_text_buffer_iter_previous_section (iter, NULL,
                                                 priv->paragraph_break_id);
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

gboolean
mech_text_buffer_iter_next_section (MechTextIter       *iter,
                                    const MechTextIter *limit,
                                    guint               id)
{
  MechTextUserData *user_data, *check;
  MechTextBufferNode *node;

  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);
  g_return_val_if_fail (!limit || IS_VALID_ITER (limit, iter->buffer), FALSE);

  if ((limit && mech_text_buffer_iter_compare (iter, limit) == 0) ||
      g_sequence_iter_is_end (iter->iter))
    return FALSE;

  node = g_sequence_get (iter->iter);
  check = _mech_text_buffer_node_get_data (node, id);

  while (!g_sequence_iter_is_end (iter->iter))
    {
      node = g_sequence_get (iter->iter);
      user_data = _mech_text_buffer_node_get_data (node, id);

      if (check != user_data)
        return TRUE;

      if (limit && limit->iter == iter->iter)
        {
          *iter = *limit;
          break;
        }

      iter->iter = g_sequence_iter_next (iter->iter);
      iter->pos = 0;
    }

  return FALSE;
}

gboolean
mech_text_buffer_iter_previous_section (MechTextIter       *iter,
                                        const MechTextIter *limit,
                                        guint               id)
{
  MechTextUserData *user_data, *check = NULL;
  MechTextBufferNode *node;

  g_return_val_if_fail (IS_VALID_ITER (iter, NULL), FALSE);
  g_return_val_if_fail (!limit || IS_VALID_ITER (limit, iter->buffer), FALSE);

  if ((limit && mech_text_buffer_iter_compare (iter, limit) == 0) ||
      g_sequence_iter_is_begin (iter->iter))
    return FALSE;

  if (!g_sequence_iter_is_end (iter->iter))
    {
      node = g_sequence_get (iter->iter);
      check = _mech_text_buffer_node_get_data (node, id);
    }

  while (TRUE)
    {
      if (!g_sequence_iter_is_end (iter->iter))
        {
          node = g_sequence_get (iter->iter);
          user_data = _mech_text_buffer_node_get_data (node, id);

          if (check != user_data)
            return TRUE;
        }

      if (limit && limit->iter == iter->iter)
        {
          *iter = *limit;
          break;
        }

      if (g_sequence_iter_is_begin (iter->iter))
        break;

      iter->iter = g_sequence_iter_prev (iter->iter);
      iter->pos = 0;
    }

  return FALSE;
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

guint
mech_text_buffer_create_mark (MechTextBuffer *buffer,
                              MechTextIter   *iter)
{
  MechTextBufferPrivate *priv;
  MechTextBufferMark new;
  gint pos;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), 0);
  g_return_val_if_fail (IS_VALID_ITER (iter, buffer), 0);

  priv = mech_text_buffer_get_instance_private (buffer);
  new.id = ++priv->mark;
  new.iter = *iter;
  pos = _mech_text_mark_get_insert_position (buffer, iter->iter, iter->pos);

  if (pos >= priv->marks->len)
    g_array_append_val (priv->marks, new);
  else
    g_array_insert_val (priv->marks, pos, new);

  return new.id;
}

void
mech_text_buffer_update_mark (MechTextBuffer *buffer,
                              guint           mark_id,
                              MechTextIter   *iter)
{
  MechTextBufferMark *mark, new;
  MechTextBufferPrivate *priv;
  guint del_pos, pos;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (IS_VALID_ITER (iter, buffer));
  g_return_if_fail (mark_id != 0);

  mark = _mech_text_mark_find (buffer, mark_id, &del_pos);

  if (!mark)
    return;

  priv = mech_text_buffer_get_instance_private (buffer);
  pos = _mech_text_mark_get_insert_position (buffer, iter->iter, iter->pos);

  if (pos == del_pos)
    {
      mark->iter = *iter;
      return;
    }

  new.id = mark_id;
  new.iter = *iter;

  if (del_pos > pos)
    del_pos++;

  if (pos >= priv->marks->len)
    g_array_append_val (priv->marks, new);
  else
    g_array_insert_val (priv->marks, pos, new);

  g_array_remove_index (priv->marks, del_pos);
}

void
mech_text_buffer_delete_mark (MechTextBuffer *buffer,
                              guint           mark_id)
{
  MechTextBufferPrivate *priv;
  guint pos;

  g_return_if_fail (MECH_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (mark_id != 0);

  priv = mech_text_buffer_get_instance_private (buffer);

  if (_mech_text_mark_find (buffer, mark_id, &pos))
    g_array_remove_index (priv->marks, pos);
}

gboolean
mech_text_buffer_get_iter_at_mark (MechTextBuffer *buffer,
                                   guint           mark_id,
                                   MechTextIter   *iter)
{
  MechTextBufferMark *mark;

  g_return_val_if_fail (MECH_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (mark_id != 0, FALSE);

  if ((mark = _mech_text_mark_find (buffer, mark_id, NULL)) == NULL)
    return FALSE;

  if (iter)
    *iter = mark->iter;

  return TRUE;
}
