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

#include <math.h>
#include <cairo/cairo-gobject.h>
#include <pango/pangocairo.h>

#include <mechane/mech-marshal.h>
#include <mechane/mech-stage-private.h>
#include <mechane/mech-renderer-private.h>
#include <mechane/mech-container-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-enum-types.h>

#define MM_PER_INCH 25.4
#define POINTS_PER_INCH 72.
#define DEFAULT_DPI 96.

#define MATRIX_IS_EQUAL(a,b) \
  ((a).xx == (b).xx &&       \
   (a).yx == (b).yx &&       \
   (a).xy == (b).xy &&       \
   (a).yy == (b).yy &&       \
   (a).x0 == (b).x0 &&       \
   (a).y0 == (b).y0)

#define MATRIX_IS_IDENTITY(m)     \
  ((m).xx == 1 && (m).yx == 0 &&  \
   (m).xy == 0 && (m).yy == 1 &&  \
   (m).x0 == 0 && (m).y0 == 0)

typedef struct _MechAreaPrivate MechAreaPrivate;
typedef struct _MechAreaDelegateData MechAreaDelegateData;
typedef struct _PreferredAxisSize PreferredAxisSize;

enum {
  PROP_0,
  PROP_EVENTS,
  PROP_CLIP,
  PROP_VISIBLE,
  PROP_DEPTH,
  PROP_NAME,
  PROP_MATRIX,
  PROP_CURSOR
};

enum {
  DRAW,
  HANDLE_EVENT,
  VISIBILITY_CHANGED,
  SURFACE_RESET,
  LAST_SIGNAL
};

struct _PreferredAxisSize
{
  gdouble value;
  guint unit   : 3;
  guint is_set : 1;
};

struct _MechAreaPrivate
{
  GNode *node;
  MechArea *parent;
  GPtrArray *children;
  cairo_matrix_t matrix;
  GQuark name;

  MechRenderer *renderer;
  MechCursor *pointer_cursor;

  /* In stage coordinates, counting borders,
   * not affected by matrix.
   */
  cairo_rectangle_t rect;

  /* Index is MechAxis */
  PreferredAxisSize preferred_size[2];

  gdouble width_requested;
  gdouble height_requested;

  gint depth;

  guint evmask              : 16;
  guint state               : 9;
  guint surface_type        : 3;
  guint clip                : 1;
  guint is_identity         : 1;
  guint visible             : 1;
  guint need_width_request  : 1;
  guint need_height_request : 1;
  guint need_allocate_size  : 1;
};

struct _MechAreaDelegateData
{
  guint property_offset;

  /* GType -> delegate object offset in priv struct */
  GHashTable *delegates;
};

static guint signals[LAST_SIGNAL] = { 0 };
static GQuark quark_window = 0;

G_DEFINE_TYPE_WITH_PRIVATE (MechArea, mech_area, G_TYPE_INITIALLY_UNOWNED)

static void
mech_area_init (MechArea *area)
{
  MechAreaPrivate *priv = mech_area_get_instance_private (area);

  priv->children = g_ptr_array_new ();
  priv->is_identity = TRUE;
  cairo_matrix_init_identity (&priv->matrix);

  /* Allocate stage node, even if the area isn't attached to none yet */
  priv->node = _mech_stage_node_new (area);
  mech_area_set_visible (area, TRUE);

  priv->rect.x = priv->rect.y = 0;
  priv->rect.width = priv->rect.height = 0;
  priv->need_width_request = priv->need_height_request = TRUE;
  priv->need_allocate_size = TRUE;
}

MechRenderer *
mech_area_get_renderer (MechArea *area)
{
  MechAreaPrivate *priv = mech_area_get_instance_private (area);

  if (!priv->renderer)
    {
      MechWindow *window;
      MechStyle *style;

      window = mech_area_get_window (area);

      if (!window)
        return NULL;

      style = mech_window_get_style (window);
      priv->renderer = mech_style_lookup_renderer (style, area,
                                                   mech_area_get_state (area));
    }

  return priv->renderer;
}

