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
#include <string.h>
#include <cairo/cairo-gobject.h>

#include <mechane/mech-stage-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-marshal.h>
#include <mechane/mech-surface-private.h>
#include <mechane/mech-renderer.h>
#include <mechane/mech-enums.h>
#include <mechane/mech-events.h>

#define MIN4(a,b,c,d) MIN (MIN ((a), (b)), MIN ((c), (d)))
#define MAX4(a,b,c,d) MAX (MAX ((a), (b)), MAX ((c), (d)))
#define RECT_TO_POINTS(r,p) G_STMT_START {              \
    (p)[0].x = (p)[2].x = (r).x;                        \
    (p)[0].y = (p)[1].y = (r).y;                        \
    (p)[1].x = (p)[3].x = (r).x + (r).width;            \
    (p)[2].y = (p)[3].y = (r).y + (r).height;           \
  } G_STMT_END

typedef struct _MechStagePrivate MechStagePrivate;
typedef struct _ZCacheNode ZCacheNode;
typedef struct _StageNode StageNode;
typedef struct _OffscreenNode OffscreenNode;
typedef struct _TraverseStageContext TraverseStageContext;
typedef struct _PickStageContext PickStageContext;
typedef struct _RenderingTarget RenderingTarget;
typedef struct _RenderStageContext RenderStageContext;

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

struct _RenderingTarget
{
  OffscreenNode *offscreen;
  cairo_region_t *invalidated;
  cairo_t *cr;
};

struct _RenderStageContext
{
  TraverseStageContext functions;
  GArray *target_stack;
  cairo_t *cr;
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
  guint draw_signal_id;
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
                                      MechArea  *area)
{
  MechStagePrivate *priv = mech_stage_get_instance_private (stage);
  GNode *area_node;

  area_node = _mech_area_get_node (area);

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

  parent = _mech_stage_find_container_offscreen (stage, area);

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
  g_object_set_qdata ((GObject *) area, quark_area_offscreen, node);

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

  g_object_set_qdata ((GObject *) offscreen->area, quark_area_offscreen, NULL);
  g_object_unref (offscreen->node.data);
  g_node_unlink ((GNode *) offscreen);
}

static OffscreenNode *
_mech_stage_check_needs_offscreen (MechStage *stage,
                                   MechArea  *area)
{
  OffscreenNode *offscreen;

  offscreen = _area_peek_offscreen (area);

  if (mech_area_get_matrix (area, NULL))
    {
      if (!offscreen)
        offscreen = _mech_stage_create_offscreen_node (stage, area);
    }
  else if (offscreen)
    {
      _mech_stage_destroy_offscreen_node (offscreen, FALSE);
      offscreen = NULL;
    }

  return offscreen;
}


static void
_mech_stage_update_offscreen (MechStage     *stage,
                              OffscreenNode *offscreen)
{
  gint width, height;

  _mech_area_guess_offscreen_size (offscreen->area, &width, &height);
  _mech_surface_set_size (offscreen->node.data, width, height);
}

static RenderingTarget *
render_stage_context_push_target (RenderStageContext *context,
                                  OffscreenNode      *offscreen)
{
  RenderingTarget target = { 0 };

  if (!context->target_stack)
    context->target_stack = g_array_new (FALSE, FALSE,
                                         sizeof (RenderingTarget));

  target.offscreen = offscreen;
  _mech_surface_acquire (offscreen->node.data);

  if (offscreen->node.parent)
    target.cr = _mech_surface_cairo_create (offscreen->node.data);
  else
    target.cr = cairo_reference (context->cr);

  _mech_surface_apply_clip (offscreen->node.data, target.cr);
  target.invalidated = _mech_surface_get_clip (offscreen->node.data);
  g_array_append_val (context->target_stack, target);

  return &g_array_index (context->target_stack, RenderingTarget,
                         context->target_stack->len - 1);
}

