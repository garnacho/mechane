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

#include <pango/pangocairo.h>
#include <pango/pango.h>
#include <cairo-gobject.h>
#include <math.h>
#include "mech-text-view.h"
#include "mech-text-range.h"
#include "mech-text.h"
#include "mech-area-private.h"

enum {
  PROP_BUFFER = 1
};

typedef gboolean (* IteratorForeachFunc) (MechTextView *view,
                                          MechTextIter *start,
                                          MechTextIter *end,
                                          gpointer      user_data);

typedef struct _MechTextViewPrivate MechTextViewPrivate;
typedef struct _TextIterator TextIterator;
typedef struct _ParagraphCalcExtentsData ParagraphCalcExtentsData;
typedef struct _FindIterData FindIterData;

struct _TextIterator
{
  MechTextView *view;
  MechTextIter start;
  MechTextIter end;
  MechTextIter current;
  IteratorForeachFunc func;
  gpointer user_data;
  GDestroyNotify destroy_notify;
  guint backwards : 1;
};

struct _FindIterData
{
  MechPoint point;
  MechTextIter iter;
  guint found : 1;
};

struct _ParagraphCalcExtentsData
{
  PangoLayout *calc_layout;
  gdouble y;
  gdouble height;
  gdouble width;

  guint force_update : 1;
  guint backwards : 1;
};

struct _MechTextViewPrivate
{
  MechTextBuffer *buffer;
  guint paragraph_layout_id;
  guint paragraph_extents_id;
  gdouble layout_width;

  gssize buffer_bytes;
  MechTextRange *visible;
  gssize visible_start_offset;
};

static void mech_text_view_text_init   (MechTextInterface *iface);
static void _mech_text_view_set_buffer (MechTextView      *view,
                                        MechTextBuffer    *buffer);

G_DEFINE_TYPE_WITH_CODE (MechTextView, mech_text_view, MECH_TYPE_VIEW,
                         G_ADD_PRIVATE (MechTextView)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_TEXT,
                                                mech_text_view_text_init))

static void
_text_iterator_free (TextIterator *iterator)
{
  if (iterator->user_data && iterator->destroy_notify)
    iterator->destroy_notify (iterator->user_data);

  g_slice_free (TextIterator, iterator);
}

/* Per-paragraph iterator */
static TextIterator *
_paragraph_iterator_new (MechTextView        *view,
                         MechTextIter        *start,
                         MechTextIter        *end,
                         gboolean             backwards,
                         IteratorForeachFunc  func,
                         gpointer             user_data,
                         GDestroyNotify       destroy_notify)
{
  MechTextViewPrivate *priv;
  TextIterator *iterator;

  priv = mech_text_view_get_instance_private (view);
  iterator = g_slice_new0 (TextIterator);
  iterator->func = func;
  iterator->user_data = user_data;
  iterator->destroy_notify = destroy_notify;
  iterator->view = view;
  iterator->backwards = backwards;

  if (!start || !end)
    mech_text_buffer_get_bounds (priv->buffer,
                                 &iterator->start, &iterator->end);
  if (start)
    mech_text_buffer_paragraph_extents (priv->buffer,
                                        start, &iterator->start, NULL);
  if (end)
    {
      if (backwards)
        mech_text_buffer_paragraph_extents (priv->buffer,
                                            end, NULL, &iterator->end);
      else
        iterator->end = *end;
    }

  iterator->current = (backwards) ? iterator->end : iterator->start;

  return iterator;
}

static gboolean
_paragraph_iterator_next_run (TextIterator *iterator)
{
  MechTextIter para_start, para_end;
  MechTextViewPrivate *priv;
  gboolean retval;

  priv = mech_text_view_get_instance_private (iterator->view);

  if (iterator->backwards)
    {
      if (mech_text_buffer_iter_is_start (&iterator->current) ||
          mech_text_buffer_iter_compare (&iterator->current, &iterator->start) <= 0)
        return FALSE;

      mech_text_buffer_iter_previous (&iterator->current, 1);
    }
  else if (mech_text_buffer_iter_is_end (&iterator->current) ||
           mech_text_buffer_iter_compare (&iterator->current, &iterator->end) >= 0)
    return FALSE;

  mech_text_buffer_paragraph_extents (priv->buffer, &iterator->current,
                                      &para_start, &para_end);
  retval = iterator->func (iterator->view, &para_start,
                           &para_end, iterator->user_data);
  iterator->current = (iterator->backwards) ? para_start : para_end;

  return retval;
}