static gboolean
mech_area_handle_event_impl (MechArea  *area,
                             MechEvent *event)
{
  if (mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  if (area != event->any.target)
    return FALSE;

  switch (event->type)
    {
    case MECH_BUTTON_PRESS:
      mech_area_set_state_flags (area, MECH_STATE_FLAG_PRESSED);
      break;
    case MECH_BUTTON_RELEASE:
      mech_area_unset_state_flags (area, MECH_STATE_FLAG_PRESSED);
      break;
    case MECH_FOCUS_IN:
      mech_area_set_state_flags (area, MECH_STATE_FLAG_FOCUSED);
      break;
    case MECH_FOCUS_OUT:
      mech_area_unset_state_flags (area, MECH_STATE_FLAG_FOCUSED);
      break;
    case MECH_ENTER:
      mech_area_set_state_flags (area, MECH_STATE_FLAG_HOVERED);
      break;
    case MECH_LEAVE:
      mech_area_unset_state_flags (area, MECH_STATE_FLAG_HOVERED);
      break;
    default:
      break;
    }

  return FALSE;
}

static void
mech_area_set_property (GObject      *object,
                        guint         param_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  MechArea *area = (MechArea *) object;

  switch (param_id)
    {
    case PROP_EVENTS:
      mech_area_set_events (area, g_value_get_flags (value));
      break;
    case PROP_CLIP:
      mech_area_set_clip (area, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE:
      mech_area_set_visible (area, g_value_get_boolean (value));
      break;
    case PROP_DEPTH:
      mech_area_set_depth (area, g_value_get_int (value));
      break;
    case PROP_NAME:
      mech_area_set_name (area, g_value_get_string (value));
      break;
    case PROP_MATRIX:
      mech_area_set_matrix (area, g_value_get_boxed (value));
      break;
    case PROP_CURSOR:
      mech_area_set_cursor (area, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
mech_area_get_property (GObject    *object,
                        guint       param_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MechArea *area = (MechArea *) object;
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);

  switch (param_id)
    {
    case PROP_EVENTS:
      g_value_set_flags (value, priv->evmask);
      break;
    case PROP_CLIP:
      g_value_set_boolean (value, priv->clip);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visible);
      break;
    case PROP_DEPTH:
      g_value_set_int (value, priv->depth);
      break;
    case PROP_NAME:
      g_value_set_string (value, g_quark_to_string (priv->name));
      break;
    case PROP_MATRIX:
      g_value_set_boxed (value, &priv->matrix);
      break;
    case PROP_CURSOR:
      g_value_set_object (value, mech_area_get_cursor (area));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
mech_area_dispose (GObject *object)
{
  MechArea *area = (MechArea *) object;
  GNode *node, *children;

  node = _mech_area_get_node (area);
  children = node->children;

  while (children)
    {
      MechArea *child;

      child = children->data;
      children = children->next;
      mech_area_set_parent (child, NULL);
    }

  if (node->parent)
    mech_area_set_parent (area, NULL);

  G_OBJECT_CLASS (mech_area_parent_class)->dispose (object);
}

static void
mech_area_finalize (GObject *object)
{
  MechAreaPrivate *priv = mech_area_get_instance_private ((MechArea *) object);

  if (priv->renderer)
    g_object_unref (priv->renderer);

  g_ptr_array_unref (priv->children);
  _mech_stage_node_free (priv->node);

  G_OBJECT_CLASS (mech_area_parent_class)->finalize (object);
}

static gdouble
mech_area_get_extent_impl (MechArea *area,
                           MechAxis  axis)
{
  gdouble ret = 0;
  GNode *children;

  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      gdouble child_size;

      child_size = mech_area_get_extent (children->data, axis);
      ret = MAX (ret, child_size);
    }

  return ret;
}

static gdouble
mech_area_get_second_extent_impl (MechArea *area,
                                  MechAxis  axis,
                                  gdouble   other_value)
{
  GNode *children;
  gdouble ret = 0;

  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    {
      gdouble child_size;

      child_size = mech_area_get_second_extent (children->data,
                                                axis, other_value);
      ret = MAX (ret, child_size);
    }

  return ret;
}

static void
mech_area_allocate_size_impl (MechArea *area,
                              gdouble   width,
                              gdouble   height)
{
  cairo_rectangle_t rect;
  GNode *children;

  rect.x = rect.y = 0;
  rect.width = width;
  rect.height = height;
  children = _mech_area_get_node (area)->children;

  for (; children; children = children->next)
    mech_area_allocate_size (children->data, &rect);
}

static void
mech_area_add_impl (MechArea *area,
                    MechArea *child)
{
  mech_area_set_parent (child, area);
}

static void
mech_area_remove_impl (MechArea *area,
                       MechArea *child)
{
  GNode *node;

  node = _mech_area_get_node (child);

  if (node->parent && node->parent->data == area)
    mech_area_set_parent (child, NULL);
}

static cairo_region_t *
mech_area_get_shape_impl (MechArea *area)
{
  MechAreaPrivate *priv = mech_area_get_instance_private (area);
  cairo_rectangle_int_t rect = { 0 };

  rect.width = priv->rect.width;
  rect.height = priv->rect.height;

  return cairo_region_create_rectangle (&rect);
}

static void
mech_area_class_init (MechAreaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_area_set_property;
  object_class->get_property = mech_area_get_property;
  object_class->finalize = mech_area_finalize;
  object_class->dispose = mech_area_dispose;

  klass->handle_event = mech_area_handle_event_impl;
  klass->get_extent = mech_area_get_extent_impl;
  klass->get_second_extent = mech_area_get_second_extent_impl;
  klass->allocate_size = mech_area_allocate_size_impl;
  klass->add = mech_area_add_impl;
  klass->remove = mech_area_remove_impl;
  klass->get_shape = mech_area_get_shape_impl;

  g_object_class_install_property (object_class,
                                   PROP_EVENTS,
                                   g_param_spec_flags ("events",
                                                       "Events",
                                                       "Events handled by the area",
                                                       MECH_TYPE_EVENT_MASK, MECH_NONE,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_CLIP,
                                   g_param_spec_boolean ("clip",
                                                         "Clip",
                                                         "Whether the area is clipped to its untrasformed rectangle area",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visible",
                                                         "Visible",
                                                         "Whether the area is visible",
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_DEPTH,
                                   g_param_spec_int ("depth",
                                                     "Depth",
                                                     "Position in the Z-index",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Area name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_MATRIX,
                                   g_param_spec_boxed ("matrix",
                                                       "Matrix",
                                                       "Transformation matrix",
                                                       CAIRO_GOBJECT_TYPE_MATRIX,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_CURSOR,
                                   g_param_spec_object ("cursor",
                                                        "Pointer cursor",
                                                        "Pointer cursor to be displayed on this area",
                                                        MECH_TYPE_CURSOR,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  signals[DRAW] =
    g_signal_new ("draw",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAreaClass, draw),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CAIRO_GOBJECT_TYPE_CONTEXT | G_SIGNAL_TYPE_STATIC_SCOPE);
  signals[HANDLE_EVENT] =
    g_signal_new ("handle-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAreaClass, handle_event),
                  g_signal_accumulator_true_handled, NULL,
                  _mech_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN, 1,
                  MECH_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  signals[VISIBILITY_CHANGED] =
    g_signal_new ("visibility-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAreaClass, visibility_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  signals[SURFACE_RESET] =
    g_signal_new ("surface-reset",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAreaClass, surface_reset),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);

  quark_window = g_quark_from_static_string ("MECH_QUARK_WINDOW");
}

void
mech_area_get_extents (MechArea          *area,
                       MechArea          *relative_to,
                       cairo_rectangle_t *extents)
{
  MechPoint tl, tr, bl, br;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (!relative_to || MECH_IS_AREA (relative_to));
  g_return_if_fail (extents != NULL);

  mech_area_transform_corners (area, relative_to,
                               &tl, &tr, &bl, &br);

  extents->x = floor (MIN (MIN (tl.x, tr.x), MIN (bl.x, br.x)));
  extents->y = floor (MIN (MIN (tl.y, tr.y), MIN (bl.y, br.y)));
  extents->width = ceil (MAX (MAX (tl.x, tr.x), MAX (bl.x, br.x)) - extents->x);
  extents->height = ceil (MAX (MAX (tl.y, tr.y), MAX (bl.y, br.y)) - extents->y);
}

static void
_mech_area_border_axis_extents (MechArea *area,
                                MechAxis  axis,
                                gdouble  *first,
                                gdouble  *second)
{
  MechRenderer *renderer;
  MechBorder border;

  renderer = mech_area_get_renderer (area);
  mech_renderer_get_border_extents (renderer, MECH_EXTENT_CONTENT, &border);

  if (axis == MECH_AXIS_X)
    {
      if (first)
        *first = border.left;
      if (second)
        *second = border.right;
    }
  else if (axis == MECH_AXIS_Y)
    {
      if (first)
        *first = border.left;
      if (second)
        *second = border.right;
    }
}

static guint
_mech_area_box_minimal_size (MechArea *area,
                             MechAxis  axis)
{
  MechRenderer *renderer;
  guint width, height;

  renderer = mech_area_get_renderer (area);
  _mech_renderer_get_minimal_pixel_size (renderer, area, &width, &height);

  return (axis == MECH_AXIS_X) ? width : height;
}

gdouble
mech_area_get_extent (MechArea *area,
                      MechAxis  axis)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);
  g_return_val_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y, 0);

  if (!_mech_area_get_stage (area))
    return 0;

  priv = mech_area_get_instance_private (area);

  if ((axis == MECH_AXIS_X && priv->need_width_request) ||
      (axis == MECH_AXIS_Y && priv->need_height_request))
    {
      gdouble val, preferred;
      gdouble first, second;

      val = MECH_AREA_GET_CLASS (area)->get_extent (area, axis);
      val = MAX (val, _mech_area_box_minimal_size (area, axis));

      _mech_area_border_axis_extents (area, axis, &first, &second);
      val += first + second;

      if (mech_area_get_preferred_size (area, axis, MECH_UNIT_PX, &preferred))
        val = MAX (val, preferred);

      if (axis == MECH_AXIS_X)
        {
          priv->width_requested = val;
          priv->need_width_request = FALSE;
        }
      else if (axis == MECH_AXIS_Y)
        {
          priv->height_requested = val;
          priv->need_height_request = FALSE;
        }
    }

  return (axis == MECH_AXIS_X) ? priv->width_requested : priv->height_requested;
}

gdouble
mech_area_get_second_extent (MechArea *area,
                             MechAxis  axis,
                             gdouble   other_value)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);
  g_return_val_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y, 0);

  if (!_mech_area_get_stage (area))
    return 0;

  priv = mech_area_get_instance_private (area);

  if ((axis == MECH_AXIS_X && priv->need_width_request) ||
      (axis == MECH_AXIS_Y && priv->need_height_request))
    {
      gdouble val, preferred;
      gdouble first, second;
      MechAxis other_axis;

      other_axis = (axis == MECH_AXIS_X) ? MECH_AXIS_Y : MECH_AXIS_X;
      _mech_area_border_axis_extents (area, other_axis, &first, &second);
      other_value -= first + second;

      val = MECH_AREA_GET_CLASS (area)->get_second_extent (area, axis,
                                                           other_value);
      val = MAX (val, _mech_area_box_minimal_size (area, axis));

      _mech_area_border_axis_extents (area, axis, &first, &second);
      val += first + second;

      if (mech_area_get_preferred_size (area, axis, MECH_UNIT_PX, &preferred))
        val = MAX (val, preferred);

      if (axis == MECH_AXIS_X)
        {
          priv->width_requested = val;
          priv->need_width_request = FALSE;
        }
      else if (axis == MECH_AXIS_Y)
        {
          priv->height_requested = val;
          priv->need_height_request = FALSE;
        }
    }

  return (axis == MECH_AXIS_X) ? priv->width_requested : priv->height_requested;
}

static gboolean
_area_update_translation (GNode    *node,
                          gpointer  user_data)
{
  MechPoint *diff = user_data;
  MechArea *area = node->data;
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);
  priv->rect.x += diff->x;
  priv->rect.y += diff->y;

  return FALSE;
}

static void
_mech_area_allocate_stage_rect (MechArea          *area,
                                cairo_rectangle_t *alloc)
{
  MechAreaPrivate *priv = mech_area_get_instance_private (area);

  if (priv->need_allocate_size ||
      priv->rect.width != alloc->width ||
      priv->rect.height != alloc->height)
    {
      MechRenderer *renderer;
      MechBorder border = { 0 };

      renderer = mech_area_get_renderer (area);
      priv->need_allocate_size = FALSE;
      priv->rect = *alloc;

      if (renderer)
        mech_renderer_get_border_extents (renderer, MECH_EXTENT_CONTENT, &border);

      MECH_AREA_GET_CLASS (area)->allocate_size (area,
                                                 alloc->width - (border.left + border.right),
                                                 alloc->height - (border.top + border.bottom));
    }
  else if (priv->rect.x != alloc->x ||
           priv->rect.y != alloc->y)
    {
      MechPoint diff;

      diff.x = alloc->x - priv->rect.x;
      diff.y = alloc->y - priv->rect.y;
      g_node_traverse (priv->node,
                       G_PRE_ORDER, G_TRAVERSE_ALL,
                       -1, _area_update_translation,
                       &diff);
    }
}

void
_mech_area_get_stage_rect (MechArea          *area,
                           cairo_rectangle_t *rect)
{
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);
  *rect = priv->rect;
}

static void
_mech_area_parent_to_stage (MechArea *area,
                            gdouble  *x,
                            gdouble  *y)
{
  MechBorder border = { 0 };
  MechRenderer *renderer;
  cairo_rectangle_t rect;
  GNode *parent;

  parent = _mech_area_get_node (area)->parent;

  if (!parent)
    return;

  _mech_area_get_stage_rect (parent->data, &rect);
  renderer = mech_area_get_renderer (parent->data);
  mech_renderer_get_border_extents (renderer, MECH_EXTENT_CONTENT, &border);

  *x += rect.x + border.left;
  *y += rect.y + border.top;
}

static void
_mech_area_stage_to_parent (MechArea *area,
                            gdouble  *x,
                            gdouble  *y)
{
  MechBorder border = { 0 };
  MechRenderer *renderer;
  cairo_rectangle_t rect;
  GNode *parent;

  parent = _mech_area_get_node (area)->parent;

  if (!parent)
    return;

  _mech_area_get_stage_rect (parent->data, &rect);
  renderer = mech_area_get_renderer (parent->data);

  if (renderer)
    mech_renderer_get_border_extents (renderer, MECH_EXTENT_CONTENT, &border);

  *x -= rect.x + border.left;
  *y -= rect.y + border.top;
}

void
mech_area_allocate_size (MechArea          *area,
                         cairo_rectangle_t *rect)
{
  cairo_rectangle_t alloc = { 0 };
  MechStage *stage;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (rect != NULL);

  stage = _mech_area_get_stage (area);

  if (!stage)
    return;

  alloc = *rect;
  _mech_area_parent_to_stage (area, &alloc.x, &alloc.y);
  _mech_area_allocate_stage_rect (area, &alloc);
}

void
mech_area_get_allocated_size (MechArea          *area,
                              cairo_rectangle_t *size)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (size != NULL);

  priv = mech_area_get_instance_private (area);

  *size = priv->rect;
  _mech_area_stage_to_parent (area, &size->x, &size->y);
}

static PreferredAxisSize *
_mech_area_peek_size (MechArea *area,
                      MechAxis  axis)
{
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);
  axis = CLAMP (axis, MECH_AXIS_X, MECH_AXIS_Y);

  return &priv->preferred_size[axis];
}

