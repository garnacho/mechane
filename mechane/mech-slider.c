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

#include <mechane/mech-slider.h>
#include <mechane/mech-orientable.h>
#include <mechane/mech-adjustable.h>
#include <math.h>

typedef struct _MechSliderPrivate MechSliderPrivate;

enum {
  PROP_ORIENTATION = 1,
  PROP_MINIMUM_VALUE,
  PROP_MAXIMUM_VALUE,
  PROP_SELECTION_SIZE,
  PROP_VALUE,
  PROP_HANDLE
};

struct _MechSliderPrivate
{
  MechArea *handle;

  gdouble min;
  gdouble max;
  gdouble value;
  gdouble selection_size;

  gdouble handle_x;
  gdouble handle_y;

  guint orientation : 1;
  guint dragging : 1;
};

G_DEFINE_TYPE_WITH_CODE (MechSlider, mech_slider, MECH_TYPE_AREA,
                         G_ADD_PRIVATE (MechSlider)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_ADJUSTABLE, NULL))

static void
mech_slider_init (MechSlider *area)
{
  MechSliderPrivate *priv;

  priv = mech_slider_get_instance_private (area);
  mech_area_set_name (MECH_AREA (area), "slider");
  priv->min = priv->value = 0;
  priv->max = 1;
}

static void
_mech_slider_update (MechSlider *slider)
{
  MechArea *area = (MechArea *) slider;
  cairo_rectangle_t rect, child_rect;
  MechSliderPrivate *priv;
  MechRenderer *renderer;
  gdouble value, minimal;
  MechBorder border;

  priv = mech_slider_get_instance_private (slider);

  if (!priv->handle)
    return;

  if (!mech_area_get_window (area))
    return;

  value = CLAMP (priv->value, priv->min, priv->max) - priv->min;
  mech_area_get_allocated_size (MECH_AREA (slider), &rect);

  renderer = mech_area_get_renderer (area);
  mech_renderer_get_border_extents (renderer, MECH_EXTENT_CONTENT, &border);
  rect.width -= border.left + border.right;
  rect.height -= border.top + border.bottom;

  if (priv->orientation == MECH_ORIENTATION_HORIZONTAL)
    {
      minimal = mech_area_get_extent (area, MECH_AXIS_X);
      child_rect.y = 0;
      child_rect.height = rect.height;

      child_rect.width = (priv->selection_size * rect.width) /
        MAX (priv->max - priv->min, 1);

      if (priv->value < priv->min)
        child_rect.width += (priv->value * rect.width) / (priv->max - priv->min);

      child_rect.width = round (MAX (child_rect.width, minimal));

      if (priv->selection_size < priv->max - priv->min)
        child_rect.x = round ((value * (rect.width - child_rect.width)) /
                              (priv->max - priv->min - priv->selection_size));
      else
        child_rect.x = 0;

      if (priv->value > priv->max - priv->selection_size)
        {
          if (rect.width - child_rect.x < minimal)
            child_rect.x = rect.width - minimal;
          child_rect.width = rect.width - child_rect.x;
        }
    }
  else
    {
      minimal = mech_area_get_extent (area, MECH_AXIS_Y);
      child_rect.x = 0;
      child_rect.width = rect.width;

      child_rect.height = (priv->selection_size * rect.height) /
        MAX (priv->max - priv->min, 1);

      if (priv->value < priv->min)
        child_rect.height += (priv->value * rect.height) / (priv->max - priv->min);

      child_rect.height = round (MAX (child_rect.height, minimal));

      if (priv->selection_size < priv->max - priv->min)
        child_rect.y = round ((value * (rect.height - child_rect.height)) /
                              (priv->max - priv->min - priv->selection_size));
      else
        child_rect.y = 0;

      if (priv->value > priv->max - priv->selection_size)
        {
          if (rect.height - child_rect.y < minimal)
            child_rect.y = rect.height - minimal;
          child_rect.height = rect.height - child_rect.y;
        }
    }

  mech_area_allocate_size (priv->handle, &child_rect);
  mech_area_redraw (area, NULL);
}

