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
#include "mech-acceleration.h"
#include "mech-marshal.h"

#define SIGN_CHANGED(a,b) (((a) > 0 && (b) <= 0) || ((a) < 0 && (b) >= 0))

typedef struct _MechAccelerationPrivate MechAccelerationPrivate;

struct _MechAccelerationPrivate
{
  gdouble initial_velocity;
  gdouble initial_position;
  gdouble position;
  gdouble acceleration;
  gdouble boundary;
  gint64 start_time;
  gint64 time;
};

enum {
  DISPLACED,
  STOPPED,
  N_SIGNALS
};

enum {
  PROP_INITIAL_POSITION = 1,
  PROP_INITIAL_VELOCITY,
  PROP_ACCELERATION,
  PROP_BOUNDARY,
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechAcceleration, mech_acceleration,
                            MECH_TYPE_ANIMATION)

static void
mech_acceleration_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MechAccelerationPrivate *priv;

  priv = mech_acceleration_get_instance_private ((MechAcceleration *) object);

  switch (prop_id)
    {
    case PROP_INITIAL_POSITION:
      g_value_set_double (value, priv->initial_position);
      break;
    case PROP_INITIAL_VELOCITY:
      g_value_set_double (value, priv->initial_velocity);
      break;
    case PROP_ACCELERATION:
      g_value_set_double (value, priv->acceleration);
      break;
    case PROP_BOUNDARY:
      g_value_set_double (value, priv->boundary);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_acceleration_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MechAccelerationPrivate *priv;

  priv = mech_acceleration_get_instance_private ((MechAcceleration *) object);

  switch (prop_id)
    {
    case PROP_INITIAL_POSITION:
      priv->initial_position = priv->position = g_value_get_double (value);
      break;
    case PROP_INITIAL_VELOCITY:
      priv->initial_velocity = g_value_get_double (value);
      break;
    case PROP_ACCELERATION:
      priv->acceleration = g_value_get_double (value);
      break;
    case PROP_BOUNDARY:
      priv->boundary = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
_mech_acceleration_get_offset (MechAcceleration *acceleration,
                               gint64            _time,
                               gdouble          *offset)
{
  MechAccelerationPrivate *priv;
  gdouble time_diff;

  g_return_val_if_fail (MECH_IS_ACCELERATION (acceleration), FALSE);

  priv = mech_acceleration_get_instance_private (acceleration);

  if (priv->time < 0)
    return FALSE;

  time_diff = ((gdouble) _time - priv->time) / G_USEC_PER_SEC;

  if (offset)
    *offset = priv->position +
      ((priv->acceleration * (time_diff * time_diff)) / 2) +
      (priv->initial_velocity * time_diff);

  return TRUE;
}

static gboolean
_mech_acceleration_get_velocity (MechAcceleration *acceleration,
                                 gint64            _time,
                                 gdouble          *velocity)
{
  MechAccelerationPrivate *priv;
  gdouble time_diff;

  g_return_val_if_fail (MECH_IS_ACCELERATION (acceleration), FALSE);

  priv = mech_acceleration_get_instance_private (acceleration);

  if (priv->time < 0)
    return FALSE;

  time_diff = ((gdouble) _time - priv->time) / G_USEC_PER_SEC;

  if (velocity)
    *velocity = (priv->acceleration * time_diff) + priv->initial_velocity;

  return TRUE;
}

static void
mech_acceleration_begin (MechAnimation *animation,
                         gint64         _time)
{
  MechAccelerationPrivate *priv;

  priv = mech_acceleration_get_instance_private ((MechAcceleration *) animation);
  priv->start_time = priv->time = _time;
}

static void
mech_acceleration_end (MechAnimation *animation,
                       gint64         _time)
{
  MechAccelerationPrivate *priv;

  priv = mech_acceleration_get_instance_private ((MechAcceleration *) animation);
  priv->start_time = priv->time = -1;
}

static gboolean
mech_acceleration_frame (MechAnimation *animation,
                         gint64         _time)
{
  MechAcceleration *acceleration = MECH_ACCELERATION (animation);
  MechAccelerationPrivate *priv;
  gdouble offset = 0, velocity;
  gint64 check_time;

  if (!_mech_acceleration_get_velocity (acceleration, _time, &velocity))
    return FALSE;

  priv = mech_acceleration_get_instance_private (acceleration);

  if (SIGN_CHANGED (priv->initial_velocity, velocity))
    {
      gdouble time_diff;

      /* direction changed, so find out the time when velocity was 0 */
      time_diff = - priv->initial_velocity / priv->acceleration;
      check_time = priv->time + (time_diff * G_USEC_PER_SEC);
      _mech_acceleration_get_offset (acceleration, check_time, &offset);
      g_signal_emit (acceleration, signals[STOPPED], 0, check_time, offset, 0.);
      return FALSE;
    }
  else
    {
      check_time = _time;
      _mech_acceleration_get_offset (acceleration, check_time, &offset);
    }

  if (SIGN_CHANGED (offset - priv->boundary,
                    priv->position - priv->boundary))
    {
      gdouble hit_velocity;
      gint64 hit_time;

      /* Find out velocity when the bound is hit */
      hit_velocity =
        sqrt ((priv->initial_velocity * priv->initial_velocity) +
              (2 * priv->acceleration * (priv->boundary - priv->position)));

      /* Then the time when the bound is hit */
      hit_time = (2 * (priv->boundary - priv->position)) /
        (priv->initial_velocity - hit_velocity);

      g_signal_emit (acceleration, signals[STOPPED], 0,
                     hit_time, priv->boundary, hit_velocity);
      return FALSE;
    }

  g_signal_emit (acceleration, signals[DISPLACED], 0, offset);
  return TRUE;
}

static void
mech_acceleration_class_init (MechAccelerationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechAnimationClass *animation_class = MECH_ANIMATION_CLASS (klass);

  object_class->get_property = mech_acceleration_get_property;
  object_class->set_property = mech_acceleration_set_property;

  animation_class->begin = mech_acceleration_begin;
  animation_class->frame = mech_acceleration_frame;
  animation_class->end = mech_acceleration_end;

  g_object_class_install_property (object_class,
                                   PROP_INITIAL_POSITION,
                                   g_param_spec_double ("initial-position",
                                                        "Initial position",
                                                        "Initial position",
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_INITIAL_VELOCITY,
                                   g_param_spec_double ("initial-velocity",
                                                        "Initial velocity",
                                                        "Initial velocity",
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_ACCELERATION,
                                   g_param_spec_double ("acceleration",
                                                        "Acceleration",
                                                        "Acceleration",
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_BOUNDARY,
                                   g_param_spec_double ("boundary",
                                                        "Boundary",
                                                        "Boundary position",
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  signals[DISPLACED] =
    g_signal_new ("displaced",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAccelerationClass, displaced),
                  NULL, NULL,
                  _mech_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  signals[STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechAccelerationClass, stopped),
                  NULL, NULL,
                  _mech_marshal_VOID__INT64_DOUBLE_DOUBLE,
                  G_TYPE_NONE, 3, G_TYPE_INT64, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}

static void
mech_acceleration_init (MechAcceleration *acceleration)
{
}

MechAnimation *
mech_acceleration_new (gdouble initial_position,
                       gdouble initial_velocity,
                       gdouble acceleration)
{
  return g_object_new (MECH_TYPE_ACCELERATION,
                       "initial-position", initial_position,
                       "initial-velocity", initial_velocity,
                       "acceleration", acceleration,
                       NULL);
}

gdouble
mech_acceleration_get_boundary (MechAcceleration *acceleration)
{
  MechAccelerationPrivate *priv;

  g_return_val_if_fail (MECH_IS_ACCELERATION (acceleration), 0);

  priv = mech_acceleration_get_instance_private (acceleration);
  return priv->boundary;
}

void
mech_acceleration_set_boundary (MechAcceleration *acceleration,
                                gdouble           boundary_pos)
{
  MechAccelerationPrivate *priv;

  g_return_if_fail (MECH_IS_ACCELERATION (acceleration));

  priv = mech_acceleration_get_instance_private (acceleration);
  priv->boundary = boundary_pos;
  g_object_notify ((GObject *) acceleration, "boundary");
}
