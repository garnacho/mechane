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

#include <cairo-gobject.h>
#include <string.h>
#include "mech-style-private.h"
#include "mech-enum-types.h"
#include "mech-pattern-private.h"
#include "mech-renderer-private.h"

#define UNPACK_VALUE(value,type,def) \
  (value) ? g_value_get_##type ((value)) : (def)

typedef struct _MechStylePrivate MechStylePrivate;
typedef struct _StyleIterator StyleIterator;
typedef struct _StyleProperty StyleProperty;
typedef struct _StylePropertySet StylePropertySet;
typedef struct _StyleTreeNode StyleTreeNode;
typedef struct _PathContext PathContext;

static GArray *registered_properties = NULL;
static GHashTable *toplevel_properties = NULL;

struct _StyleProperty
{
  guint32 property;
  gint16 layer;
  guint16 flags;
  GValue value;
};

struct _StylePropertySet
{
  GArray *properties;
  guint16 resolved : 1;
};

struct _StyleIterator
{
  StylePropertySet *set;
  guint current;
};

struct _PathContext
{
  GQuark elem_quark;
  guint state_flags;
  StylePropertySet *properties;
};

struct _StyleTreeNode
{
  StyleTreeNode *parent;
  GArray *children;
  StylePropertySet *properties;
  GQuark element;
  guint state_flags;
};

struct _MechStylePrivate
{
  GArray *path_context;
  StyleTreeNode *style_tree;
};

static void _style_tree_node_free    (StyleTreeNode    *node);
static void _style_property_set_free (StylePropertySet *set);

G_DEFINE_TYPE_WITH_PRIVATE (MechStyle, mech_style, G_TYPE_OBJECT)

static void
mech_style_finalize (GObject *object)
{
  MechStylePrivate *priv;
  guint i;

  priv = mech_style_get_instance_private ((MechStyle *) object);
  _style_tree_node_free (priv->style_tree);

  for (i = 0; i < priv->path_context->len; i++)
    {
      PathContext *context;

      context = &g_array_index (priv->path_context, PathContext, i);
      _style_property_set_free (context->properties);
    }

  g_array_unref (priv->path_context);

  G_OBJECT_CLASS (mech_style_parent_class)->finalize (object);
}

static void
mech_style_class_init (MechStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_style_finalize;
}

static StylePropertySet *
_style_property_set_new (void)
{
  StylePropertySet *set;

  set = g_new0 (StylePropertySet, 1);
  set->properties = g_array_new (FALSE, FALSE, sizeof (StyleProperty));

  return set;
}

static void
_style_property_set_free (StylePropertySet *set)
{
  guint i;

  for (i = 0; i < set->properties->len; i++)
    {
      StyleProperty *property;

      property = &g_array_index (set->properties, StyleProperty, i);
      g_value_unset (&property->value);
    }

  g_array_free (set->properties, TRUE);
  g_free (set);
}

static gint
_style_property_compare (StyleProperty *prop1,
                         guint          property,
                         gint           layer)
{
  /* Order is ascending by layer, then ascending by property */
  if (prop1->layer < layer)
    return 1;
  else if (prop1->layer > layer)
    return -1;
  else if (prop1->property < property)
    return 1;
  else if (prop1->property > property)
    return -1;

  return 0;
}

static gboolean
_style_iterator_finished (StyleIterator *iter)
{
  return iter->current >= iter->set->properties->len;
}

static StyleProperty *
_style_iterator_current (StyleIterator *iter)
{
  if (_style_iterator_finished (iter))
    return NULL;

  return &g_array_index (iter->set->properties, StyleProperty,
                         iter->current);
}

static gboolean
_style_iterator_current_group (StyleIterator *iter,
                               guint         *property,
                               gint          *layer,
                               gint          *flags)
{
  StyleProperty *prop;

  prop = _style_iterator_current (iter);

  if (!prop)
    return FALSE;

  if (property)
    *property = prop->property;
  if (layer)
    *layer = prop->layer;
  if (flags)
    *flags = prop->flags;

  return TRUE;
}

