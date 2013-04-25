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
#include <mechane/mech-enums.h>
#include <mechane/mech-events.h>

#define MIN4(a,b,c,d) MIN (MIN ((a), (b)), MIN ((c), (d)))
#define MAX4(a,b,c,d) MAX (MAX ((a), (b)), MAX ((c), (d)))

typedef struct _MechStagePrivate MechStagePrivate;
typedef struct _StageNode StageNode;
typedef struct _TraverseStageContext TraverseStageContext;

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

struct _TraverseStageContext
{
  EnterNodeFunc enter;
  LeaveNodeFunc leave;
  VisitNodeFunc visit;
};

struct _StageNode
{
  GNode node; /* data is MechArea */
  StageNode *last_child;
};

struct _MechStagePrivate
{
  StageNode *areas;

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

static void
mech_stage_class_init (MechStageClass *klass)
{
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

GNode *
_mech_stage_node_new (MechArea *area)
{
  StageNode *node;

  node = g_slice_new0 (StageNode);
  node->node.data = area;

  return (GNode *) node;
}

void
_mech_stage_node_free (GNode *node)
{
  StageNode *stage_node = (StageNode *) node;

  g_slice_free (StageNode, stage_node);
}

gboolean
_mech_stage_add (GNode *parent_node,
                 GNode *child_node)
{
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
  _stage_node_insert ((StageNode *) parent_node,
		      ((StageNode *) parent_node)->last_child,
                      (StageNode *) child_node);
  return TRUE;
}

gboolean
_mech_stage_remove (GNode *child_node)
{
  if (!child_node->parent)
    return FALSE;

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

  *width = priv->width;
  *height = priv->height;

  return TRUE;
}

void
_mech_stage_set_root (MechStage *stage,
                      MechArea  *area)
{
  MechStagePrivate *priv = mech_stage_get_instance_private (stage);

  g_assert (!priv->areas);
  priv->areas = (StageNode *) _mech_area_get_node (area);
}