static void
_mech_text_view_paragraph_foreach (MechTextView        *view,
                                   MechTextIter        *start,
                                   MechTextIter        *end,
                                   IteratorForeachFunc  func,
                                   gpointer             user_data)
{
  TextIterator *iterator;

  iterator = _paragraph_iterator_new (view, start, end, FALSE,
                                      func, user_data, NULL);

  while (_paragraph_iterator_next_run (iterator))
    ;

  _text_iterator_free (iterator);
}

/* Iterator to calculate extents */
static gboolean
_text_view_paragraph_calc_extents (MechTextView *view,
                                   MechTextIter *start,
                                   MechTextIter *end,
                                   gpointer      user_data)
{
  ParagraphCalcExtentsData *data = user_data;
  MechTextViewPrivate *priv;
  cairo_rectangle_t rect;
  PangoLayout *layout;
  gint layout_height;

  priv = mech_text_view_get_instance_private (view);
  mech_text_buffer_get_data (priv->buffer, start,
                             priv->paragraph_layout_id, &layout, 0);

  if (!layout)
    {
      if (data->force_update)
        {
          MechRenderer *renderer;
          PangoContext *context;

          renderer = mech_area_get_renderer (MECH_AREA (view));
          context = mech_renderer_get_font_context (renderer);
          layout = pango_layout_new (context);

          mech_text_buffer_set_data (priv->buffer, start, end,
                                     priv->paragraph_layout_id, layout, 0);
          g_object_unref (layout);
        }
      else
        layout = data->calc_layout;
    }

  pango_layout_set_width (layout, data->width * PANGO_SCALE);
  pango_layout_get_pixel_size (layout, NULL, &layout_height);

  rect.x = 0;
  rect.width = data->width;
  rect.height = layout_height;

  if (!data->backwards)
    {
      rect.y = data->y;
      data->y += layout_height;
    }
  else
    {
      data->y -= layout_height;
      rect.y = data->y;
    }

  mech_text_buffer_set_data (priv->buffer, start, end,
                             priv->paragraph_extents_id,
                             &rect, 0);
  data->height += layout_height;

  return TRUE;
}

static void
_paragraph_calc_extents_data_free (gpointer user_data)
{
  ParagraphCalcExtentsData *data = user_data;

  g_object_unref (data->calc_layout);
  g_free (data);
}

static TextIterator *
_calculate_extents_iterator_new (MechTextView *view,
                                 MechTextIter *iter,
                                 gboolean      backwards,
                                 gdouble       initial_y,
                                 gdouble       width,
                                 gboolean      update_layouts)
{
  ParagraphCalcExtentsData *data;
  MechRenderer *renderer;
  PangoContext *context;

  renderer = mech_area_get_renderer (MECH_AREA (view));
  context = mech_renderer_get_font_context (renderer);

  data = g_new0 (ParagraphCalcExtentsData, 1);
  data->calc_layout = pango_layout_new (context);
  data->y = initial_y;
  data->width = width;
  data->height = 0;
  data->force_update = update_layouts;
  data->backwards = backwards;

  return _paragraph_iterator_new (view,
                                  (!backwards) ? iter : NULL,
                                  (backwards) ? iter : NULL,
                                  backwards,
                                  _text_view_paragraph_calc_extents,
                                  data, _paragraph_calc_extents_data_free);
}

static gboolean
_calculate_extents_iterator_run (TextIterator *iterator,
                                 gdouble       height,
                                 gdouble      *height_calculated)
{
  MechTextViewPrivate *priv;
  cairo_rectangle_t *rect;
  gdouble calculated = 0;
  MechTextIter prev;

  priv = mech_text_view_get_instance_private (iterator->view);

  while (_paragraph_iterator_next_run (iterator))
    {
      prev = iterator->current;

      if (!iterator->backwards)
        mech_text_buffer_iter_previous (&prev, 1);

      mech_text_buffer_get_data (priv->buffer, &prev,
                                 priv->paragraph_extents_id, &rect, 0);
      g_assert (rect != NULL);

      calculated += rect->height;

      if (calculated > height)
        break;
    }

  if (height_calculated)
    *height_calculated = calculated;

  return calculated > height ||
    mech_text_buffer_iter_is_end (&iterator->current);
}