void
mech_area_unset_preferred_size (MechArea *area,
                                MechAxis  axis)
{
  PreferredAxisSize *size;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y);

  size = _mech_area_peek_size (area, axis);
  size->is_set = FALSE;
}

void
mech_area_set_preferred_size (MechArea *area,
                              MechAxis  axis,
                              MechUnit  unit,
                              gdouble   value)
{
  PreferredAxisSize *size;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y);
  g_return_if_fail (unit >= MECH_UNIT_PX && unit <= MECH_UNIT_PERCENTAGE);

  size = _mech_area_peek_size (area, axis);
  size->value = value;
  size->unit = unit;
  size->is_set = TRUE;
}

gboolean
mech_area_get_preferred_size (MechArea *area,
                              MechAxis  axis,
                              MechUnit  unit,
                              gdouble  *value)
{
  PreferredAxisSize *size;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);
  g_return_val_if_fail (axis == MECH_AXIS_X || axis == MECH_AXIS_Y, FALSE);
  g_return_val_if_fail (unit >= MECH_UNIT_PX &&
                        unit <= MECH_UNIT_PERCENTAGE, FALSE);

  size = _mech_area_peek_size (area, axis);

  if (value)
    {
      *value = 0;

      if (size->is_set)
        *value = round (mech_area_translate_unit (area, size->value,
                                                  size->unit, unit, axis));
    }

  return size->is_set;
}