static RenderingTarget *
render_stage_context_lookup_target (RenderStageContext *context)
{
  if (context->target_stack && context->target_stack->len > 0)
    return &g_array_index (context->target_stack, RenderingTarget,
                           context->target_stack->len - 1);
  return NULL;
}

static OffscreenNode *
render_stage_context_pop_target (RenderStageContext *context)
{
  OffscreenNode *offscreen;
  RenderingTarget *target;

  target = render_stage_context_lookup_target (context);
  g_assert (target != NULL);

  cairo_destroy (target->cr);
  cairo_region_destroy (target->invalidated);
  offscreen = target->offscreen;

  g_array_remove_index (context->target_stack,
                        context->target_stack->len - 1);

  _mech_surface_push_update (offscreen->node.data);
  _mech_surface_release (offscreen->node.data);

  return offscreen;
}

static gboolean
render_stage_context_area_overlaps_clip (RenderStageContext *context,
                                         MechStage          *stage,
                                         MechArea           *area)
{
  cairo_rectangle_t renderable, alloc, parent;
  cairo_rectangle_int_t rect;
  RenderingTarget *target;

  target = render_stage_context_lookup_target (context);

  if (cairo_region_is_empty (target->invalidated))
    return FALSE;

  _mech_stage_get_renderable_rect (stage, area, &renderable);
  _mech_area_get_stage_rect (target->offscreen->area, &parent);
  _mech_area_get_stage_rect (area, &alloc);

  renderable.x += alloc.x - parent.x;
  renderable.y += alloc.y - parent.y;

  rect.x = floor (renderable.x);
  rect.y = floor (renderable.y);
  rect.width = ceil (renderable.width);
  rect.height = ceil (renderable.height);

  return cairo_region_contains_rectangle (target->invalidated,
                                          &rect) != CAIRO_REGION_OVERLAP_OUT;
}

static gboolean
render_stage_enter (MechStage          *stage,
                    GNode              *node,
                    RenderStageContext *context)
{
  MechArea *area = node->data;
  OffscreenNode *offscreen;
  RenderingTarget *target;
  cairo_rectangle_t rect;
  cairo_matrix_t matrix;
  gboolean has_matrix;

  has_matrix = mech_area_get_matrix (area, &matrix);
  target = render_stage_context_lookup_target (context);
  cairo_save (target->cr);

  if (!mech_area_get_visible (area))
    return FALSE;

  _mech_area_get_stage_rect (area, &rect);

  if (rect.width < 1 || rect.height < 1)
    return FALSE;

  if (!has_matrix &&
      !render_stage_context_area_overlaps_clip (context, stage, area))
    return FALSE;

  if (node->parent)
    {
      cairo_rectangle_t parent;

      _mech_area_get_stage_rect (node->parent->data, &parent);
      cairo_translate (target->cr, rect.x - parent.x, rect.y - parent.y);
    }

  if (has_matrix)
    {
      cairo_matrix_t cur;

      cairo_get_matrix (target->cr, &cur);
      cairo_matrix_multiply (&cur, &matrix, &cur);
      cairo_set_matrix (target->cr, &cur);
    }

  offscreen = _mech_stage_check_needs_offscreen (stage, area);

  if (offscreen)
    {
      _mech_stage_update_offscreen (stage, offscreen);
      render_stage_context_push_target (context, offscreen);

      /* Check again using the new target's invalidated area */
      if (!render_stage_context_area_overlaps_clip (context, stage, area))
        return FALSE;
    }

  return TRUE;
}