static gboolean
_text_view_get_resize_anchor (MechTextView *view,
                              MechTextIter *anchor,
                              gdouble      *dy)
{
  cairo_rectangle_t visible, *rect;
  MechTextIter para_start, iter;
  PangoRectangle anchor_rect;
  MechTextViewPrivate *priv;
  PangoLayout *layout;
  MechPoint point;
  gint index;

  priv = mech_text_view_get_instance_private (view);
  _mech_area_get_visible_rect ((MechArea *) view, &visible);
  point.x = visible.x;
  point.y = visible.y;

  if (!mech_text_view_get_iter_at_point (view, &point, &iter))
    return FALSE;

  mech_text_buffer_paragraph_extents (priv->buffer, &iter, &para_start, NULL);
  mech_text_buffer_get_data (priv->buffer, &iter,
                             priv->paragraph_layout_id, &layout,
                             priv->paragraph_extents_id, &rect,
                             0);
  index = mech_text_buffer_get_byte_offset (priv->buffer, &para_start, &iter);
  pango_layout_index_to_pos (layout, index, &anchor_rect);

  if (anchor)
    *anchor = iter;
  if (dy)
    *dy = anchor_rect.y / PANGO_SCALE;

  return TRUE;
}

static gboolean
_text_view_find_paragraph_at_y (MechTextView *view,
                                gdouble       y,
                                MechTextIter *from,
                                MechTextIter *iter)
{
  MechTextViewPrivate *priv;
  cairo_rectangle_t *rect;

  priv = mech_text_view_get_instance_private (view);

  if (!from)
    mech_text_buffer_get_bounds (priv->buffer, iter, NULL);
  else
    *iter = *from;

  mech_text_buffer_get_data (priv->buffer, iter,
                             priv->paragraph_extents_id, &rect, 0);

  if (!rect)
    return FALSE;

  while (rect->y > y || rect->y + rect->height < y)
    {
      gboolean retval;

      if (rect->y > y)
        retval = mech_text_buffer_iter_previous_paragraph (priv->buffer, iter);
      else if (rect->y + rect->height < y)
        retval = mech_text_buffer_iter_next_paragraph (priv->buffer, iter);

      if (!retval)
        return FALSE;

      mech_text_buffer_get_data (priv->buffer, from,
                                 priv->paragraph_extents_id, &rect, 0);
      if (!rect)
        return FALSE;
    }

  return TRUE;
}

static void
_text_view_set_visible_bounds (MechTextView *view,
                               MechTextIter *start,
                               MechTextIter *end)
{
  MechTextIter old_start, old_end;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);

  if (mech_text_range_get_bounds (priv->visible, &old_start, &old_end))
    priv->visible_start_offset +=
      mech_text_buffer_get_byte_offset (priv->buffer, &old_start, start);
  else
    priv->visible_start_offset =
        mech_text_buffer_get_byte_offset (priv->buffer, NULL, start);

  mech_text_range_set_bounds (priv->visible, start, end);
}