void
_mech_area_set_window (MechArea   *area,
                       MechWindow *window)
{
  g_object_set_qdata ((GObject *) area, quark_window, window);
}

MechStage *
_mech_area_get_stage (MechArea *area)
{
  MechWindow *window;

  window = mech_area_get_window (area);

  if (!window)
    return NULL;

  return _mech_container_get_stage ((MechContainer *) window);
}

MechWindow *
mech_area_get_window (MechArea *area)
{
  GNode *node, *root;

  node = _mech_area_get_node (area);

  if (!node)
    return NULL;

  root = g_node_get_root (node);
  return g_object_get_qdata (root->data, quark_window);
}

MechArea *
mech_area_new (const gchar   *name,
               MechEventMask  evmask)
{
  return g_object_new (MECH_TYPE_AREA,
                       "name", name,
                       "events", evmask,
                       NULL);
}

gboolean
mech_area_is_ancestor (MechArea *area,
                       MechArea *ancestor)
{
  GNode *node, *ancestor_node;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);
  g_return_val_if_fail (MECH_IS_AREA (ancestor), FALSE);

  node = _mech_area_get_node (area);
  ancestor_node = _mech_area_get_node (ancestor);

  return g_node_is_ancestor (ancestor_node, node);
}

MechArea *
mech_area_get_parent (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), NULL);

  priv = mech_area_get_instance_private (area);
  return priv->parent;
}

gint
mech_area_get_children (MechArea   *area,
                        MechArea ***children)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);

  priv = mech_area_get_instance_private (area);

  if (children)
    *children = g_memdup (priv->children->pdata,
                          priv->children->len * sizeof (gpointer));

  return priv->children->len;
}

void
mech_area_add (MechArea *area,
               MechArea *child)
{
  MechAreaPrivate *priv, *child_priv;
  MechAreaClass *area_class;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (MECH_IS_AREA (child));

  area_class = MECH_AREA_GET_CLASS (area);

  if (!area_class->add)
    {
      g_warning (G_STRLOC ": %s has no add() implementation",
                 G_OBJECT_CLASS_NAME (area_class));
      return;
    }

  priv = mech_area_get_instance_private (area);
  child_priv = mech_area_get_instance_private (child);

  if (child_priv->node->parent)
    {
      g_warning (G_STRLOC ": Area %s already has a parent (%s)",
                 G_OBJECT_TYPE_NAME (child),
                 G_OBJECT_TYPE_NAME (child_priv->node->parent->data));
      return;
    }

  area_class->add (area, child);

  if (child_priv->node->parent)
    {
      g_ptr_array_add (priv->children, child);
      child_priv->parent = area;
    }
}

void
mech_area_remove (MechArea *area,
                  MechArea *child)
{
  MechAreaClass *area_class;
  MechAreaPrivate *priv;
  guint i;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (MECH_IS_AREA (child));
  g_return_if_fail (mech_area_get_parent (child) == area);

  area_class = MECH_AREA_GET_CLASS (area);

  if (!area_class->remove)
    {
      g_warning (G_STRLOC ": %s has no add() implementation",
                 G_OBJECT_CLASS_NAME (area_class));
      return;
    }

  area_class->remove (area, child);
  priv = mech_area_get_instance_private (area);

  for (i = 0; i < priv->children->len; i++)
    {
      MechArea *check;

      check = g_ptr_array_index (priv->children, i);

      if (check != child)
        continue;

      g_ptr_array_remove_index (priv->children, i);
      break;
    }
}

void
mech_area_set_events (MechArea      *area,
                      MechEventMask  evmask)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));

  /* FIXME: ensure consistent state after setting evmask
   * * Crossing events
   * * breaking grabs
   */

  priv = mech_area_get_instance_private (area);
  priv->evmask = evmask;
}

void
mech_area_add_events (MechArea      *area,
                      MechEventMask  evmask)
{
  g_return_if_fail (MECH_IS_AREA (area));

  evmask |= mech_area_get_events (area);
  mech_area_set_events (area, evmask);
}

MechEventMask
mech_area_get_events (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), MECH_NONE);

  priv = mech_area_get_instance_private (area);
  return priv->evmask;
}

gboolean
mech_area_handles_event (MechArea      *area,
                         MechEventType  event_type)
{
  MechEventMask evmask;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);

  evmask = mech_area_get_events (area);

  switch (event_type)
    {
    case MECH_KEY_PRESS:
    case MECH_KEY_RELEASE:
      return (evmask & MECH_KEY_MASK);
    case MECH_FOCUS_IN:
    case MECH_FOCUS_OUT:
      return (evmask & MECH_FOCUS_MASK);
    case MECH_MOTION:
      return (evmask & MECH_MOTION_MASK);
    case MECH_BUTTON_PRESS:
    case MECH_BUTTON_RELEASE:
      return (evmask & MECH_BUTTON_MASK);
    case MECH_ENTER:
    case MECH_LEAVE:
      return (evmask & MECH_CROSSING_MASK);
    case MECH_SCROLL:
      return (evmask & MECH_SCROLL_MASK);
    case MECH_TOUCH_DOWN:
    case MECH_TOUCH_MOTION:
    case MECH_TOUCH_UP:
      return (evmask & MECH_TOUCH_MASK);
    }

  return FALSE;
}

gboolean
_mech_area_get_renderable_rect (MechArea          *area,
                                cairo_rectangle_t *rect)
{
  MechStage *stage;

  stage = _mech_area_get_stage (area);

  if (!stage)
    return FALSE;

  return _mech_stage_get_renderable_rect (stage, area, rect);
}