static gboolean
_style_iterator_check_group (StyleIterator *iter,
                             guint          property,
                             gint           layer)
{
  guint cur_property;
  gint cur_layer;

  if (!_style_iterator_current_group (iter, &cur_property, &cur_layer, NULL))
    return FALSE;

  return cur_property == property && cur_layer == layer;
}

static gboolean
_style_iterator_next (StyleIterator *iter)
{
  iter->current = MIN (iter->current + 1,
                       iter->set->properties->len);
  return iter->current < iter->set->properties->len;
}

static gboolean
_style_iterator_next_property (StyleIterator *iter)
{
  guint property;
  gint layer;

  if (!_style_iterator_current_group (iter, &property, &layer, NULL))
    return FALSE;

  while (_style_iterator_next (iter))
    {
      if (!_style_iterator_check_group (iter, property, layer))
        break;
    }

  return !_style_iterator_finished (iter);
}

static gboolean
_style_iterator_forward_position (StyleIterator *iter,
                                  guint          property,
                                  gint           layer)
{
  StyleProperty *prop;
  gint match;

  if (_style_iterator_finished (iter))
    return FALSE;

  do
    {
      prop = _style_iterator_current (iter);
      match = _style_property_compare (prop, property, layer);

      if (match <= 0)
        break;
    }
  while (_style_iterator_next_property (iter));

  return match == 0;
}

static void
_style_iterator_add (StyleIterator *iter,
                     guint          property,
                     gint           layer,
                     guint          flags,
                     GValue        *value)
{
  StyleProperty *prev, new_property = { 0 };
  guint i;

  new_property.property = property;
  new_property.layer = layer;
  new_property.flags = flags;
  g_value_init (&new_property.value, G_VALUE_TYPE (value));
  g_value_copy (value, &new_property.value);

  if (_style_iterator_finished (iter))
    g_array_append_val (iter->set->properties, new_property);
  else
    {
      prev = &g_array_index (iter->set->properties, StyleProperty, iter->current);
      i = iter->current + 1;

      if (prev->flags == flags &&
          _style_property_compare (prev, property, layer) == 0)
        {
          g_value_unset (&prev->value);
          *prev = new_property;
        }
      else
        g_array_insert_val (iter->set->properties, iter->current, new_property);

      /* Check ahead to remove duplicates for these
       * specific flags within the group
       */
      while (i < iter->set->properties->len)
        {
          StyleProperty *prop;
          guint coinciding;

          prop = &g_array_index (iter->set->properties, StyleProperty, i);

          if (_style_property_compare (prop, property, layer) != 0)
            break;

          coinciding = prop->flags & flags;
          prop->flags &= ~(coinciding);
          flags &= ~(coinciding);

          if (prop->flags == 0)
            g_array_remove_index (iter->set->properties, i);
          else
            i++;

          if (flags == 0)
            break;
        }
    }

  _style_iterator_next (iter);
}

static void
_style_property_set_combine (StylePropertySet       *set,
                             const StylePropertySet *source)
{
  StyleIterator source_iter = { (StylePropertySet *) source, 0 };
  StyleIterator iter = { set, 0 };
  guint flags_set, flags, property;
  StyleProperty *prop;
  gboolean found;
  gint layer;

  while (!_style_iterator_finished (&source_iter))
    {
      _style_iterator_current_group (&source_iter, &property, &layer, NULL);
      found = FALSE;
      flags_set = 0;

      if (_style_iterator_forward_position (&iter, property, layer))
        {
          /* Go to the last position for this
           * property/layer and accumulate flags
           */
          while (!_style_iterator_finished (&iter) &&
                 _style_iterator_check_group (&iter, property, layer))
            {
              prop = _style_iterator_current (&iter);
              _style_iterator_next (&iter);
              flags_set |= prop->flags;
              found = TRUE;
            }
        }

      /* Run through the source values for this group and add them,
       * taking care of unsetting the flags that are already set
       * in the destination.
       */
      while (!_style_iterator_finished (&source_iter) &&
             _style_iterator_check_group (&source_iter, property, layer))
        {
          prop = _style_iterator_current (&source_iter);
          _style_iterator_next (&source_iter);

          if (found)
            {
              flags = prop->flags & ~(flags_set);

              if (flags == 0)
                continue;
            }
          else
            flags = prop->flags;

          _style_iterator_add (&iter, prop->property, prop->layer,
                               flags, &prop->value);
        }
    }
}