static gboolean
_text_view_update_visible_range (MechTextView   *view,
                                 cairo_region_t *visible)
{
  cairo_rectangle_int_t visible_rect;
  MechTextIter start, end, prev;
  MechTextViewPrivate *priv;
  cairo_rectangle_t *rect;
  TextIterator *iterator;

  priv = mech_text_view_get_instance_private (view);

  if (!mech_text_range_get_bounds (priv->visible, &start, &end))
    return FALSE;

  cairo_region_get_extents (visible, &visible_rect);

  /* Find new first visible paragraph */
  mech_text_buffer_get_data (priv->buffer, &start,
                             priv->paragraph_extents_id,
                             &rect, 0);

  if (rect->y > visible_rect.y)
    {
      iterator = _calculate_extents_iterator_new (view, &start, TRUE,
                                                  rect->y + rect->height,
                                                  priv->layout_width,
                                                  TRUE);
      _calculate_extents_iterator_run (iterator,
                                       rect->y + rect->height -
                                       visible_rect.y,
                                       NULL);
      start = iterator->current;
      _text_iterator_free (iterator);
    }
  else
    _text_view_find_paragraph_at_y (view, visible_rect.y, &start, &start);

  /* Find new last visible paragraph */
  prev = end;
  mech_text_buffer_iter_previous (&prev, 1);
  mech_text_buffer_get_data (priv->buffer, &prev,
                             priv->paragraph_extents_id,
                             &rect, 0);

  if (rect->y + rect->height < visible_rect.y + visible_rect.height)
    {
      iterator = _calculate_extents_iterator_new (view, &end, FALSE,
                                                  rect->y + rect->height,
                                                  priv->layout_width,
                                                  TRUE);
      _calculate_extents_iterator_run (iterator,
                                       visible_rect.y + visible_rect.height -
                                       (rect->y + rect->height),
                                       NULL);
      end = iterator->current;
      _text_iterator_free (iterator);
    }
  else
    {
      mech_text_buffer_iter_previous_paragraph (priv->buffer, &end);
      _text_view_find_paragraph_at_y (view,
                                      visible_rect.y + visible_rect.height,
                                      &end, &end);
      mech_text_buffer_iter_next_paragraph (priv->buffer, &end);
    }

  _text_view_set_visible_bounds (view, &start, &end);

  return TRUE;
}

static gboolean
_text_view_guess_visible_range (MechTextView   *view,
                                cairo_region_t *visible)
{
  cairo_rectangle_int_t visible_rect;
  gdouble min, max, span, start_y;
  gboolean backwards = FALSE;
  MechTextViewPrivate *priv;
  cairo_rectangle_t size;
  TextIterator *iterator;
  MechTextIter iter;
  gssize n_bytes;

  priv = mech_text_view_get_instance_private (view);
  cairo_region_get_extents (visible, &visible_rect);
  span = mech_view_get_boundaries ((MechView *) view, MECH_AXIS_Y, &min, &max);
  mech_area_get_allocated_size (MECH_AREA (view), &size);

  if (span == 0 || visible_rect.width == 0 || visible_rect.height == 0)
    return FALSE;

  /* Roughly estimate the first paragraph out of
   * the current viewport position within the boundaries.
   */
  n_bytes = ((visible_rect.y - min) * priv->buffer_bytes) / span;

  mech_text_buffer_get_bounds (priv->buffer, &iter, NULL);
  mech_text_buffer_iter_move_bytes (&iter, n_bytes);
  mech_text_buffer_paragraph_extents (priv->buffer, &iter, &iter, NULL);

  if (mech_text_buffer_iter_is_start (&iter))
    start_y = min;
  else
    start_y = MAX (visible_rect.y, min);

  iterator = _calculate_extents_iterator_new (view, &iter, backwards,
                                              start_y, size.width, TRUE);
  _calculate_extents_iterator_run (iterator, visible_rect.height, NULL);

  if (backwards)
    {
      mech_text_buffer_iter_next_paragraph (priv->buffer, &iter);
      _text_view_set_visible_bounds (view, &iterator->current, &iter);
    }
  else
    _text_view_set_visible_bounds (view, &iter, &iterator->current);

  _text_iterator_free (iterator);

  return TRUE;
}

static void
_text_view_update_visible_region (MechTextView   *view,
                                  cairo_region_t *visible,
                                  cairo_region_t *old_visible)
{
  MechTextViewPrivate *priv;
  gboolean intersects = FALSE;

  priv = mech_text_view_get_instance_private (view);

  if (!cairo_region_is_empty (old_visible))
    {
      cairo_region_t *intersection;

      intersection = cairo_region_copy (visible);
      cairo_region_intersect (intersection, old_visible);
      intersects = !cairo_region_is_empty (intersection);
      cairo_region_destroy (intersection);
    }

  if (intersects && mech_text_range_get_bounds (priv->visible, NULL, NULL))
    _text_view_update_visible_range (view, visible);
  else
    _text_view_guess_visible_range (view, visible);
}

