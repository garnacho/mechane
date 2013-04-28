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

#include <mechane/mech-monitor.h>
#include <mechane/mech-window-private.h>

typedef struct _MechMonitorPrivate MechMonitorPrivate;

enum {
  PROP_NAME = 1,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_WIDTH_MM,
  PROP_HEIGHT_MM,
  PROP_FREQUENCY
};

struct _MechMonitorPrivate
{
  gchar *name;
  guint16 width;
  guint16 height;
  guint16 width_mm;
  guint16 height_mm;
  guint16 frequency;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechMonitor, mech_monitor, G_TYPE_OBJECT)

static void
mech_monitor_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  MechMonitorPrivate *priv;

  priv = mech_monitor_get_instance_private ((MechMonitor *) object);

  switch (prop_id)
    {
    case PROP_NAME:
      if (priv->name)
        g_free (priv->name);
      priv->name = g_value_dup_string (value);
      break;
    case PROP_WIDTH:
      priv->width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      priv->height = g_value_get_int (value);
      break;
    case PROP_WIDTH_MM:
      priv->width_mm = g_value_get_int (value);
      break;
    case PROP_HEIGHT_MM:
      priv->height_mm = g_value_get_int (value);
      break;
    case PROP_FREQUENCY:
      priv->frequency = g_value_get_int (value);
      break;
    }
}

static void
mech_monitor_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MechMonitorPrivate *priv;

  priv = mech_monitor_get_instance_private ((MechMonitor *) object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, priv->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, priv->height);
      break;
    case PROP_WIDTH_MM:
      g_value_set_int (value, priv->width_mm);
      break;
    case PROP_HEIGHT_MM:
      g_value_set_int (value, priv->height_mm);
      break;
    case PROP_FREQUENCY:
      g_value_set_int (value, priv->frequency);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_monitor_class_init (MechMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mech_monitor_get_property;
  object_class->set_property = mech_monitor_set_property;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Device name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
                                                     "Width",
                                                     "Width",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_int ("height",
                                                     "Height",
                                                     "Height",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_WIDTH_MM,
                                   g_param_spec_int ("width-mm",
                                                     "Width in millimeters",
                                                     "Width in millimeters",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_HEIGHT_MM,
                                   g_param_spec_int ("height-mm",
                                                     "Height in millimeters",
                                                     "Height in millimeters",
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_FREQUENCY,
                                   g_param_spec_int ("frequency",
                                                     "Frequency",
                                                     "Frequency in hertzs",
                                                     0, G_MAXINT, 60,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));
}

static void
mech_monitor_init (MechMonitor *monitor)
{
}

void
_mech_monitor_set_mode (MechMonitor *monitor,
                        gint         width,
                        gint         height,
                        gint         frequency)
{
  MechMonitorPrivate *priv;

  priv = mech_monitor_get_instance_private (monitor);
  g_object_freeze_notify ((GObject *) monitor);

  if (width != priv->width)
    {
      priv->width = width;
      g_object_notify ((GObject *) monitor, "width");
    }

  if (height != priv->height)
    {
      priv->height = height;
      g_object_notify ((GObject *) monitor, "height");
    }

  if (frequency != priv->frequency)
    {
      priv->frequency = frequency;
      g_object_notify ((GObject *) monitor, "frequency");
    }

  g_object_thaw_notify ((GObject *) monitor);
}

void
_mech_monitor_set_physical_size (MechMonitor *monitor,
                                 gint         width,
                                 gint         height)
{
  MechMonitorPrivate *priv;

  priv = mech_monitor_get_instance_private (monitor);
  g_object_freeze_notify ((GObject *) monitor);

  if (width != priv->width_mm)
    {
      priv->width_mm = width;
      g_object_notify ((GObject *) monitor, "width-mm");
    }

  if (height != priv->height_mm)
    {
      priv->height_mm = height;
      g_object_notify ((GObject *) monitor, "height-mm");
    }

  g_object_thaw_notify ((GObject *) monitor);
}

void
mech_monitor_get_extents (MechMonitor *monitor,
                          gint        *width,
                          gint        *height)
{
  MechMonitorPrivate *priv;

  g_return_if_fail (MECH_IS_MONITOR (monitor));

  priv = mech_monitor_get_instance_private (monitor);

  if (width)
    *width = priv->width;

  if (height)
    *height = priv->height;
}

void
mech_monitor_get_mm_extents (MechMonitor *monitor,
                             gint        *width_mm,
                             gint        *height_mm)
{
  MechMonitorPrivate *priv;

  g_return_if_fail (MECH_IS_MONITOR (monitor));

  priv = mech_monitor_get_instance_private (monitor);

  if (width_mm)
    *width_mm = priv->width_mm;

  if (height_mm)
    *height_mm = priv->height_mm;
}

gint
mech_monitor_get_frequency (MechMonitor *monitor)
{
  MechMonitorPrivate *priv;

  g_return_val_if_fail (MECH_IS_MONITOR (monitor), 1);

  priv = mech_monitor_get_instance_private (monitor);
  return priv->frequency;
}
