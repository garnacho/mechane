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

#include <mechane/mech-stage-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-marshal.h>
#include <mechane/mech-surface-private.h>
#include <mechane/mech-enums.h>
#include <mechane/mech-events.h>

#define MIN4(a,b,c,d) MIN (MIN ((a), (b)), MIN ((c), (d)))
#define MAX4(a,b,c,d) MAX (MAX ((a), (b)), MAX ((c), (d)))

typedef struct _MechStagePrivate MechStagePrivate;
typedef struct _ZCacheNode ZCacheNode;
typedef struct _StageNode StageNode;
typedef struct _OffscreenNode OffscreenNode;
typedef struct _TraverseStageContext TraverseStageContext;
typedef struct _PickStageContext PickStageContext;

typedef enum {
  TRAVERSE_FLAG_STOP     = 0,
  TRAVERSE_FLAG_CONTINUE = 1 << 1,
  TRAVERSE_FLAG_RECURSE  = 1 << 2,
} TraverseFlags;

typedef TraverseFlags (* VisitNodeFunc) (MechStage            *stage,
                                         GNode                *node,
                                         TraverseStageContext *context);
typedef gboolean      (* EnterNodeFunc) (MechStage            *stage,
                                         GNode                *parent,
                                         TraverseStageContext *context);
typedef void          (* LeaveNodeFunc) (MechStage            *stage,
                                         GNode                *parent,
                                         TraverseStageContext *context);

struct _ZCacheNode
{
  GNode *first_child;
  GNode *last_child;
  gint depth;
};

struct _StageNode
{
  GNode node; /* data is MechArea */
  StageNode *last_child;
  GArray *z_cache;
};

struct _OffscreenNode
{
  GNode node;  /* data is MechSurface */
  MechArea *area;
};

struct _TraverseStageContext
{
  EnterNodeFunc enter;
  LeaveNodeFunc leave;
  VisitNodeFunc visit;
};

struct _PickStageContext
{
  TraverseStageContext functions;
  MechArea *root;
  GPtrArray *areas;
  MechEventType event_type;
  gint x;
  gint y;
};

struct _MechStagePrivate
{
  StageNode *areas;
  OffscreenNode *offscreens;

  gint width;
  gint height;
};

static GQuark quark_area_offscreen = 0;

G_DEFINE_TYPE_WITH_PRIVATE (MechStage, mech_stage, G_TYPE_OBJECT)

static StageNode *
_stage_node_insert (StageNode *parent,
                    StageNode *after,
                    StageNode *node)
{
  if (!parent || !node)
    return NULL;

  if (after)
    g_assert (after->node.parent == (GNode *) parent);

  if (!parent->node.children || after == parent->last_child)
    parent->last_child = node;

  return (StageNode *) g_node_insert_after ((GNode *) parent,
                                            (GNode *) after,
                                            (GNode *) node);
}

/* Rendering */
static OffscreenNode *
_area_peek_offscreen (MechArea *area)
{
  return g_object_get_qdata ((GObject *) area, quark_area_offscreen);
}

static OffscreenNode *
_mech_stage_find_container_offscreen (MechStage *stage,
                                      MechArea  *area,
                                      gboolean   start_from_parent)
{
  MechStagePrivate *priv = mech_stage_get_instance_private (stage);
  GNode *area_node;

  area_node = _mech_area_get_node (area);

  if (start_from_parent)
    area_node = area_node->parent;

  while (area_node)
    {
      OffscreenNode *offscreen;

      offscreen = _area_peek_offscreen (area_node->data);

      if (offscreen)
        return offscreen;

      area_node = area_node->parent;
    }

  return priv->offscreens;
}

static OffscreenNode *
_mech_stage_create_offscreen_node (MechStage *stage,
                                   MechArea  *area)
{
  OffscreenNode *parent, *node;
  GNode *child;

  parent = _mech_stage_find_container_offscreen (stage, area, TRUE);

  node = g_new0 (OffscreenNode, 1);
  node->node.data = _mech_surface_new (area);
  node->area = area;

  /* Set all older child nodes below this new node */
  for (child = parent->node.children; child; child = child->next)
    {
      OffscreenNode *child_offscreen = (OffscreenNode *) child;

      if (mech_area_is_ancestor (child_offscreen->area, area))
        {
          g_node_unlink (child);
          g_node_append ((GNode *) node, child);
        }
    }

  g_node_append ((GNode *) parent, (GNode *) node);

  return node;
}