static void
mech_text_view_set_property (GObject      *object,
                             guint         param_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (param_id)
    {
    case PROP_BUFFER:
      _mech_text_view_set_buffer (MECH_TEXT_VIEW (object),
                                  g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
mech_text_view_get_property (GObject    *object,
                             guint       param_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  MechTextView *view = (MechTextView *) object;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);

  switch (param_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, priv->buffer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
mech_text_view_finalize (GObject *object)
{
  _mech_text_view_set_buffer ((MechTextView *) object, NULL);

  G_OBJECT_CLASS (mech_text_view_parent_class)->finalize (object);
}

static gboolean
_text_view_paragraph_render (MechTextView *view,
                             MechTextIter *start,
                             MechTextIter *end,
                             gpointer      user_data)
{
  MechTextViewPrivate *priv;
  cairo_rectangle_t *rect;
  cairo_t *cr = user_data;
  MechRenderer *renderer;
  PangoLayout *layout;

  priv = mech_text_view_get_instance_private (view);
  renderer = mech_area_get_renderer (MECH_AREA (view));
  mech_text_buffer_get_data (priv->buffer, start,
                             priv->paragraph_extents_id, &rect,
                             priv->paragraph_layout_id, &layout,
                             0);
  if (layout && rect)
    {
      cairo_save (cr);
      cairo_translate (cr, rect->x, rect->y);
      pango_cairo_update_layout (cr, layout);
      mech_renderer_render_layout (renderer, cr, layout);
      cairo_restore (cr);
    }
  else
    g_warning ("%s: Attempting to render an invalid "
               "paragraph (layout: %p, rect: %p)",
               G_STRFUNC, layout, rect);

  return TRUE;
}

static void
mech_text_view_draw (MechArea *area,
                     cairo_t  *cr)
{
  MechTextView *view = (MechTextView *) area;
  MechTextIter visible_start, visible_end;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);

  if (mech_text_range_get_bounds (priv->visible, &visible_start, &visible_end))
    _mech_text_view_paragraph_foreach (view, &visible_start, &visible_end,
                                       _text_view_paragraph_render, cr);
}

static gboolean
_text_view_store_layout (MechTextView *view,
                         MechTextIter *start,
                         MechTextIter *end,
                         gpointer      user_data)
{
  MechTextViewPrivate *priv;
  MechRenderer *renderer;
  PangoContext *context;
  PangoLayout *layout;

  priv = mech_text_view_get_instance_private (view);
  renderer = mech_area_get_renderer (MECH_AREA (view));
  context = mech_renderer_get_font_context (renderer);

  layout = pango_layout_new (context);
  pango_layout_set_width (layout, priv->layout_width * PANGO_SCALE);

  mech_text_buffer_set_data (priv->buffer, start, end,
                             priv->paragraph_layout_id, layout, 0);
  g_object_unref (layout);

  return TRUE;
}

static gboolean
_text_view_get_visible_yrange (MechTextView *view,
                               gdouble      *y,
                               gdouble      *height)
{
  cairo_rectangle_t *rect_start, *rect_end;
  MechTextIter start, end, prev;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);

  if (!mech_text_range_get_bounds (priv->visible, &start, &end))
    return FALSE;

  mech_text_buffer_get_data (priv->buffer, &start,
                             priv->paragraph_extents_id, &rect_start, 0);

  prev = end;
  mech_text_buffer_iter_previous (&prev, 1);
  mech_text_buffer_get_data (priv->buffer, &prev,
                             priv->paragraph_extents_id, &rect_end, 0);

  if (y)
    *y = rect_start->y;

  if (height)
    *height = rect_end->y + rect_end->height - rect_start->y;

  return TRUE;
}

static gdouble
_text_view_guess_height (MechTextView *view,
                         gdouble       width,
                         gssize        n_bytes)
{
  MechTextViewPrivate *priv;
  MechTextIter start, end;
  gssize validated_bytes;
  gdouble height;

  priv = mech_text_view_get_instance_private (view);

  if (mech_text_range_get_bounds (priv->visible, &start, &end))
    {
      _text_view_get_visible_yrange (view, NULL, &height);
      validated_bytes = mech_text_buffer_get_byte_offset (priv->buffer,
                                                          &start, &end);
    }
  else
    {
      TextIterator *iterator;

      /* Validate some text from the buffer start */
      iterator = _calculate_extents_iterator_new (view, NULL, FALSE,
                                                  0, width, FALSE);
      _calculate_extents_iterator_run (iterator, 2000, &height);

      validated_bytes = mech_text_buffer_get_byte_offset (priv->buffer, NULL,
                                                          &iterator->current);
      start = iterator->start;
      end = iterator->current;
      _text_iterator_free (iterator);
    }

  if (mech_text_buffer_iter_is_start (&start) &&
      mech_text_buffer_iter_is_end (&end))
    return height;
  else
    return round (n_bytes * height / validated_bytes);
}

static void
_mech_text_view_recalculate_visible (MechTextView *view,
                                     gdouble       width)
{
  MechTextIter anchor, anchor_para, start, end;
  cairo_rectangle_t *rect, visible;
  PangoRectangle anchor_rect;
  MechTextViewPrivate *priv;
  gdouble dy = 0, initial_y;
  TextIterator *iterator;
  PangoLayout *layout;
  gint index;

  priv = mech_text_view_get_instance_private (view);

  if (!mech_text_range_get_bounds (priv->visible, NULL, NULL))
    return;

  if (!_text_view_get_resize_anchor (view, &anchor, &dy))
    return;

  /* Figure out new Y coord for the paragraph at the top/left
   * corner, so the first currently visible text stays visible
   */
  _mech_area_get_renderable_rect ((MechArea *) view, &visible);
  mech_text_buffer_paragraph_extents (priv->buffer, &anchor,
                                      &anchor_para, NULL);
  index = mech_text_buffer_get_byte_offset (priv->buffer,
                                            &anchor_para, &anchor);

  mech_text_buffer_get_data (priv->buffer, &anchor,
                             priv->paragraph_extents_id, &rect,
                             priv->paragraph_layout_id, &layout,
                             0);
  pango_layout_set_width (layout, width * PANGO_SCALE);

  pango_layout_index_to_pos (layout, index, &anchor_rect);

  initial_y = rect->y + dy - ((anchor_rect.y / PANGO_SCALE));

  /* Find out new visible range end */
  iterator = _calculate_extents_iterator_new (view, &anchor_para, FALSE,
                                              initial_y, width, TRUE);
  _calculate_extents_iterator_run (iterator,
                                   visible.y + visible.height - initial_y,
                                   NULL);
  end = iterator->current;
  _text_iterator_free (iterator);

  if (mech_text_buffer_iter_previous_paragraph (priv->buffer, &anchor_para))
    {
      /* Recalculate extents upwards and find out visible range start */
      iterator = _calculate_extents_iterator_new (view, &anchor_para, TRUE,
                                                  initial_y, width, TRUE);
      _calculate_extents_iterator_run (iterator,initial_y - visible.y, NULL);
      start = iterator->current;
      _text_iterator_free (iterator);
    }
  else
    start = anchor_para;

  _text_view_set_visible_bounds (view, &start, &end);
}

static gdouble
mech_text_view_get_extent (MechArea *area,
                           MechAxis  axis)
{
  MechTextView *view = (MechTextView *) area;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);

  if (!priv->buffer)
    return 0;

  /* FIXME */
  return 100;
}

static gdouble
mech_text_view_get_second_extent (MechArea *area,
                                  MechAxis  axis,
                                  gdouble   other_value)
{
  MechTextView *view = (MechTextView *) area;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);

  if (!priv->buffer)
    return 0;

  return _text_view_guess_height (view, other_value, priv->buffer_bytes);
}