static void
render_stage_leave (MechStage          *stage,
                    GNode              *node,
                    RenderStageContext *context)
{
  MechArea *area = node->data;
  RenderingTarget *target;
  cairo_rectangle_t rect;

  target = render_stage_context_lookup_target (context);

  if (render_stage_context_area_overlaps_clip (context, stage, area))
    {
      MechRenderer *renderer;
      MechBorder border;

      _mech_area_get_stage_rect (area, &rect);
      renderer = mech_area_get_renderer (area);
      mech_renderer_get_border_extents (renderer, MECH_EXTENT_BORDER, &border);

      mech_renderer_render_border (renderer, target->cr,
                                   border.left, border.top,
                                   rect.width - (border.left + border.right),
                                   rect.height - (border.top + border.bottom));
    }

  if (target->offscreen->area == area && !G_NODE_IS_ROOT (node))
    {
      OffscreenNode *offscreen;

      offscreen = render_stage_context_pop_target (context);
      target = render_stage_context_lookup_target (context);
      _mech_surface_render (offscreen->node.data, target->cr);
    }

  cairo_restore (target->cr);
}

static TraverseFlags
render_stage_visit (MechStage          *stage,
                    const GNode        *node,
                    RenderStageContext *context)
{
  MechArea *area = node->data;
  RenderingTarget *target;
  MechRenderer *renderer;
  cairo_rectangle_t rect;
  MechStagePrivate *priv;
  MechBorder border;

  priv = mech_stage_get_instance_private (stage);
  target = render_stage_context_lookup_target (context);
  _mech_area_get_stage_rect (area, &rect);

  renderer = mech_area_get_renderer (area);
  mech_renderer_get_border_extents (renderer, MECH_EXTENT_PADDING, &border);

  mech_renderer_render_background (renderer, target->cr,
                                   border.left, border.top,
                                   rect.width - (border.left + border.right),
                                   rect.height - (border.top + border.bottom));

  if (mech_area_get_clip (area))
    {
      mech_renderer_get_border_extents (renderer, MECH_EXTENT_BORDER, &border);
      mech_renderer_set_border_path (renderer, target->cr,
                                     border.left, border.top,
                                     rect.width - (border.left + border.right),
                                     rect.height - (border.top + border.bottom));
      cairo_clip (target->cr);
    }

  if (priv->draw_signal_id == 0)
    priv->draw_signal_id = g_signal_lookup ("draw", MECH_TYPE_AREA);

  cairo_save (target->cr);
  mech_renderer_get_border_extents (renderer, MECH_EXTENT_CONTENT, &border);
  cairo_translate (target->cr, border.left, border.top);
  g_signal_emit (area, priv->draw_signal_id, 0, target->cr);
  cairo_restore (target->cr);

  return TRAVERSE_FLAG_CONTINUE | TRAVERSE_FLAG_RECURSE;
}

static void
render_stage_context_init (RenderStageContext *context,
                           MechStage          *stage,
                           cairo_t            *cr)
{
  MechStagePrivate *priv = mech_stage_get_instance_private (stage);

  context->functions.enter = (EnterNodeFunc) render_stage_enter;
  context->functions.leave = (LeaveNodeFunc) render_stage_leave;
  context->functions.visit = (VisitNodeFunc) render_stage_visit;
  context->cr = cairo_reference (cr);
  context->target_stack = NULL;

  render_stage_context_push_target (context, priv->offscreens);
}

static void
render_stage_context_finish (RenderStageContext *context)
{
  render_stage_context_pop_target (context);
  cairo_destroy (context->cr);

  g_assert (context->target_stack == NULL ||
            context->target_stack->len == 0);

  if (context->target_stack)
    g_array_unref (context->target_stack);
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
  OffscreenNode *offscreen = (OffscreenNode *) node;

  g_object_set_qdata ((GObject *) offscreen->area, quark_area_offscreen, NULL);
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

  _mech_stage_invalidate (stage, NULL, NULL, FALSE);

  return TRUE;
}

void
_mech_stage_render (MechStage *stage,
                    cairo_t   *cr)
{
  RenderStageContext context;

  render_stage_context_init (&context, stage, cr);
  _mech_stage_traverse (stage, &context.functions, NULL, FALSE);
  render_stage_context_finish (&context);
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

  _mech_stage_invalidate (stage, NULL, NULL, FALSE);
}