static gboolean
_style_iterator_unpack (StyleIterator *iter,
                        GValue        *value)
{
  StyleProperty *prop;

  prop = _style_iterator_current (iter);

  if (!prop)
    return FALSE;

  if (value)
    *value = prop->value;

  _style_iterator_next (iter);
  return TRUE;
}

static gboolean
_style_iterator_unpack_values (StyleIterator  *iter,
                               GValue        **values_out,
                               guint           n_values)
{
  guint flags = 0, set_values = 0, val, property;
  StyleProperty *prop;
  gint layer;

  if (_style_iterator_finished (iter))
    return FALSE;

  _style_iterator_current_group (iter, &property, &layer, NULL);

  while (!_style_iterator_finished (iter) &&
         _style_iterator_check_group (iter, property, layer))
    {
      prop = _style_iterator_current (iter);
      _style_iterator_next (iter);

      flags = prop->flags & ~(set_values);
      set_values |= flags;

      if (flags == 0)
        continue;

      for (val = 0; val < n_values; val++)
        {
          if (flags & 1)
            values_out[val] = &prop->value;

          flags >>= 1;

          if (flags == 0)
            break;
        }
    }

  return set_values != 0;
}

static void
_style_iterator_unpack_border (StyleIterator *iter,
                               MechBorder    *border)
{
  GValue *distances[4] = { 0 };
  GValue *units[4] = { 0 };
  guint cur_property;
  gint cur_layer;

  _style_iterator_current_group (iter, &cur_property, &cur_layer, NULL);
  _style_iterator_unpack_values (iter, distances, 4);

  if (_style_iterator_forward_position (iter, cur_property + 1, cur_layer))
    {
      _style_iterator_unpack_values (iter, units, 4);
      border->left_unit = UNPACK_VALUE (units[MECH_SIDE_LEFT], enum, MECH_UNIT_PX);
      border->right_unit = UNPACK_VALUE (units[MECH_SIDE_RIGHT], enum, MECH_UNIT_PX);
      border->top_unit = UNPACK_VALUE (units[MECH_SIDE_TOP], enum, MECH_UNIT_PX);
      border->bottom_unit = UNPACK_VALUE (units[MECH_SIDE_BOTTOM], enum, MECH_UNIT_PX);
    }

  border->left = UNPACK_VALUE (distances[MECH_SIDE_LEFT], double, 0);
  border->right = UNPACK_VALUE (distances[MECH_SIDE_RIGHT], double, 0);
  border->top = UNPACK_VALUE (distances[MECH_SIDE_TOP], double, 0);
  border->bottom = UNPACK_VALUE (distances[MECH_SIDE_BOTTOM], double, 0);
}

static StyleTreeNode *
_style_tree_node_new (GQuark         elem_quark,
                      guint          state_flags,
                      StyleTreeNode *parent)
{
  StyleTreeNode *node;

  node = g_new0 (StyleTreeNode, 1);
  node->parent = parent;
  node->element = elem_quark;
  node->state_flags = state_flags;
  node->children = g_array_new (FALSE, FALSE, sizeof (StyleTreeNode*));
  node->properties = _style_property_set_new ();

  return node;
}

static void
_style_tree_node_free (StyleTreeNode *node)
{
  guint i;

  _style_property_set_free (node->properties);

  for (i = 0; i < node->children->len; i++)
    _style_tree_node_free (g_array_index (node->children,
                                          StyleTreeNode *, i));

  g_array_free (node->children, TRUE);
  g_free (node);
}

static gint
_style_tree_node_compare (StyleTreeNode *node1,
                          GQuark         quark,
                          guint          state)
{
  if (node1->element < quark)
    return -1;
  else if (node1->element > quark)
    return 1;
  else
    return 0;
}