static void
mech_text_view_allocate_size (MechArea *area,
                              gdouble   width,
                              gdouble   height)
{
  MechTextView *view = (MechTextView *) area;
  MechTextViewPrivate *priv;
  gboolean has_visible;

  priv = mech_text_view_get_instance_private (view);

  if (priv->buffer)
    {
      has_visible = mech_text_range_get_bounds (priv->visible, NULL, NULL);

      if (has_visible && width != priv->layout_width)
        _mech_text_view_recalculate_visible (view, width);
    }

  priv->layout_width = width;
  mech_area_redraw (area, NULL);
  MECH_AREA_CLASS (mech_text_view_parent_class)->allocate_size (area,
                                                                width, height);
}

static void
mech_text_view_viewport_moved (MechView       *area,
                               cairo_region_t *visible,
                               cairo_region_t *previous)
{
  MechTextView *view = (MechTextView *) area;

  _text_view_update_visible_region (view, visible, previous);
  mech_area_check_size ((MechArea *) area);
}

static void
mech_text_view_get_boundaries (MechView *view,
                               MechAxis  axis,
                               gdouble  *min,
                               gdouble  *max)
{
  MechTextView *text_view = (MechTextView *) view;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (text_view);

  if (!priv->buffer)
    {
      *min = *max = 0;
      return;
    }

  if (axis == MECH_AXIS_X)
    {
      *min = 0;
      *max = priv->layout_width;
    }
  else if (axis == MECH_AXIS_Y)
    {
      MechTextIter start, end;

      if (!mech_text_range_get_bounds (priv->visible, &start, &end) ||
          (mech_text_buffer_iter_is_start (&start) &&
           mech_text_buffer_iter_is_end (&end)))
        {
          *min = 0;
          *max = _text_view_guess_height (text_view, priv->layout_width,
                                          priv->buffer_bytes);
        }
      else
        {
          gdouble y, height;
          gssize visible_len;

          _text_view_get_visible_yrange (text_view, &y, &height);
          visible_len = mech_text_buffer_get_byte_offset (priv->buffer,
                                                          &start, &end);

          *min = y - _text_view_guess_height (text_view, priv->layout_width,
                                              priv->visible_start_offset);
          *max = y + height +
            _text_view_guess_height (text_view, priv->layout_width,
                                     priv->buffer_bytes -
                                     (priv->visible_start_offset +
                                      visible_len));
        }
    }
}

