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

#ifndef __MECH_TEXT_BUFFER_H__
#define __MECH_TEXT_BUFFER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_TEXT_ITER           (mech_text_iter_get_type ())

#define MECH_TYPE_TEXT_BUFFER         (mech_text_buffer_get_type ())
#define MECH_TEXT_BUFFER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_TEXT_BUFFER, MechTextBuffer))
#define MECH_TEXT_BUFFER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_TEXT_BUFFER, MechTextBufferClass))
#define MECH_IS_TEXT_BUFFER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_TEXT_BUFFER))
#define MECH_IS_TEXT_BUFFER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_TEXT_BUFFER))
#define MECH_TEXT_BUFFER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_TEXT_BUFFER, MechTextBufferClass))

typedef struct _MechTextBuffer MechTextBuffer;
typedef struct _MechTextBufferClass MechTextBufferClass;
typedef struct _MechTextIter MechTextIter;

typedef gboolean (* MechTextFindFunc) (const MechTextIter *iter,
                                       gunichar            ch,
                                       gpointer            user_data);

struct _MechTextBuffer
{
  GObject parent_instance;
};

struct _MechTextBufferClass
{
  GObjectClass parent_class;

  void (* insert) (MechTextBuffer *buffer,
                   MechTextIter   *start,
                   MechTextIter   *end,
                   const gchar    *string,
		   gulong          len);
  void (* delete) (MechTextBuffer *buffer,
                   MechTextIter   *start,
                   MechTextIter   *end);
};

struct _MechTextIter
{
  MechTextBuffer *buffer;
  GSequenceIter *iter;
  gint pos;
};

GType            mech_text_iter_get_type (void) G_GNUC_CONST;
MechTextIter *   mech_text_iter_copy     (MechTextIter *iter);
void             mech_text_iter_free     (MechTextIter *iter);

GType            mech_text_buffer_get_type (void) G_GNUC_CONST;
MechTextBuffer * mech_text_buffer_new      (void);

/* Buffer manipulation */
void             mech_text_buffer_set_text   (MechTextBuffer *buffer,
                                              const gchar    *text,
                                              gint            len);
gchar          * mech_text_buffer_get_text   (MechTextBuffer *buffer,
                                              MechTextIter   *start,
                                              MechTextIter   *end);

void             mech_text_buffer_get_bounds (MechTextBuffer *buffer,
                                              MechTextIter   *start,
                                              MechTextIter   *end);
void             mech_text_buffer_delete     (MechTextBuffer *buffer,
                                              MechTextIter   *start,
                                              MechTextIter   *end);
void             mech_text_buffer_insert     (MechTextBuffer *buffer,
                                              MechTextIter   *iter,
                                              const gchar    *text,
                                              gssize          len);
gssize           mech_text_buffer_get_char_count (MechTextBuffer     *buffer,
                                                  const MechTextIter *start,
                                                  const MechTextIter *end);
gssize           mech_text_buffer_get_byte_offset   (MechTextBuffer     *buffer,
                                                     const MechTextIter *start,
                                                     const MechTextIter *end);

/* Iterators */
void             mech_text_buffer_paragraph_extents (MechTextBuffer     *buffer,
                                                     const MechTextIter *iter,
                                                     MechTextIter       *paragraph_start,
                                                     MechTextIter       *paragraph_end);

gboolean         mech_text_buffer_find_forward  (MechTextIter     *iter,
                                                 MechTextFindFunc  func,
                                                 gpointer          user_data);
gboolean         mech_text_buffer_find_backward (MechTextIter     *iter,
                                                 MechTextFindFunc  func,
                                                 gpointer          user_data);

gboolean         mech_text_buffer_iter_move_bytes (MechTextIter *iter,
                                                   gssize        bytes);

gboolean         mech_text_buffer_iter_next_paragraph     (MechTextBuffer *buffer,
                                                           MechTextIter   *iter);
gboolean         mech_text_buffer_iter_previous_paragraph (MechTextBuffer *buffer,
                                                           MechTextIter   *iter);

gint             mech_text_buffer_iter_compare  (const MechTextIter *a,
                                                 const MechTextIter *b);

gboolean         mech_text_buffer_iter_next     (MechTextIter     *iter,
                                                 guint             count);
gboolean         mech_text_buffer_iter_previous (MechTextIter     *iter,
                                                 guint             count);

gboolean         mech_text_buffer_iter_is_start (MechTextIter     *iter);
gboolean         mech_text_buffer_iter_is_end   (MechTextIter     *iter);

gunichar         mech_text_buffer_iter_get_char (MechTextIter     *iter);

gboolean         mech_text_buffer_iter_next_section     (MechTextIter       *iter,
                                                         const MechTextIter *limit,
                                                         guint               id);
gboolean         mech_text_buffer_iter_previous_section (MechTextIter       *iter,
                                                         const MechTextIter *limit,
                                                         guint               id);

/* User data */
guint            mech_text_buffer_register_data       (MechTextBuffer *buffer,
                                                       gpointer        instance,
                                                       GType           type);
void             mech_text_buffer_unregister_data     (MechTextBuffer *buffer,
                                                       gpointer        instance,
                                                       guint           id);
void             mech_text_buffer_unregister_instance (MechTextBuffer *buffer,
                                                       gpointer        instance);

void             mech_text_buffer_get_datav           (MechTextBuffer     *buffer,
                                                       const MechTextIter *iter,
                                                       guint               id,
                                                       GValue             *value);
gboolean         mech_text_buffer_set_datav           (MechTextBuffer     *buffer,
                                                       MechTextIter       *start,
                                                       MechTextIter       *end,
                                                       guint               id,
                                                       GValue             *value);
void             mech_text_buffer_get_data            (MechTextBuffer     *buffer,
                                                       const MechTextIter *iter,
                                                       ...);
void             mech_text_buffer_set_data            (MechTextBuffer     *buffer,
                                                       MechTextIter       *start,
                                                       MechTextIter       *end,
                                                       ...);

/* Marks */
void             mech_text_buffer_update_mark         (MechTextBuffer *buffer,
                                                       guint           mark_id,
                                                       MechTextIter   *iter);
guint            mech_text_buffer_create_mark         (MechTextBuffer *buffer,
                                                       MechTextIter   *iter);
void             mech_text_buffer_delete_mark         (MechTextBuffer *buffer,
                                                       guint           mark_id);

gboolean         mech_text_buffer_get_iter_at_mark    (MechTextBuffer *buffer,
                                                       guint           mark,
                                                       MechTextIter   *iter);

G_END_DECLS

#endif /* __MECH_TEXT_BUFFER_H__ */