static void
_style_tree_node_ensure_resolved (StyleTreeNode *node)
{
  StyleTreeNode *parent;

  if (node->properties->resolved)
    return;

  parent = node->parent;

  while (parent)
    {
      _style_property_set_combine (node->properties,
                                   parent->properties);

      if (parent->properties->resolved)
        break;

      parent = parent->parent;
    }

  node->properties->resolved = TRUE;
}

static StyleTreeNode *
_find_child_node (StyleTreeNode *node,
                  GQuark         elem_quark,
                  guint          state_flags,
                  gboolean       create)
{
  StyleTreeNode *child, *candidate = NULL;
  gint diff, i;

  /* FIXME: binary search */
  for (i = 0; i < node->children->len; i++)
    {
      child = g_array_index (node->children, gpointer, i);
      diff = _style_tree_node_compare (child, elem_quark, state_flags);

      if (diff < 0)
        continue;
      else if (diff > 0)
        break;

      if ((child->state_flags & state_flags) == child->state_flags)
        {
          candidate = child;

          if (state_flags == child->state_flags)
            break;
        }
      else if (child->state_flags > state_flags)
        break;
    }

  if (!create)
    return candidate;

  child = _style_tree_node_new (elem_quark, state_flags, node);

  if (i == node->children->len)
    g_array_append_val (node->children, child);
  else
    g_array_insert_val (node->children, i, child);

  return child;
}

static StyleTreeNode *
_mech_style_resolve_path_node (MechStyle *style)
{
  MechStylePrivate *priv;
  PathContext *context;
  StyleTreeNode *node;
  gint i;

  priv = mech_style_get_instance_private (style);
  node = priv->style_tree;

  for (i = priv->path_context->len - 1; i >= 0; i--)
    {
      context = &g_array_index (priv->path_context, PathContext, i);
      node = _find_child_node (node, context->elem_quark,
                               context->state_flags, TRUE);
    }

  return node;
}

static void
mech_style_init (MechStyle *style)
{
  MechStylePrivate *priv;

  priv = mech_style_get_instance_private (style);
  priv->path_context = g_array_new (FALSE, FALSE, sizeof (PathContext));
  priv->style_tree = _style_tree_node_new (0, 0, NULL);
}

MechStyle *
mech_style_new (void)
{
  return g_object_new (MECH_TYPE_STYLE, NULL);
}

void
_mech_style_push_path (MechStyle      *style,
                       const gchar    *element,
                       MechStateFlags  state)
{
  MechStylePrivate *priv;
  PathContext data;

  g_return_if_fail (MECH_IS_STYLE (style));

  priv = mech_style_get_instance_private (style);
  data.elem_quark = g_quark_from_string (element);
  data.properties = _style_property_set_new ();
  data.state_flags = state;

  g_array_append_val (priv->path_context, data);
}

StylePropertySet *
_style_peek_context_properties (MechStyle *style)
{
  MechStylePrivate *priv;
  PathContext *context;

  priv = mech_style_get_instance_private (style);

  if (priv->path_context->len == 0)
    return priv->style_tree->properties;

  context = &g_array_index (priv->path_context, PathContext,
                            priv->path_context->len - 1);
  return context->properties;
}

void
_mech_style_pop_path (MechStyle *style)
{
  StylePropertySet *properties;
  MechStylePrivate *priv;
  StyleTreeNode *node;

  g_return_if_fail (MECH_IS_STYLE (style));

  priv = mech_style_get_instance_private (style);

  if (priv->path_context->len == 0)
    {
      g_warning ("Unmatched pop_path() call");
      return;
    }

  /* Transfer properties from the context to the style tree node */
  node = _mech_style_resolve_path_node (style);
  properties = _style_peek_context_properties (style);
  _style_property_set_combine (node->properties, properties);

  g_array_remove_index (priv->path_context, priv->path_context->len - 1);
  _style_property_set_free (properties);
}