cairo_region_t *
_mech_area_get_renderable_region (MechArea *area)
{
  cairo_rectangle_int_t int_rect;
  cairo_rectangle_t rect;

  if (!_mech_area_get_renderable_rect (area, &rect))
    return cairo_region_create ();

  int_rect.x = rect.x;
  int_rect.y = rect.y;
  int_rect.width = rect.width;
  int_rect.height = rect.height;

  return cairo_region_create_rectangle (&int_rect);
}

gboolean
_mech_area_get_visible_rect (MechArea          *area,
                             cairo_rectangle_t *rect)
{
  cairo_rectangle_t size, visible;
  MechPoint tl, tr, bl, br;
  gdouble x1, x2, y1, y2;
  GNode *node;

  node = _mech_area_get_node (area);
  mech_area_get_allocated_size (area, &size);

  if (!node->parent)
    {
      if (rect)
        *rect = size;
      return TRUE;
    }

  while (node->parent && !mech_area_get_clip (node->data))
    node = node->parent;

  mech_area_transform_corners (node->data, area,
                               &tl, &tr, &bl, &br);

  x1 = MIN (MIN (tl.x, tr.x), MIN (bl.x, br.x));
  x2 = MAX (MAX (tl.x, tr.x), MAX (bl.x, br.x));
  y1 = MIN (MIN (tl.y, tr.y), MIN (bl.y, br.y));
  y2 = MAX (MAX (tl.y, tr.y), MAX (bl.y, br.y));

  if (rect)
    {
      rect->x = x1;
      rect->y = y1;
      rect->width = x2 - x1;
      rect->height = y2 - y1;
    }

  if (x2 < 0 || y2 < 0 || x1 > size.width ||
      y1 > size.height || visible.width <= 0 || visible.height <= 0)
    return FALSE;

  return TRUE;
}

void
_mech_area_guess_offscreen_size (MechArea *area,
                                 gint     *width,
                                 gint     *height)
{
  MechPoint tl, tr, bl, br;
  gdouble x1, x2, y1, y2;
  MechAreaPrivate *priv;
  GNode *clipping_node;

  priv = mech_area_get_instance_private (area);
  clipping_node = priv->node->parent;

  while (clipping_node->parent &&
         !mech_area_get_clip (clipping_node->data))
    clipping_node = clipping_node->parent;

  mech_area_transform_corners (clipping_node->data, NULL,
                               &tl, &tr, &bl, &br);

  x1 = MIN (MIN (tl.x, tr.x), MIN (bl.x, br.x));
  x2 = MAX (MAX (tl.x, tr.x), MAX (bl.x, br.x));
  y1 = MIN (MIN (tl.y, tr.y), MIN (bl.y, br.y));
  y2 = MAX (MAX (tl.y, tr.y), MAX (bl.y, br.y));

  if (width)
    *width = x2 - x1;
  if (height)
    *height = y2 - y1;
}

void
mech_area_set_matrix (MechArea             *area,
                      const cairo_matrix_t *matrix)
{
  MechAreaPrivate *priv;
  MechWindow *window;
  MechStage *stage;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (matrix != NULL);

  priv = mech_area_get_instance_private (area);

  if (MATRIX_IS_EQUAL (*matrix, priv->matrix))
    return;

  window = mech_area_get_window (area);

  if (window)
    {
      stage = _mech_container_get_stage ((MechContainer *) window);
      _mech_stage_invalidate (stage, area, NULL, TRUE);
    }

  priv->matrix = *matrix;
  priv->is_identity =
    (MATRIX_IS_IDENTITY (priv->matrix) == TRUE);

  if (stage)
    {
      _mech_stage_invalidate (stage, area, NULL, TRUE);
      mech_container_queue_redraw ((MechContainer *) window);
    }
}

gboolean
mech_area_get_matrix (MechArea       *area,
                      cairo_matrix_t *matrix)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);

  priv = mech_area_get_instance_private (area);

  if (matrix)
    *matrix = priv->matrix;

  return !priv->is_identity;
}

void
mech_area_set_clip (MechArea *area,
                    gboolean  clip)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));

  priv = mech_area_get_instance_private (area);

  if ((clip == TRUE) != (priv->clip == TRUE))
    {
      priv->clip = clip;
      g_object_notify ((GObject *) area, "clip");

      /* FIXME: needs redraw, may also affect pointer state */
    }
}

gboolean
mech_area_get_clip (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);

  priv = mech_area_get_instance_private (area);
  return priv->clip;
}

static MechArea *
find_common_root (MechArea *area1,
                  MechArea *area2)
{
  GNode *node1, *node2;
  gint depth1, depth2;

  node1 = _mech_area_get_node (area1);
  node2 = _mech_area_get_node (area2);
  depth1 = g_node_depth (node1);
  depth2 = g_node_depth (node2);

  if (depth1 == depth2)
    {
      while (node1 != node2)
        {
          node1 = node1->parent;
          node2 = node2->parent;
        }

      return (node1) ? node1->data : NULL;
    }

  if (depth1 > depth2)
    {
      GNode *tmp;

      /* Swap nodes */
      tmp = node2;
      node2 = node1;
      node1 = tmp;
    }

  while (node1 && !g_node_is_ancestor (node1, node2))
    node1 = node1->parent;

  return (node1) ? node1->data : NULL;
}

static void
_mech_area_get_matrix_linear_upwards (MechArea       *area,
                                      MechArea       *ancestor,
                                      cairo_matrix_t *matrix_ret)
{
  cairo_rectangle_t parent_rect, rect;
  MechArea *cur, *parent;
  cairo_matrix_t diff;
  gboolean has_matrix;

  cur = area;
  _mech_area_get_stage_rect (cur, &rect);
  cairo_matrix_init_identity (matrix_ret);

  while (cur != ancestor)
    {
      parent = _mech_area_get_node (cur)->parent->data;
      has_matrix = mech_area_get_matrix (cur, &diff);

      if (parent != ancestor && !has_matrix)
        {
          cur = parent;
          continue;
        }
      else if (has_matrix)
        _mech_area_get_stage_rect (cur, &parent_rect);
      else
        _mech_area_get_stage_rect (parent, &parent_rect);

      cairo_matrix_translate (&diff,
                              rect.x - parent_rect.x,
                              rect.y - parent_rect.y);
      cairo_matrix_multiply (matrix_ret, matrix_ret, &diff);
      rect = parent_rect;
      cur = parent;
    }
}