static void
mech_slider_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  MechSlider *slider = MECH_SLIDER (object);
  MechSliderPrivate *priv;

  priv = mech_slider_get_instance_private (slider);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      priv->orientation = g_value_get_enum (value);
      break;
    case PROP_MINIMUM_VALUE:
      priv->min = g_value_get_double (value);
      _mech_slider_update (slider);
      break;
    case PROP_MAXIMUM_VALUE:
      priv->max = g_value_get_double (value);
      _mech_slider_update (slider);
      break;
    case PROP_SELECTION_SIZE:
      priv->selection_size = g_value_get_double (value);
      _mech_slider_update (slider);
      break;
    case PROP_VALUE:
      priv->value = g_value_get_double (value);
      _mech_slider_update (slider);
      g_signal_emit_by_name (slider, "value-changed");
      break;
    case PROP_HANDLE:
      priv->handle = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_slider_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  MechSliderPrivate *priv;

  priv = mech_slider_get_instance_private ((MechSlider *) object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_MINIMUM_VALUE:
      g_value_set_double (value, priv->min);
      break;
    case PROP_MAXIMUM_VALUE:
      g_value_set_double (value, priv->max);
      break;
    case PROP_SELECTION_SIZE:
      g_value_set_double (value, priv->selection_size);
      break;
    case PROP_VALUE:
      g_value_set_double (value, priv->value);
      break;
    case PROP_HANDLE:
      g_value_set_object (value, priv->handle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_slider_constructed (GObject *object)
{
  MechSliderPrivate *priv;

  priv = mech_slider_get_instance_private ((MechSlider *) object);

  if (!priv->handle)
    priv->handle = mech_area_new ("handle",
                                  MECH_BUTTON_MASK |
                                  MECH_MOTION_MASK |
                                  MECH_CROSSING_MASK);

  mech_area_set_parent (priv->handle, MECH_AREA (object));

  mech_area_add_events (MECH_AREA (object),
                        MECH_BUTTON_MASK | MECH_MOTION_MASK);
}

static gdouble
mech_slider_get_extent (MechArea *area,
			MechAxis  axis)
{
  MechSliderPrivate *priv;
  gdouble size;

  priv = mech_slider_get_instance_private ((MechSlider *) area);
  size = mech_area_get_extent (priv->handle, axis);

  if ((axis == MECH_AXIS_X &&
       priv->orientation == MECH_ORIENTATION_HORIZONTAL) ||
      (axis == MECH_AXIS_Y &&
       priv->orientation == MECH_ORIENTATION_VERTICAL))
    size *= 2;

  return size;
}

static gdouble
mech_slider_get_second_extent (MechArea *area,
			       MechAxis  axis,
			       gdouble   other_value)
{
  MechSliderPrivate *priv;
  gdouble size;

  priv = mech_slider_get_instance_private ((MechSlider *) area);
  size = mech_area_get_extent (priv->handle, axis);

  if ((axis == MECH_AXIS_X &&
       priv->orientation == MECH_ORIENTATION_HORIZONTAL) ||
      (axis == MECH_AXIS_Y &&
       priv->orientation == MECH_ORIENTATION_VERTICAL))
    size *= 2;

  return size;
}

static void
mech_slider_allocate_size (MechArea *area,
			   gdouble   width,
			   gdouble   height)
{
  _mech_slider_update (MECH_SLIDER (area));
}

static gboolean
mech_slider_handle_event (MechArea  *area,
			  MechEvent *event)
{
  MechSliderPrivate *priv;

  priv = mech_slider_get_instance_private ((MechSlider *) area);

  if (event->any.target != priv->handle)
    return FALSE;

  if (event->type != MECH_MOTION &&
      event->type != MECH_BUTTON_PRESS &&
      event->type != MECH_BUTTON_RELEASE)
    return FALSE;

  switch (event->type)
    {
    case MECH_MOTION:
      if (priv->dragging)
        {
          cairo_rectangle_t rect, parent;
          gdouble pos, value;

          mech_area_get_allocated_size (area, &parent);
          mech_area_get_allocated_size (priv->handle, &rect);

          if (priv->orientation == MECH_ORIENTATION_HORIZONTAL)
            {
              pos = event->pointer.x - priv->handle_x;
              pos = CLAMP (pos, 0, parent.width - rect.width);
              value = (((priv->max - priv->min - priv->selection_size) * pos) /
                       (parent.width - rect.width)) + priv->min;
            }
          else
            {
              pos = event->pointer.y - priv->handle_y;
              pos = CLAMP (pos, 0, parent.height - rect.height);
              value = (((priv->max - priv->min - priv->selection_size) * pos) /
                       (parent.height - rect.height)) + priv->min;
            }

          mech_adjustable_set_value (MECH_ADJUSTABLE (area), value);
        }
      break;
    case MECH_BUTTON_PRESS:
      {
        gdouble handle_x, handle_y;

        handle_x = event->pointer.x;
        handle_y = event->pointer.y;
        mech_area_transform_point (area, priv->handle,
                                   &handle_x, &handle_y);
        priv->handle_x = handle_x;
        priv->handle_y = handle_y;
        priv->dragging = TRUE;
        break;
      }
    case MECH_BUTTON_RELEASE:
      priv->handle_x = priv->handle_y = -1;
      priv->dragging = FALSE;
      break;
    default:
      break;
    }

  return TRUE;
}

static void
mech_slider_class_init (MechSliderClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_slider_set_property;
  object_class->get_property = mech_slider_get_property;
  object_class->constructed = mech_slider_constructed;

  area_class->get_extent = mech_slider_get_extent;
  area_class->get_second_extent = mech_slider_get_second_extent;
  area_class->allocate_size = mech_slider_allocate_size;
  area_class->handle_event = mech_slider_handle_event;

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");
  g_object_class_override_property (object_class,
                                    PROP_MINIMUM_VALUE,
				    "minimum-value");
  g_object_class_override_property (object_class,
                                    PROP_MAXIMUM_VALUE,
				    "maximum-value");
  g_object_class_override_property (object_class,
                                    PROP_VALUE,
				    "value");
  g_object_class_override_property (object_class,
                                    PROP_SELECTION_SIZE,
				    "selection-size");

  g_object_class_install_property (object_class,
                                   PROP_HANDLE,
                                   g_param_spec_object ("handle",
                                                        "Handle",
                                                        "Handle area",
                                                        MECH_TYPE_AREA,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

MechArea *
mech_slider_new (MechOrientation orientation)
{
  return g_object_new (MECH_TYPE_SLIDER,
                       "orientation", orientation,
                       NULL);
}