static void
_mech_stage_destroy_offscreen_node (OffscreenNode *offscreen,
                                    gboolean       recurse)
{
  GNode *child;

  if (!recurse && offscreen->node.parent)
    {
      for (child = offscreen->node.children; child; child = child->next)
        {
          g_node_unlink (child);
          g_node_append (offscreen->node.parent, child);
        }
    }
  else if (recurse)
    {
      for (child = offscreen->node.children; child; child = child->next)
        _mech_stage_destroy_offscreen_node ((OffscreenNode *) child, recurse);
    }

  g_object_unref (offscreen->node.data);
  g_node_unlink ((GNode *) offscreen);
}

/* Picking */
static TraverseFlags
pick_stage_visit (MechStage        *stage,
                  const GNode      *node,
                  PickStageContext *context)
{
  cairo_region_t *region;
  MechStagePrivate *priv;
  MechArea *root, *area;
  gboolean inside;
  gdouble x, y;

  priv = mech_stage_get_instance_private (stage);
  area = node->data;
  root = context->root;
  x = context->x;
  y = context->y;

  if (!root)
    root = priv->areas->node.data;

  mech_area_transform_point (root, area, &x, &y);
  region = mech_area_get_shape (area);
  inside = cairo_region_contains_point (region, (int) x, (int) y);
  cairo_region_destroy (region);

  if (!inside)
    return TRAVERSE_FLAG_CONTINUE;

  if (context->event_type == 0 ||
      mech_area_handles_event (area, context->event_type))
    g_ptr_array_add (context->areas, g_object_ref (area));

  return TRAVERSE_FLAG_CONTINUE | TRAVERSE_FLAG_RECURSE;
}

static void
pick_stage_context_init (PickStageContext *context,
                         MechArea         *root,
                         MechEventType     event_type,
                         gint              x,
                         gint              y)
{
  context->functions.enter = NULL;
  context->functions.leave = NULL;
  context->functions.visit = (VisitNodeFunc) pick_stage_visit;
  context->areas = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  context->root = root;
  context->event_type = event_type;
  context->x = x;
  context->y = y;
}

static void
pick_stage_context_finish (PickStageContext *context)
{
  g_ptr_array_unref (context->areas);
}

/* MechStage */
static gboolean
_stage_dispose_offscreens (GNode    *node,
                           gpointer  user_data)
{
  g_object_unref (node->data);
  return FALSE;
}

static void
mech_stage_finalize (GObject *object)
{
  MechStagePrivate *priv;

  priv = mech_stage_get_instance_private ((MechStage *) object);

  if (priv->offscreens)
    {
      g_node_traverse ((GNode *) priv->offscreens, G_POST_ORDER,
                       G_TRAVERSE_ALL, -1, _stage_dispose_offscreens,
                       object);
      g_node_destroy ((GNode *) priv->offscreens);
    }

  G_OBJECT_CLASS (mech_stage_parent_class)->finalize (object);
}

static void
mech_stage_class_init (MechStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_stage_finalize;

  quark_area_offscreen = g_quark_from_static_string ("mech-stage-offscreen");
}

static void
mech_stage_init (MechStage *stage)
{
}

MechStage *
_mech_stage_new (void)
{
  return g_object_new (MECH_TYPE_STAGE, NULL);
}

static void
_mech_stage_pop_stack (MechStage             *stage,
                       TraverseStageContext  *context,
                       GList                **stack,
                       GNode                 *cur)
{
  while (*stack)
    {
      GNode *node = (*stack)->data;

      if (!cur || (cur->parent != node &&
                   (!node->next || cur != node)))
        {
          *stack = g_list_remove (*stack, node);

          if (context->leave)
            context->leave (stage, node, context);
        }
      else
        break;
    }
}

