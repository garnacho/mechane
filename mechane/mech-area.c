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

struct _MechAreaPrivate
{
  GNode *node;
  MechArea *parent;
  GPtrArray *children;

  gdouble width_requested;
  gdouble height_requested;

  guint need_width_request  : 1;
  guint need_height_request : 1;
};

static GQuark quark_window = 0;

G_DEFINE_TYPE_WITH_PRIVATE (MechArea, mech_area, G_TYPE_INITIALLY_UNOWNED)

static void
mech_area_init (MechArea *area)
{
  MechAreaPrivate *priv = mech_area_get_instance_private (area);

  priv->children = g_ptr_array_new ();

  /* Allocate stage node, even if the area isn't attached to none yet */
  priv->node = _mech_stage_node_new (area);
  priv->need_width_request = priv->need_height_request = TRUE;
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

  return ret;
}

static gdouble
mech_area_get_second_extent_impl (MechArea *area,
                                  MechAxis  axis,
                                  gdouble   other_value)
{
  gdouble ret = 0;

  return ret;
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

  object_class->finalize = mech_area_finalize;
  object_class->dispose = mech_area_dispose;

  klass->get_extent = mech_area_get_extent_impl;
  klass->get_second_extent = mech_area_get_second_extent_impl;
  klass->add = mech_area_add_impl;
  klass->remove = mech_area_remove_impl;

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
      priv->need_width_request = priv->need_height_request = TRUE;
    }

  g_object_unref (area);
}

GNode *
_mech_area_get_node (MechArea *area)
{
  MechAreaPrivate *priv;

  priv = mech_area_get_instance_private (area);
  return priv->node;
}