void
_mech_style_set_property (MechStyle *style,
                          guint      property,
                          gint       layer,
                          guint      flags,
                          GValue    *value)
{
  StyleIterator iter = { 0 };

  g_return_if_fail (MECH_IS_STYLE (style));
  g_return_if_fail (value != NULL);

  iter.set = _style_peek_context_properties (style);
  _style_iterator_forward_position (&iter, property, layer);
  _style_iterator_add (&iter, property, layer, flags, value);
}

static void
_style_apply_current_property (StyleIterator *iter,
                               MechRenderer  *renderer)
{
  MechBorder border = { 0, 0, 0, 0, 0 };
  gint cur_layer, cur_flags;
  guint cur_property;
  MechPattern *pat;
  GValue value;

  _style_iterator_current_group (iter, &cur_property, &cur_layer, &cur_flags);

  switch (cur_property)
    {
    case MECH_PROPERTY_MARGIN:
      _style_iterator_unpack_border (iter, &border);
      _mech_renderer_set_margin (renderer, &border);
      break;
    case MECH_PROPERTY_PADDING:
      _style_iterator_unpack_border (iter, &border);
      _mech_renderer_set_padding (renderer, &border);
      break;
    case MECH_PROPERTY_FONT:
      _style_iterator_unpack (iter, &value);
      _mech_renderer_set_font_family (renderer, g_value_get_string (&value));
      break;
    case MECH_PROPERTY_FONT_SIZE:
      {
        GValue size, unit;

        _style_iterator_unpack (iter, &size);
        _style_iterator_unpack (iter, &unit);
        _mech_renderer_set_font_size (renderer, g_value_get_double (&size),
                                      g_value_get_enum (&unit));
        break;
      }
    case MECH_PROPERTY_CORNER_RADIUS:
      _style_iterator_unpack (iter, &value);
      _mech_renderer_set_corner_radius (renderer, cur_flags,
                                        g_value_get_double (&value));
      break;
    case MECH_PROPERTY_BORDER:
      _style_iterator_unpack_border (iter, &border);
      _style_iterator_unpack (iter, &value);
      pat = g_value_get_boxed (&value);
      _mech_renderer_add_border (renderer, pat, &border);
      break;
    case MECH_PROPERTY_BACKGROUND_PATTERN:
      _style_iterator_unpack (iter, &value);
      pat = g_value_get_boxed (&value);
      _mech_renderer_add_background (renderer, pat);
      break;
    case MECH_PROPERTY_FOREGROUND_PATTERN:
      _style_iterator_unpack (iter, &value);
      pat = g_value_get_boxed (&value);
      _mech_renderer_add_foreground (renderer, pat);
      break;
    default:
      _style_iterator_next (iter);
      break;
    }
}

static MechRenderer *
_style_create_renderer (StylePropertySet *properties)
{
  StyleIterator iter = { properties, 0 };
  MechRenderer *renderer;

  renderer = _mech_renderer_new ();

  while (!_style_iterator_finished (&iter))
    _style_apply_current_property (&iter, renderer);

  return renderer;
}

MechRenderer *
mech_style_lookup_renderer (MechStyle      *style,
                            MechArea       *area,
                            MechStateFlags  state)
{
  gboolean first = TRUE, child_match = FALSE;
  StyleTreeNode *node, *next;
  MechStylePrivate *priv;
  GQuark qname;

  g_return_val_if_fail (MECH_IS_STYLE (style), NULL);
  g_return_val_if_fail (MECH_IS_AREA (area), NULL);

  priv = mech_style_get_instance_private (style);
  node = priv->style_tree;

  /* FIXME: Cache renderers */

  while (area)
    {
      qname = mech_area_get_qname (area);
      next = _find_child_node (node, qname, state, FALSE);

      if (next)
        node = next;
      else if (first)
        child_match = TRUE;

      area = mech_area_get_parent (area);
      first = FALSE;
    }

  _style_tree_node_ensure_resolved (node);

  if (child_match && node->parent)
    {
      MechRenderer *renderer, *copy;

      renderer = _style_create_renderer (node->properties);
      copy = _mech_renderer_copy (renderer, MECH_RENDERER_COPY_FOREGROUND);
      g_object_unref (renderer);

      return copy;
    }
  else
    return _style_create_renderer (node->properties);
}