gboolean
mech_area_get_relative_matrix (MechArea       *area,
                               MechArea       *relative_to,
                               cairo_matrix_t *matrix_ret)
{
  MechArea *common_root;
  GNode *node;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);
  g_return_val_if_fail (!relative_to || MECH_IS_AREA (relative_to), FALSE);
  g_return_val_if_fail (matrix_ret != NULL, FALSE);

  cairo_matrix_init_identity (matrix_ret);

  if (area == relative_to)
    return TRUE;

  node = _mech_area_get_node (area);

  if (!relative_to)
    relative_to = common_root = g_node_get_root (node)->data;
  else
    common_root = find_common_root (area, relative_to);

  g_return_val_if_fail (common_root != NULL, FALSE);

  _mech_area_get_matrix_linear_upwards (area, common_root, matrix_ret);

  if (common_root != relative_to)
    {
      cairo_matrix_t downwards;

      _mech_area_get_matrix_linear_upwards (relative_to, common_root, &downwards);
      cairo_matrix_invert (&downwards);
      cairo_matrix_multiply (matrix_ret, matrix_ret, &downwards);
    }

  return TRUE;
}

void
mech_area_transform_points (MechArea  *area,
                            MechArea  *relative_to,
                            MechPoint *points,
                            gint       n_points)
{
  cairo_matrix_t matrix;
  gint i;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (!relative_to || MECH_IS_AREA (relative_to));

  mech_area_get_relative_matrix (area, relative_to, &matrix);

  for (i = 0; i < n_points; i++)
    cairo_matrix_transform_point (&matrix, &points[i].x, &points[i].y);
}

void
mech_area_transform_point (MechArea *area,
                           MechArea *relative_to,
                           gdouble  *x,
                           gdouble  *y)
{
  MechPoint point;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (!relative_to || MECH_IS_AREA (relative_to));
  g_return_if_fail (x && y);

  point.x = *x;
  point.y = *y;

  mech_area_transform_points (area, relative_to, &point, 1);
  *x = point.x;
  *y = point.y;
}

void
mech_area_transform_corners (MechArea  *area,
                             MechArea  *relative_to,
                             MechPoint *top_left,
                             MechPoint *top_right,
                             MechPoint *bottom_left,
                             MechPoint *bottom_right)
{
  cairo_rectangle_t rect;
  MechPoint points[4];

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (!relative_to || MECH_IS_AREA (relative_to));

  _mech_area_get_stage_rect (area, &rect);
  points[0].x = points[0].y = points[1].y = points[2].x = 0;
  points[1].x = points[3].x = rect.width;
  points[2].y = points[3].y = rect.height;

  mech_area_transform_points (area, relative_to, points, 4);

  if (top_left)
    *top_left = points[0];

  if (top_right)
    *top_right = points[1];

  if (bottom_left)
    *bottom_left = points[2];

  if (bottom_right)
    *bottom_right = points[3];
}

gboolean
_mech_area_handle_event (MechArea  *area,
                         MechEvent *event)
{
  gboolean retval;

  if (!mech_area_handles_event (area, event->type))
    return FALSE;

  g_signal_emit (area, signals[HANDLE_EVENT], 0, event, &retval);

  return retval;
}

void
mech_area_set_state_flags (MechArea       *area,
                           MechStateFlags  state)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));

  priv = mech_area_get_instance_private (area);
  priv->state |= state;

  g_clear_object (&priv->renderer);
  mech_area_redraw (area, NULL);
}

void
mech_area_unset_state_flags (MechArea       *area,
                             MechStateFlags  state)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));

  priv = mech_area_get_instance_private (area);
  priv->state &= ~(state);

  g_clear_object (&priv->renderer);
  mech_area_redraw (area, NULL);
}

MechStateFlags
mech_area_get_state (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), MECH_STATE_FLAG_NONE);

  priv = mech_area_get_instance_private (area);
  return priv->state;
}

static gboolean
_area_node_visibility_change (GNode    *node,
                              gpointer  user_data)
{
  MechArea *area = node->data;

  g_signal_emit (area, signals[VISIBILITY_CHANGED], 0);

  return FALSE;
}

static void
_mech_area_notify_visibility_change (MechArea *area)
{
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);
  g_node_traverse (priv->node,
                   G_PRE_ORDER, G_TRAVERSE_ALL,
                   -1, _area_node_visibility_change,
                   NULL);
}

void
mech_area_set_parent (MechArea *area,
                      MechArea *parent)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (!parent || MECH_IS_AREA (parent));

  priv = mech_area_get_instance_private (area);

  if (priv->parent == parent)
    return;

  g_object_ref (area);

  if (priv->node->parent)
    {
      mech_area_redraw (area, NULL);
      _mech_stage_remove (priv->node);
    }

  priv->parent = parent;

  if (parent)
    {
      _mech_stage_add (_mech_area_get_node (parent), priv->node);
      mech_area_check_size (area);
    }
  else
    {
      priv->rect.x = priv->rect.y = 0;
      priv->rect.width = priv->rect.height = 0;
      priv->need_width_request = priv->need_height_request = TRUE;
      priv->need_allocate_size = TRUE;
    }

  _mech_area_notify_visibility_change (area);

  g_object_unref (area);
}

void
mech_area_set_visible (MechArea *area,
                       gboolean  visible)
{
  MechAreaPrivate *priv;
  gboolean parent_visible;

  g_return_if_fail (MECH_IS_AREA (area));

  priv = mech_area_get_instance_private (area);

  if (priv->visible == visible)
    return;

  if (priv->node->parent)
    parent_visible = mech_area_is_visible (priv->node->parent->data);
  else
    {
      MechWindow *window;

      window = mech_area_get_window (area);
      parent_visible = window && mech_window_get_visible (window);
    }

  priv->visible = visible;
  g_object_notify ((GObject *) area, "visible");

  if (parent_visible)
    {
      _mech_area_notify_visibility_change (area);
      mech_area_redraw (area, NULL);
    }
}

gboolean
mech_area_get_visible (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);

  priv = mech_area_get_instance_private (area);
  return priv->visible;
}

gboolean
mech_area_is_visible (MechArea *area)
{
  MechWindow *window = NULL;
  GNode *node;

  node = _mech_area_get_node (area);

  while (node)
    {
      if (!mech_area_get_visible (node->data))
        return FALSE;

      if (!node->parent)
        window = mech_area_get_window (node->data);

      node = node->parent;
    }

  if (!window)
    return FALSE;

  return mech_window_get_visible (window);
}

GNode *
_mech_area_get_node (MechArea *area)
{
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);
  return priv->node;
}

gint
mech_area_get_depth (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);

  priv = mech_area_get_instance_private (area);
  return priv->depth;
}

