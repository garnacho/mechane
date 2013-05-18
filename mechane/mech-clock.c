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

#include <mechane/mech-animation-private.h>
#include <mechane/mech-clock-private.h>
#include <mechane/mech-window-private.h>
#include <glib.h>

#define MS_FOR_HZ(hz) (1000 / (hz))

typedef struct _MechClockPrivate MechClockPrivate;

enum {
  PROP_WINDOW = 1
};

struct _MechClockPrivate
{
  MechWindow *window;
  GList *animations;
  GList *dispatched;
  guint running            : 1;
  guint dispatched_deleted : 1;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechClock, _mech_clock, G_TYPE_OBJECT)

static gboolean
mech_clock_dispatch_impl (MechClock *clock)
{
  MechClockPrivate *priv;

  priv = _mech_clock_get_instance_private (clock);
  priv->dispatched = priv->animations;

  while (priv->dispatched)
    {
      MechAnimation *animation = priv->dispatched->data;
      GList *next = priv->dispatched->next;

      priv->dispatched_deleted = FALSE;

      if (!_mech_animation_tick (animation) && !priv->dispatched_deleted)
        {
          next = priv->dispatched->next;
          priv->animations = g_list_delete_link (priv->animations,
                                                 priv->dispatched);
        }

      priv->dispatched = next;
    }

  _mech_window_process_updates (priv->window);

  return priv->animations != NULL;
}

static void
mech_clock_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  MechClockPrivate *priv;

  priv = _mech_clock_get_instance_private ((MechClock *) object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      priv->window = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_clock_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  MechClockPrivate *priv;

  priv = _mech_clock_get_instance_private ((MechClock *) object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      g_value_set_object (value, priv->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_clock_finalize (GObject *object)
{
  MechClockPrivate *priv;

  priv = _mech_clock_get_instance_private ((MechClock *) object);
  g_list_free (priv->animations);

  G_OBJECT_CLASS (_mech_clock_parent_class)->finalize (object);
}

static void
_mech_clock_class_init (MechClockClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->dispatch = mech_clock_dispatch_impl;

  object_class->set_property = mech_clock_set_property;
  object_class->get_property = mech_clock_get_property;
  object_class->finalize = mech_clock_finalize;

  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_object ("window",
                                                        "Window",
                                                        "The window signaling "
                                                        "updates through this clock",
                                                        MECH_TYPE_WINDOW,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
_mech_clock_init (MechClock *clock)
{
}

void
_mech_clock_set_running (MechClock *clock,
                         gboolean   running)
{
  MechClockPrivate *priv;

  priv = _mech_clock_get_instance_private (clock);

  if (priv->running && !running)
    {
      MECH_CLOCK_GET_CLASS (clock)->stop (clock);
      priv->running = FALSE;
    }
  else if (!priv->running && running)
    {
      priv->running = TRUE;
      MECH_CLOCK_GET_CLASS (clock)->start (clock);
    }
}

void
_mech_clock_attach_animation (MechClock     *clock,
                              MechAnimation *animation)
{
  MechClockPrivate *priv;

  g_return_if_fail (MECH_IS_CLOCK (clock));
  g_return_if_fail (MECH_IS_ANIMATION (animation));

  priv = _mech_clock_get_instance_private (clock);
  _mech_clock_set_running (clock, TRUE);
  priv->animations = g_list_append (priv->animations, animation);
}

gboolean
_mech_clock_detach_animation (MechClock     *clock,
                              MechAnimation *animation)
{
  MechClockPrivate *priv;
  GList *list;

  g_return_val_if_fail (MECH_IS_CLOCK (clock), FALSE);
  g_return_val_if_fail (MECH_IS_ANIMATION (animation), FALSE);

  priv = _mech_clock_get_instance_private (clock);
  list = g_list_find (priv->animations, animation);

  if (!list)
    return FALSE;

  if (list == priv->dispatched)
    priv->dispatched_deleted = TRUE;

  priv->animations = g_list_delete_link (priv->animations, list);
  return TRUE;
}

gboolean
_mech_clock_dispatch (MechClock *clock)
{
  MechClockClass *clock_class;

  g_return_val_if_fail (MECH_IS_CLOCK (clock), FALSE);

  clock_class = MECH_CLOCK_GET_CLASS (clock);

  if (!clock_class->dispatch (clock))
    {
      _mech_clock_set_running (clock, FALSE);
      return FALSE;
    }

  return TRUE;
}

void
_mech_clock_tick (MechClock *clock)
{
  _mech_clock_set_running (clock, TRUE);
}

gint64
_mech_clock_get_time (MechClock *clock)
{
  return MECH_CLOCK_GET_CLASS (clock)->get_time (clock);
}