static void
_mech_stage_traverse (MechStage            *stage,
                      TraverseStageContext *context,
                      GNode                *node,
                      gboolean              reverse)
{
  GList *test_nodes, *stack = NULL;
  MechStagePrivate *priv;
  GNode *cur, *next;

  priv = mech_stage_get_instance_private (stage);

  if (!priv->areas)
    return;

  if (node)
    {
      if (!g_node_is_ancestor ((GNode *) priv->areas, node))
        return;

      test_nodes = g_list_prepend (NULL, node);
    }
  else
    test_nodes = g_list_prepend (NULL, priv->areas);

  while (test_nodes)
    {
      TraverseFlags flags = TRAVERSE_FLAG_CONTINUE;
      gboolean enter = TRUE;

      cur = test_nodes->data;
      test_nodes = g_list_remove (test_nodes, cur);
      _mech_stage_pop_stack (stage, context, &stack, cur);

      if (!mech_area_get_visible (cur->data))
        enter = FALSE;
      else if (context->enter)
        {
          enter = context->enter (stage, cur, context);
          stack = g_list_prepend (stack, cur);
        }

      if (enter)
        flags = context->visit (stage, cur, context);

      if (reverse)
        next = cur->prev;
      else
        next = cur->next;

      if (!reverse && next && (flags & TRAVERSE_FLAG_CONTINUE))
        {
          /* Continue traversal on to the next sibling */
          test_nodes = g_list_prepend (test_nodes, next);
        }

      if (cur->children &&
          flags & TRAVERSE_FLAG_RECURSE)
        {
          /* Continue traversal across children of this node */
          if (reverse)
            test_nodes = g_list_prepend (test_nodes, ((StageNode *) cur)->last_child);
          else
            test_nodes = g_list_prepend (test_nodes, cur->children);
        }

      if (reverse && next && (flags & TRAVERSE_FLAG_CONTINUE))
        {
          /* Continue traversal on to the next sibling */
          test_nodes = g_list_prepend (test_nodes, next);
        }
    }

  _mech_stage_pop_stack (stage, context, &stack, NULL);
}

static GNode*
_mech_stage_get_insertion_node (StageNode *parent,
                                StageNode *child_node)
{
  ZCacheNode new, *cache, *prev = NULL;
  gint depth;
  guint i;

  depth = mech_area_get_depth (child_node->node.data);

  for (i = 0; i < parent->z_cache->len; i++)
    {
      cache = &g_array_index (parent->z_cache, ZCacheNode, i);

      if (cache->depth == depth)
        {
          GNode *prev_last = cache->last_child;

          cache->last_child = (GNode *) child_node;
          return prev_last;
        }
      else if (cache->depth > depth)
        break;

      prev = cache;
    }

  new.depth = depth;
  new.first_child = new.last_child = (GNode *) child_node;

  if (i < parent->z_cache->len)
    g_array_insert_val (parent->z_cache, i, new);
  else
    g_array_append_val (parent->z_cache, new);

  return (prev) ? prev->last_child : NULL;
}

GNode *
_mech_stage_node_new (MechArea *area)
{
  StageNode *node;

  node = g_slice_new0 (StageNode);
  node->z_cache = g_array_new (FALSE, TRUE, sizeof (ZCacheNode));
  node->node.data = area;

  return (GNode *) node;
}

void
_mech_stage_node_free (GNode *node)
{
  StageNode *stage_node = (StageNode *) node;

  g_array_free (stage_node->z_cache, TRUE);
  g_slice_free (StageNode, stage_node);
}

gboolean
_mech_stage_add (GNode *parent_node,
                 GNode *child_node)
{
  GNode *sibling;

  if (!parent_node || !child_node)
    return FALSE;

  if (child_node->parent)
    {
      g_warning ("Area '%s' already had a parent area (%s)",
                 G_OBJECT_TYPE_NAME (child_node->data),
                 G_OBJECT_TYPE_NAME (child_node->parent->data));
      return FALSE;
    }

  g_object_ref_sink (child_node->data);
  sibling = _mech_stage_get_insertion_node ((StageNode *) parent_node,
                                            (StageNode *) child_node);
  _stage_node_insert ((StageNode *) parent_node,
                      (StageNode *) sibling,
                      (StageNode *) child_node);
  return TRUE;
}