void
mech_area_set_depth (MechArea *area,
                     gint      depth)
{
  MechAreaPrivate *priv;
  MechStage *stage;

  g_return_if_fail (MECH_IS_AREA (area));

  priv = mech_area_get_instance_private (area);

  if (priv->depth != depth)
    {
      priv->depth = depth;
      g_object_notify ((GObject *) area, "depth");
    }

  stage = _mech_area_get_stage (area);

  if (stage)
    _mech_stage_notify_depth_change (stage, area);
}

static gboolean
_mech_area_child_resize (MechArea *area,
                         MechArea *child)
{
  MechAreaClass *area_class;

  area_class = MECH_AREA_GET_CLASS (area);

  if (!area_class->child_resize)
    return TRUE;

  return area_class->child_resize (area, child);
}

void
mech_area_check_size (MechArea *area)
{
  MechAreaPrivate *priv, *parent_priv;
  MechArea *parent, *cur;
  cairo_rectangle_t rect;
  gdouble extent;

  g_return_if_fail (MECH_IS_AREA (area));

  cur = area;

  while (_mech_area_get_node (cur)->parent)
    {
      priv = mech_area_get_instance_private (cur);
      parent = priv->node->parent->data;
      parent_priv = mech_area_get_instance_private (parent);

      priv->need_width_request = priv->need_height_request = TRUE;
      priv->need_allocate_size = TRUE;

      if (parent_priv->need_allocate_size)
        return;

      if (!_mech_area_child_resize (parent, cur))
        break;

      cur = parent;
    }

  priv = mech_area_get_instance_private (cur);
  priv->need_allocate_size = TRUE;

  _mech_area_get_stage_rect (cur, &rect);

  /* FIXME: there's better ways to do this for sure */
  extent = mech_area_get_extent (cur, MECH_AXIS_X);
  rect.width = MAX (rect.width, extent);

  extent = mech_area_get_second_extent (cur, MECH_AXIS_Y, extent);
  rect.height = MAX (rect.height, extent);

  _mech_area_allocate_stage_rect (cur, &rect);
}

void
mech_area_redraw (MechArea       *area,
                  cairo_region_t *region)
{
  MechWindow *window;
  MechStage *stage;

  g_return_if_fail (MECH_IS_AREA (area));

  window = mech_area_get_window (area);

  if (!window)
    return;

  stage = _mech_container_get_stage ((MechContainer *) window);
  _mech_stage_invalidate (stage, area, region, FALSE);
  mech_container_queue_redraw ((MechContainer *) window);
}

static gdouble
_unit_to_px (MechArea *area,
             MechAxis  axis,
             MechUnit  unit,
             gdouble   value)
{
  gint width_mm, width;
  MechMonitor *monitor;
  MechWindow *window;
  gdouble pixels;
  GNode *node;

  node = _mech_area_get_node (area);

  switch (unit)
    {
    case MECH_UNIT_PX:
      pixels = value;
      break;
    case MECH_UNIT_EM:
      {
        MechRenderer *renderer;
        PangoContext *context;
        gdouble resolution;

        renderer = mech_area_get_renderer (area);
        context = mech_renderer_get_font_context (renderer);
        resolution = pango_cairo_context_get_resolution (context);

        if (resolution < 0)
          resolution = DEFAULT_DPI;

        pixels = value * (resolution / POINTS_PER_INCH);
        break;
      }
    case MECH_UNIT_IN:
    case MECH_UNIT_MM:
      {
        window = mech_area_get_window (area);
        monitor = mech_window_get_monitor (window);
        mech_monitor_get_mm_extents (monitor, &width_mm, NULL);
        mech_monitor_get_extents (monitor, &width, NULL);
        pixels = (value * width) / width_mm;

        if (unit == MECH_UNIT_IN)
          pixels /= MM_PER_INCH;

        break;
      }
    case MECH_UNIT_PERCENTAGE:
      if (node->parent)
        {
          cairo_rectangle_t size;
          MechArea *check;

          check = node->parent->data;
          mech_area_get_allocated_size (check, &size);

          if (axis == MECH_AXIS_X)
            pixels = value * size.width;
          else
            pixels = value * size.height;

          break;
        }
    default:
      pixels = 0;
      break;
    }

  return pixels;
}

static gdouble
_px_to_unit (MechArea *area,
             MechAxis  axis,
             MechUnit  unit,
             gdouble   pixels)
{
  gdouble value;
  GNode *node;

  node = _mech_area_get_node (area);

  switch (unit)
    {
    case MECH_UNIT_PX:
      value = pixels;
      break;
    case MECH_UNIT_EM:
      {
        MechRenderer *renderer;
        PangoContext *context;
        gdouble resolution;

        renderer = mech_area_get_renderer (area);
        context = mech_renderer_get_font_context (renderer);
        resolution = pango_cairo_context_get_resolution (context);

        if (resolution < 0)
          resolution = DEFAULT_DPI;

        value = pixels * (POINTS_PER_INCH / resolution);
        break;
      }
    case MECH_UNIT_IN:
    case MECH_UNIT_MM:
      {
        gint width_mm, width;
        MechMonitor *monitor;
        MechWindow *window;

        window = mech_area_get_window (area);
        monitor = mech_window_get_monitor (window);
        mech_monitor_get_mm_extents (monitor, &width_mm, NULL);
        mech_monitor_get_extents (monitor, &width, NULL);
        value = (pixels * width_mm) / width;

        if (unit == MECH_UNIT_IN)
          value *= MM_PER_INCH;

        break;
      }
    case MECH_UNIT_PERCENTAGE:
      if (node->parent)
        {
          cairo_rectangle_t size;
          MechArea *check;

          check = node->parent->data;
          mech_area_get_allocated_size (check, &size);

          if (axis == MECH_AXIS_X)
            value = pixels / size.width;
          else
            value = pixels / size.height;

          break;
        }
    default:
      value = 0;
      break;
    }

  return value;
}

gdouble
mech_area_translate_unit (MechArea *area,
                          gdouble   value,
                          MechUnit  from,
                          MechUnit  to,
                          MechAxis  axis)
{
  gdouble pixels;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);
  g_return_val_if_fail (from >= MECH_UNIT_PX && to <= MECH_UNIT_PERCENTAGE, 0);
  g_return_val_if_fail (to >= MECH_UNIT_PX && to <= MECH_UNIT_PERCENTAGE, 0);

  if (value == 0)
    return 0;

  if (from == to)
    return value;

  pixels = _unit_to_px (area, axis, from, value);
  return _px_to_unit (area, axis, to, pixels);
}

void
mech_area_set_name (MechArea *area,
		    const gchar *name)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));

  priv = mech_area_get_instance_private (area);
  priv->name = (name) ? g_quark_from_string (name) : 0;
  g_clear_object (&priv->renderer);
}

