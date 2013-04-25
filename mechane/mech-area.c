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

#include <mechane/mech-marshal.h>
#include <mechane/mech-stage-private.h>
#include <mechane/mech-window-private.h>
#include <mechane/mech-area-private.h>

typedef struct _MechAreaPrivate MechAreaPrivate;
typedef struct _MechAreaDelegateData MechAreaDelegateData;
typedef struct _PreferredAxisSize PreferredAxisSize;

enum {
  PROP_0,
  PROP_VISIBLE,
  PROP_DEPTH
};

enum {
  VISIBILITY_CHANGED,
  LAST_SIGNAL
};

struct _MechAreaPrivate
{
  GNode *node;
  MechArea *parent;
  GPtrArray *children;

  /* In stage coordinates */
  cairo_rectangle_t rect;

  gdouble width_requested;
  gdouble height_requested;

  gint depth;

  guint visible             : 1;
  guint need_width_request  : 1;
  guint need_height_request : 1;
  guint need_allocate_size  : 1;
};

static guint signals[LAST_SIGNAL] = { 0 };
static GQuark quark_window = 0;

G_DEFINE_TYPE_WITH_PRIVATE (MechArea, mech_area, G_TYPE_INITIALLY_UNOWNED)

static void
mech_area_init (MechArea *area)
{
  MechAreaPrivate *priv = mech_area_get_instance_private (area);

  priv->children = g_ptr_array_new ();

  /* Allocate stage node, even if the area isn't attached to none yet */
  priv->node = _mech_stage_node_new (area);
  mech_area_set_visible (area, TRUE);

  priv->rect.x = priv->rect.y = 0;
  priv->rect.width = priv->rect.height = 0;
  priv->need_width_request = priv->need_height_request = TRUE;
  priv->need_allocate_size = TRUE;
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
    case PROP_VISIBLE:
      mech_area_set_visible (area, g_value_get_boolean (value));
      break;
    case PROP_DEPTH:
      mech_area_set_depth (area, g_value_get_int (value));
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
    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visible);
      break;
    case PROP_DEPTH:
      g_value_set_int (value, priv->depth);
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

static void
mech_area_class_init (MechAreaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_area_set_property;
  object_class->get_property = mech_area_get_property;
  object_class->finalize = mech_area_finalize;
  object_class->dispose = mech_area_dispose;

  klass->get_extent = mech_area_get_extent_impl;
  klass->get_second_extent = mech_area_get_second_extent_impl;
  klass->allocate_size = mech_area_allocate_size_impl;
  klass->add = mech_area_add_impl;
  klass->remove = mech_area_remove_impl;

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
  signals[VISIBILITY_CHANGED] =
    g_signal_new ("visibility-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAreaClass, visibility_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  quark_window = g_quark_from_static_string ("MECH_QUARK_WINDOW");
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
      gdouble val;

      val = MECH_AREA_GET_CLASS (area)->get_extent (area, axis);

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
      gdouble val;

      val = MECH_AREA_GET_CLASS (area)->get_second_extent (area, axis,
                                                           other_value);
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
      priv->need_allocate_size = FALSE;
      priv->rect = *alloc;

      MECH_AREA_GET_CLASS (area)->allocate_size (area,
                                                 alloc->width,
                                                 alloc->height);
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
  cairo_rectangle_t rect;
  GNode *parent;

  parent = _mech_area_get_node (area)->parent;

  if (!parent)
    return;

  _mech_area_get_stage_rect (parent->data, &rect);
  *x += rect.x;
  *y += rect.y;
}

static void
_mech_area_stage_to_parent (MechArea *area,
                            gdouble  *x,
                            gdouble  *y)
{
  cairo_rectangle_t rect;
  GNode *parent;

  parent = _mech_area_get_node (area)->parent;

  if (!parent)
    return;

  _mech_area_get_stage_rect (parent->data, &rect);
  *x -= rect.x;
  *y -= rect.y;
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

void
_mech_area_make_window_root (MechArea   *area,
                             MechWindow *window)
{
  g_object_set_qdata ((GObject *) area, quark_window, window);
  _mech_stage_set_root (_mech_window_get_stage (window), area);
}

MechStage *
_mech_area_get_stage (MechArea *area)
{
  MechWindow *window;

  window = mech_area_get_window (area);

  if (!window)
    return NULL;

  return _mech_window_get_stage (window);
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
      _mech_stage_remove (priv->node);
    }

  priv->parent = parent;

  if (parent)
    {
      _mech_stage_add (_mech_area_get_node (parent), priv->node);
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
