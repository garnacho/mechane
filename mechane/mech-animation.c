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

#include "mech-animation.h"
#include "mech-window-private.h"
#include "mech-clock-private.h"
#include "mech-marshal.h"

enum {
  BEGIN,
  FRAME,
  END,
  N_SIGNALS
};

typedef struct _MechAnimationPrivate MechAnimationPrivate;

struct _MechAnimationPrivate
{
  MechClock *clock;
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechAnimation, mech_animation,
                                     G_TYPE_OBJECT)

static void
_mech_animation_set_clock (MechAnimation *animation,
                           MechClock     *clock)
{
  MechAnimationPrivate *priv;

  priv = mech_animation_get_instance_private (animation);

  if (priv->clock)
    {
      _mech_clock_detach_animation (priv->clock, animation);
      priv->clock = NULL;
    }

  if (clock)
    {
      priv->clock = clock;
      _mech_clock_attach_animation (priv->clock, animation);
    }
}

static void
mech_animation_finalize (GObject *object)
{
  MechAnimationPrivate *priv;
  MechAnimation *animation;

  animation = (MechAnimation *) object;
  priv = mech_animation_get_instance_private (animation);

  /* Ensure it's stopped, we miss the ::end signal this way though... */
  if (priv->clock)
    _mech_animation_set_clock (animation, NULL);

  G_OBJECT_CLASS (mech_animation_parent_class)->finalize (object);
}

static void
mech_animation_class_init (MechAnimationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_animation_finalize;

  signals[BEGIN] =
    g_signal_new ("begin",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAnimationClass, begin),
                  NULL, NULL,
                  _mech_marshal_VOID__INT64,
                  G_TYPE_NONE, 1, G_TYPE_INT64);
  signals[FRAME] =
    g_signal_new ("frame",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAnimationClass, frame),
                  g_signal_accumulator_true_handled, NULL,
                  _mech_marshal_BOOLEAN__INT64,
                  G_TYPE_BOOLEAN, 1, G_TYPE_INT64);
  signals[END] =
    g_signal_new ("end",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAnimationClass, end),
                  NULL, NULL,
                  _mech_marshal_VOID__INT64,
                  G_TYPE_NONE, 1, G_TYPE_INT64);
}

static void
mech_animation_init (MechAnimation *animation)
{
}

gboolean
_mech_animation_tick (MechAnimation *animation)
{
  MechAnimationPrivate *priv;
  gboolean cont;
  gint64 _time;

  priv = mech_animation_get_instance_private (animation);

  if (!priv->clock)
    return FALSE;

  g_object_ref (animation);
  _time = _mech_clock_get_time (priv->clock);
  g_signal_emit (animation, signals[FRAME], 0, _time, &cont);

  if (!cont)
    {
      g_signal_emit (animation, signals[END], 0, _time);
      priv->clock = NULL;
    }

  g_object_unref (animation);

  return cont;
}

void
mech_animation_run (MechAnimation *animation,
                    MechWindow    *window)
{
  MechAnimationPrivate *priv;
  MechClock *clock;

  g_return_if_fail (MECH_IS_ANIMATION (animation));
  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_animation_get_instance_private (animation);
  clock = _mech_window_get_clock (window);

  if (priv->clock == clock)
    return;
  else if (priv->clock)
    {
      g_warning ("The animation is currently running on another window");
      return;
    }

  _mech_animation_set_clock (animation, clock);
  g_signal_emit (animation, signals[BEGIN], 0, _mech_clock_get_time (clock));
}

void
mech_animation_stop (MechAnimation *animation)
{
  MechAnimationPrivate *priv;

  g_return_if_fail (MECH_IS_ANIMATION (animation));

  priv = mech_animation_get_instance_private (animation);

  if (priv->clock)
    {
      g_signal_emit (animation, signals[END], 0,
                     _mech_clock_get_time (priv->clock));
      _mech_animation_set_clock (animation, NULL);
    }
}

gboolean
mech_animation_is_running (MechAnimation *animation)
{
  MechAnimationPrivate *priv;

  g_return_val_if_fail (MECH_IS_ANIMATION (animation), FALSE);

  priv = mech_animation_get_instance_private (animation);
  return priv->clock != NULL;
}