static void
_text_view_store_layouts (MechTextView *view,
                          MechTextIter *from,
                          MechTextIter *to)
{
  _mech_text_view_paragraph_foreach (view, from, to,
                                     _text_view_store_layout,
                                     NULL);
}

static void
_text_view_unset_data (MechTextView *view,
                       MechTextIter *from,
                       MechTextIter *to)
{
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);
  mech_text_buffer_set_data (priv->buffer, from, to,
                             priv->paragraph_layout_id, NULL,
                             priv->paragraph_extents_id, NULL,
                             0);
}

static void
_text_view_buffer_insert_after (MechTextBuffer *buffer,
                                MechTextIter   *start,
                                MechTextIter   *end,
                                gchar          *text,
                                gulong          len,
                                MechTextView   *view)
{
  MechTextIter visible_start, visible_end;
  MechTextIter para_start, para_end;
  MechTextViewPrivate *priv;

  priv = mech_text_view_get_instance_private (view);
  priv->buffer_bytes += len;

  if (!mech_text_range_get_bounds (priv->visible, &visible_start, &visible_end))
    return;

  mech_text_buffer_paragraph_extents (buffer, start, &para_start, &para_end);

  if (mech_text_buffer_iter_compare (&para_start, start) == 0)
    mech_text_buffer_iter_previous_paragraph (buffer, &para_start);

  if (mech_text_buffer_iter_compare (&para_end, end) >= 0)
    mech_text_buffer_iter_next_paragraph (buffer, &para_end);

  if (mech_text_buffer_iter_compare (start, &visible_start) >= 0 &&
      mech_text_buffer_iter_compare (end, &visible_end) <= 0)
    {
      _text_view_store_layouts (view, &para_start, &para_end);
      _mech_text_view_recalculate_visible (view, priv->layout_width);
      mech_area_redraw ((MechArea *) view, NULL);
    }
  else if (mech_text_buffer_iter_compare (end, &visible_start) <= 0)
    priv->visible_start_offset += len;
}

static void
_text_view_buffer_delete (MechTextBuffer *buffer,
                          MechTextIter   *start,
                          MechTextIter   *end,
                          MechTextView   *view)
{
  MechTextIter visible_start;
  MechTextViewPrivate *priv;
  gssize len;

  priv = mech_text_view_get_instance_private (view);
  len = mech_text_buffer_get_byte_offset (buffer, start, end);
  priv->buffer_bytes -= len;

  if (mech_text_range_get_bounds (priv->visible, &visible_start, NULL) &&
      mech_text_buffer_iter_compare (end, &visible_start) <= 0)
    priv->visible_start_offset -= len;
}