const gchar *
mech_area_get_name (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), NULL);

  priv = mech_area_get_instance_private (area);
  return g_quark_to_string (priv->name);
}

GQuark
mech_area_get_qname (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), 0);

  priv = mech_area_get_instance_private (area);
  return priv->name;
}

cairo_region_t *
mech_area_get_shape (MechArea *area)
{
  g_return_val_if_fail (MECH_IS_AREA (area), NULL);

  return MECH_AREA_GET_CLASS (area)->get_shape (area);
}

void
mech_area_grab_focus (MechArea *area,
                      MechSeat *seat)
{
  MechWindow *window;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (MECH_IS_SEAT (seat));

  window = mech_area_get_window (area);
  mech_container_grab_focus ((MechContainer *) window, area, seat);
}

void
mech_area_set_cursor (MechArea   *area,
                      MechCursor *cursor)
{
  MechAreaPrivate *priv;

  g_return_if_fail (MECH_IS_AREA (area));
  g_return_if_fail (!cursor || MECH_IS_CURSOR (cursor));

  priv = mech_area_get_instance_private (area);

  if (cursor)
    g_object_ref (cursor);

  if (priv->pointer_cursor)
    g_object_unref (priv->pointer_cursor);

  priv->pointer_cursor = cursor;
}

MechCursor *
mech_area_get_cursor (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), NULL);

  priv = mech_area_get_instance_private (area);
  return priv->pointer_cursor;
}

void
_mech_area_reset_surface_type (MechArea        *area,
                               MechSurfaceType  surface_type)
{
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);

  if (priv->surface_type == surface_type)
    return;

  priv->surface_type = surface_type;
  g_signal_emit (area, signals[SURFACE_RESET], 0);
}

void
mech_area_set_surface_type (MechArea        *area,
                            MechSurfaceType  surface_type)
{
  MechAreaPrivate *priv;
  MechStage *stage;

  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);
  g_return_val_if_fail (surface_type >= MECH_SURFACE_TYPE_NONE &&
                        surface_type <= MECH_SURFACE_TYPE_GL, FALSE);

  priv = mech_area_get_instance_private (area);

  if (priv->surface_type == surface_type)
    return;

  priv->surface_type = surface_type;
  stage = _mech_area_get_stage (area);

  if (stage)
    {
      _mech_stage_check_update_surface (stage, area);
      _mech_stage_invalidate (stage, area, NULL, FALSE);
    }
}

MechSurfaceType
mech_area_get_surface_type (MechArea *area)
{
  MechAreaPrivate *priv;

  g_return_val_if_fail (MECH_IS_AREA (area), MECH_SURFACE_TYPE_NONE);

  priv = mech_area_get_instance_private (area);
  return priv->surface_type;
}

MechSurfaceType
mech_area_get_effective_surface_type (MechArea *area)
{
  GNode *node;

  g_return_val_if_fail (MECH_IS_AREA (area), MECH_SURFACE_TYPE_NONE);

  node = _mech_area_get_node (area);

  while (node)
    {
      MechSurfaceType type;

      type = mech_area_get_surface_type (node->data);

      if (type != MECH_SURFACE_TYPE_NONE &&
          type != MECH_SURFACE_TYPE_OFFSCREEN)
        return type;

      node = node->parent;
    }

  return MECH_SURFACE_TYPE_NONE;
}

/* Interface delegates */
static MechAreaDelegateData *
_mech_area_delegate_data_new (void)
{
  MechAreaDelegateData *data;

  data = g_new0 (MechAreaDelegateData, 1);
  data->property_offset = G_MAXUINT / 2;
  data->delegates = g_hash_table_new (NULL, NULL);

  return data;
}

void
mech_area_class_set_delegate (MechAreaClass *area_class,
                              GType          iface_type,
                              gssize         delegate_offset)
{
  GParamSpec **iface_properties;
  MechAreaDelegateData *data;
  guint n_properties, i;
  gpointer g_iface;

  g_return_if_fail (MECH_IS_AREA_CLASS (area_class));
  g_return_if_fail (g_type_is_a (iface_type, G_TYPE_INTERFACE));
  g_return_if_fail (g_type_is_a (G_TYPE_FROM_CLASS (area_class), iface_type));

  g_iface = g_type_interface_peek (area_class, iface_type);
  iface_properties = g_object_interface_list_properties (g_iface,
                                                         &n_properties);

  if (area_class->delegate_data)
    data = area_class->delegate_data;
  else
    area_class->delegate_data = data = _mech_area_delegate_data_new ();

  for (i = 0; i < n_properties; i++)
    {
      iface_properties[i] = g_param_spec_override (iface_properties[i]->name,
                                                   iface_properties[i]);
      g_object_class_install_property (G_OBJECT_CLASS (area_class),
				       data->property_offset++,
                                       iface_properties[i]);
    }

  g_hash_table_insert (data->delegates,
                       GSIZE_TO_POINTER (iface_type),
                       GSIZE_TO_POINTER (delegate_offset));
}

static gboolean
_mech_area_update_delegate_property (MechArea   *area,
                                     GParamSpec *pspec,
                                     GValue     *value,
                                     gboolean    set)
{
  MechAreaDelegateData *data;
  MechAreaClass *area_class;
  MechArea *delegate;
  gpointer offset;

  area_class = MECH_AREA_GET_CLASS (area);
  data = area_class->delegate_data;

  if (!data)
    return FALSE;

  if (!g_hash_table_lookup_extended (data->delegates,
                                     GSIZE_TO_POINTER (pspec->owner_type),
                                     NULL, &offset))
    return FALSE;

  delegate = *((MechArea **) G_STRUCT_MEMBER_P (area, (gssize) offset));

  if (!delegate || !g_type_is_a (G_OBJECT_TYPE (delegate), pspec->owner_type))
    {
      g_critical ("Invalid delegate %s (at offset %li in object data) for interface %s\n",
                  G_OBJECT_TYPE_NAME (delegate), (gssize) offset,
                  g_type_name (pspec->owner_type));
      return FALSE;
    }

  if (set)
    g_object_set_property ((GObject *) delegate, pspec->name, value);
  else
    g_object_get_property ((GObject *) delegate, pspec->name, value);

  return TRUE;
}

gboolean
mech_area_get_delegate_property (MechArea   *area,
                                 GParamSpec *pspec,
                                 GValue     *value)
{
  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  return _mech_area_update_delegate_property (area, pspec, value, FALSE);
}

gboolean
mech_area_set_delegate_property (MechArea     *area,
                                 GParamSpec   *pspec,
                                 const GValue *value)
{
  g_return_val_if_fail (MECH_IS_AREA (area), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  return _mech_area_update_delegate_property (area, pspec,
                                              (GValue *) value, TRUE);
}