static gboolean
_stage_area_clipped_on_offscreen (MechArea       *area,
                                  OffscreenNode  *offscreen,
                                  MechArea      **clip_area)
{
  GNode *area_node;

  if (area == offscreen->area)
    return FALSE;

  area_node = _mech_area_get_node (area)->parent;

  while (area_node && area_node->data != offscreen->area)
    {
      if (mech_area_get_clip (area_node->data))
        {
          *clip_area = area_node->data;
          return TRUE;
        }

      area_node = area_node->parent;
    }

  return FALSE;
}

static void
_stage_clamp_to_area (OffscreenNode     *offscreen,
                      MechArea          *clip_area,
                      cairo_rectangle_t *rect)
{
  cairo_rectangle_t parent_rect, clip_rect, clamped;

  _mech_area_get_stage_rect (offscreen->area, &parent_rect);
  _mech_area_get_stage_rect (clip_area, &clip_rect);

  /* Set clip_rect on the same coords than rect */
  clip_rect.x -= parent_rect.x;
  clip_rect.y -= parent_rect.y;

  clamped.x = MAX (rect->x, clip_rect.x);
  clamped.y = MAX (rect->y, clip_rect.y);
  clamped.width = MIN (rect->x + rect->width,
                       clip_rect.x + clip_rect.width) - clamped.x;
  clamped.height = MIN (rect->y + rect->height,
                        clip_rect.y + clip_rect.height) - clamped.y;

  if (clamped.width < 0)
    clamped.width = 0;
  if (clamped.height < 0)
    clamped.height = 0;

  *rect = clamped;
}

void
_mech_stage_invalidate (MechStage      *stage,
                        MechArea       *area,
                        cairo_region_t *region,
                        gboolean        start_from_parent)
{
  MechStagePrivate *priv = mech_stage_get_instance_private (stage);
  OffscreenNode *offscreen = NULL, *prev = NULL;
  MechArea *current, *clip_area;
  cairo_rectangle_t rect;
  MechPoint points[4];

  if (!priv->offscreens)
    return;

  if (!area)
    area = priv->areas->node.data;

  if (!region)
    _mech_stage_get_renderable_rect (stage, area, &rect);
  else if (!cairo_region_is_empty (region))
    {
      cairo_rectangle_int_t int_rect;

      cairo_region_get_extents (region, &int_rect);
      rect.x = int_rect.x;
      rect.y = int_rect.y;
      rect.width = int_rect.width;
      rect.height = int_rect.height;
    }
  else
    return;

  RECT_TO_POINTS (rect, points);

  offscreen = _mech_stage_find_container_offscreen (stage, area);
  current = area;

  while (offscreen)
    {
      mech_area_transform_points (current, offscreen->area,
                                  (MechPoint *) &points, 4);

      rect.x = MIN4 (points[0].x, points[1].x, points[2].x, points[3].x);
      rect.y = MIN4 (points[0].y, points[1].y, points[2].y, points[3].y);
      rect.width = MAX4 (points[0].x, points[1].x, points[2].x, points[3].x) - rect.x;
      rect.height = MAX4 (points[0].y, points[1].y, points[2].y, points[3].y) - rect.y;

      if (_stage_area_clipped_on_offscreen (current, offscreen, &clip_area))
        _stage_clamp_to_area (offscreen, clip_area, &rect);

      if (!start_from_parent || offscreen->area != area)
        _mech_surface_damage (offscreen->node.data, &rect);

      RECT_TO_POINTS (rect, points);
      prev = offscreen;
      current = offscreen->area;
      offscreen = (OffscreenNode *) offscreen->node.parent;
    }
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

gboolean
_mech_stage_get_renderable_rect (MechStage         *stage,
                                 MechArea          *area,
                                 cairo_rectangle_t *rect)
{
  OffscreenNode *offscreen;

  offscreen = _mech_stage_find_container_offscreen (stage, area);

  if (!offscreen)
    return FALSE;

  return _mech_surface_area_is_rendered (offscreen->node.data, area, rect);
}