static void
_text_view_buffer_delete_after (MechTextBuffer *buffer,
                                MechTextIter   *unused,
                                MechTextIter   *iter,
                                MechTextView   *view)
{
  MechTextIter visible_start, visible_end;
  MechTextViewPrivate *priv;
  MechTextIter start, end;

  priv = mech_text_view_get_instance_private (view);
  mech_text_buffer_paragraph_extents (buffer, iter, &start, &end);
  mech_text_buffer_iter_previous_paragraph (buffer, &start);
  mech_text_buffer_iter_next_paragraph (buffer, &end);

  if (mech_text_range_get_bounds (priv->visible, &visible_start, &visible_end) &&
      mech_text_buffer_iter_compare (&start, &visible_start) >= 0 &&
      mech_text_buffer_iter_compare (&end, &visible_end) <= 0)
    {
      _text_view_store_layouts (view, &start, &end);
      _mech_text_view_recalculate_visible (view, priv->layout_width);
      mech_area_redraw ((MechArea *) view, NULL);
    }
}

static void
_text_view_clear_visible (MechTextView *view)
{
  MechTextViewPrivate *priv;
  MechTextIter start, end;

  priv = mech_text_view_get_instance_private (view);

  if (!priv->visible)
    return;

  if (mech_text_range_get_bounds (priv->visible, &start, &end))
    mech_text_buffer_set_data (priv->buffer, &start, &end,
                               priv->paragraph_layout_id, NULL,
                               priv->paragraph_extents_id, NULL,
                               0);

  g_clear_object (&priv->visible);
}

static void
_mech_text_view_set_buffer (MechTextView   *view,
                            MechTextBuffer *buffer)
{
  MechTextViewPrivate *priv;

  g_return_if_fail (MECH_IS_TEXT_VIEW (view));
  g_return_if_fail (!buffer || MECH_IS_TEXT_BUFFER (buffer));

  priv = mech_text_view_get_instance_private (view);

  if (priv->buffer)
    {
      _text_view_clear_visible (view);

      g_signal_handlers_disconnect_by_data (priv->buffer, view);
      mech_text_buffer_unregister_instance (priv->buffer, view);
      g_object_unref (priv->buffer);
      priv->paragraph_layout_id = 0;
      priv->paragraph_extents_id = 0;
    }

  priv->buffer = buffer;

  if (priv->buffer)
    {
      MechTextIter start;

      g_object_ref (priv->buffer);
      priv->paragraph_layout_id =
        mech_text_buffer_register_data (priv->buffer, view,
                                        PANGO_TYPE_LAYOUT);
      priv->paragraph_extents_id =
        mech_text_buffer_register_data (priv->buffer, view,
                                        CAIRO_GOBJECT_TYPE_RECTANGLE);

      mech_text_buffer_get_bounds (priv->buffer, &start, NULL);

      priv->visible = mech_text_range_new ();
      g_signal_connect_swapped (priv->visible, "removed",
                                G_CALLBACK (_text_view_unset_data), view);

      g_signal_connect_after (priv->buffer, "insert",
			      G_CALLBACK (_text_view_buffer_insert_after),
                              view);
      g_signal_connect_after (priv->buffer, "delete",
			      G_CALLBACK (_text_view_buffer_delete_after),
			      view);
      g_signal_connect (priv->buffer, "delete",
                        G_CALLBACK (_text_view_buffer_delete), view);

      priv->buffer_bytes = mech_text_buffer_get_byte_offset (priv->buffer,
                                                             NULL, NULL);
    }

  g_object_notify (G_OBJECT (view), "buffer");

  mech_area_check_size (MECH_AREA (view));
}

static void
mech_text_view_text_init (MechTextInterface *iface)
{
}

static void
mech_text_view_class_init (MechTextViewClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  MechViewClass *view_class = MECH_VIEW_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_text_view_set_property;
  object_class->get_property = mech_text_view_get_property;
  object_class->finalize = mech_text_view_finalize;

  area_class->draw = mech_text_view_draw;
  area_class->get_extent = mech_text_view_get_extent;
  area_class->get_second_extent = mech_text_view_get_second_extent;
  area_class->allocate_size = mech_text_view_allocate_size;

  view_class->viewport_moved = mech_text_view_viewport_moved;
  view_class->get_boundaries = mech_text_view_get_boundaries;

  g_object_class_override_property (object_class,
                                    PROP_BUFFER,
                                    "buffer");
}

static void
mech_text_view_init (MechTextView *view)
{
}

MechArea *
mech_text_view_new (void)
{
  return g_object_new (MECH_TYPE_TEXT_VIEW, NULL);
}
