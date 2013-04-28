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

#include "mech-monitor-layout.h"
#include "mech-types.h"

typedef struct _MechMonitorLayoutPrivate MechMonitorLayoutPrivate;

enum {
  MONITOR_ADDED,
  MONITOR_REMOVED,
  LAST_SIGNAL
};

struct _MechMonitorLayoutPrivate
{
  GHashTable *monitors;
  MechMonitor *primary;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechMonitorLayout, mech_monitor_layout,
                            G_TYPE_OBJECT)

static void
mech_monitor_layout_finalize (GObject *object)
{
  MechMonitorLayoutPrivate *priv;

  priv = mech_monitor_layout_get_instance_private ((MechMonitorLayout *) object);
  g_hash_table_unref (priv->monitors);

  G_OBJECT_CLASS (mech_monitor_layout_parent_class)->finalize (object);
}

static void
mech_monitor_layout_class_init (MechMonitorLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_monitor_layout_finalize;

  signals[MONITOR_ADDED] =
    g_signal_new ("monitor-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechMonitorLayoutClass, monitor_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, MECH_TYPE_MONITOR);
  signals[MONITOR_REMOVED] =
    g_signal_new ("monitor-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechMonitorLayoutClass, monitor_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, MECH_TYPE_MONITOR);
}

static void
mech_monitor_layout_init (MechMonitorLayout *layout)
{
  MechMonitorLayoutPrivate *priv;

  priv = mech_monitor_layout_get_instance_private (layout);
  priv->monitors = g_hash_table_new_full (NULL, NULL, NULL,
                                          (GDestroyNotify) g_free);
}

MechMonitorLayout *
mech_monitor_layout_get (void)
{
  static MechMonitorLayout *layout = NULL;

  if (g_once_init_enter (&layout))
    {
      MechMonitorLayout *object;

      object = g_object_new (MECH_TYPE_MONITOR_LAYOUT, NULL);
      g_once_init_leave (&layout, object);
    }

  return layout;
}

MechMonitor *
mech_monitor_layout_get_primary (MechMonitorLayout *layout)
{
  MechMonitorLayoutPrivate *priv;

  g_return_val_if_fail (MECH_IS_MONITOR_LAYOUT (layout), NULL);

  priv = mech_monitor_layout_get_instance_private (layout);
  return priv->primary;
}

GList *
mech_monitor_layout_list (MechMonitorLayout *layout)
{
  MechMonitorLayoutPrivate *priv;

  g_return_val_if_fail (MECH_IS_MONITOR_LAYOUT (layout), NULL);

  priv = mech_monitor_layout_get_instance_private (layout);
  return g_hash_table_get_keys (priv->monitors);
}

void
_mech_monitor_set_primary (MechMonitorLayout *layout,
                           MechMonitor       *monitor)
{
  MechMonitorLayoutPrivate *priv;

  priv = mech_monitor_layout_get_instance_private (layout);

  if (g_hash_table_contains (priv->monitors, monitor))
    priv->primary = monitor;
}

void
_mech_monitor_layout_add (MechMonitorLayout *layout,
                          MechMonitor       *monitor)
{
  MechMonitorLayoutPrivate *priv;

  priv = mech_monitor_layout_get_instance_private (layout);
  g_hash_table_insert (priv->monitors, monitor, NULL);

  if (!priv->primary)
    priv->primary = monitor;

  g_signal_emit (layout, signals[MONITOR_ADDED], 0, monitor);
}

void
_mech_monitor_layout_remove (MechMonitorLayout *layout,
                             MechMonitor       *monitor)
{
  MechMonitorLayoutPrivate *priv;

  priv = mech_monitor_layout_get_instance_private (layout);

  if (g_hash_table_remove (priv->monitors, monitor))
    g_signal_emit (layout, signals[MONITOR_REMOVED], 0, monitor);
}

void
_mech_monitor_layout_relocate (MechMonitorLayout *layout,
                               MechMonitor       *monitor,
                               gint               x,
                               gint               y)
{
  MechMonitorLayoutPrivate *priv;
  MechPoint *point;
  gpointer value;

  priv = mech_monitor_layout_get_instance_private (layout);

  if (!g_hash_table_lookup_extended (priv->monitors, monitor, NULL, &value))
    return;

  if (value)
    point = value;
  else
    {
      point = g_new0 (MechPoint, 1);
      g_hash_table_insert (priv->monitors, monitor, point);
    }

  point->x = x;
  point->y = y;
}