gboolean
_mech_stage_remove (GNode *child_node)
{
  OffscreenNode *offscreen;
  ZCacheNode *cache;
  StageNode *parent;
  gint i, depth;

  if (!child_node->parent)
    return FALSE;

  parent = (StageNode *) child_node->parent;
  depth = mech_area_get_depth (child_node->data);
  offscreen = _area_peek_offscreen (child_node->data);

  if (offscreen)
    _mech_stage_destroy_offscreen_node (offscreen, TRUE);

  for (i = 0; i < parent->z_cache->len; i++)
    {
      cache = &g_array_index (parent->z_cache, ZCacheNode, i);

      if (cache->depth == depth && cache->last_child == child_node)
        g_array_remove_index (parent->z_cache, i);
      else if (cache->depth > depth)
        break;
    }

  g_node_unlink (child_node);
  g_object_unref (child_node->data);

  return TRUE;
}

void
_mech_stage_get_size (MechStage *stage,
                      gint      *width,
                      gint      *height)
{
  MechStagePrivate *priv;
  gdouble x_value, y_value;

  priv = mech_stage_get_instance_private (stage);
  x_value = mech_area_get_extent (priv->areas->node.data, MECH_AXIS_X);
  y_value = mech_area_get_second_extent (priv->areas->node.data,
                                         MECH_AXIS_Y, x_value);
  if (width)
    *width = MAX (1, x_value);

  if (height)
    *height = MAX (1, y_value);
}

gboolean
_mech_stage_set_size (MechStage *stage,
                      gint      *width,
                      gint      *height)
{
  MechStagePrivate *priv;
  cairo_rectangle_t rect;

  priv = mech_stage_get_instance_private (stage);
  rect.x = rect.y = 0;
  rect.width = *width;
  rect.height = *height;
  mech_area_allocate_size (priv->areas->node.data, &rect);

  priv->width = MAX (1, rect.width);
  priv->height = MAX (1, rect.height);
  _mech_surface_set_size (priv->offscreens->node.data,
                          priv->width,
                          priv->height);

  *width = priv->width;
  *height = priv->height;

  return TRUE;
}

GPtrArray *
_mech_stage_pick_for_event (MechStage     *stage,
                            MechArea      *area,
                            MechEventType  event_type,
                            gdouble        x,
                            gdouble        y)
{
  PickStageContext context;
  GNode *node = NULL;
  GPtrArray *areas;

  if (area)
    {
      if (stage != _mech_area_get_stage (area))
        return NULL;

      node = _mech_area_get_node (area);
    }

  pick_stage_context_init (&context, area, event_type, x, y);
  _mech_stage_traverse (stage, &context.functions, node, FALSE);

  if (context.areas && context.areas->len > 0)
    areas = g_ptr_array_ref (context.areas);
  else
    areas = NULL;

  pick_stage_context_finish (&context);

  return areas;
}

GPtrArray *
_mech_stage_pick (MechStage *stage,
                  MechArea  *area,
                  gdouble    x,
                  gdouble    y)
{
  return _mech_stage_pick_for_event (stage, area, 0, x, y);
}

void
_mech_stage_set_root (MechStage *stage,
                      MechArea  *area)
{
  MechStagePrivate *priv = mech_stage_get_instance_private (stage);

  g_assert (!priv->areas);
  priv->areas = (StageNode *) _mech_area_get_node (area);
}

void
_mech_stage_set_root_surface (MechStage   *stage,
                              MechSurface *surface)
{
  MechStagePrivate *priv = mech_stage_get_instance_private (stage);
  OffscreenNode *offscreen;

  g_assert (!priv->offscreens);

  offscreen = g_new0 (OffscreenNode, 1);
  offscreen->node.data = surface;
  offscreen->area = priv->areas->node.data;
  priv->offscreens = offscreen;
}

void
_mech_stage_notify_depth_change (MechStage *stage,
                                 MechArea  *area)
{
  GNode *node, *parent, *sibling;

  node = _mech_area_get_node (area);
  parent = node->parent;

  if (!parent)
    return;

  g_node_unlink (node);
  sibling = _mech_stage_get_insertion_node ((StageNode *) parent,
                                            (StageNode *) node);
  _stage_node_insert ((StageNode *) parent,
                      (StageNode *) sibling,
                      (StageNode *) node);
}
