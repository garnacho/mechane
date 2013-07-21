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

#include "mech-check-mark.h"
#include "mechane.h"

enum {
  PROP_ACTIVE = 1
};

typedef struct _MechCheckMarkPrivate MechCheckMarkPrivate;

struct _MechCheckMarkPrivate
{
  MechArea *check;
  MechArea *label;
  guint active : 1;
};

G_DEFINE_TYPE_WITH_CODE (MechCheckMark, mech_check_mark, MECH_TYPE_LINEAR_BOX,
                         G_ADD_PRIVATE (MechCheckMark)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_TOGGLE, NULL)
			 G_IMPLEMENT_INTERFACE (MECH_TYPE_TEXT, NULL))

static void
_mech_check_mark_set_active (MechCheckMark *mark,
                             gboolean       active)
{
  MechCheckMarkPrivate *priv;

  priv = mech_check_mark_get_instance_private (mark);
  priv->active = active;

  if (active)
    mech_area_set_state_flags (priv->check, MECH_STATE_FLAG_ACTIVE);
  else
    mech_area_unset_state_flags (priv->check, MECH_STATE_FLAG_ACTIVE);

  mech_area_redraw (MECH_AREA (mark), NULL);
  g_object_notify (G_OBJECT (mark), "active");
}

static void
mech_check_mark_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  MechCheckMarkPrivate *priv;

  priv = mech_check_mark_get_instance_private ((MechCheckMark *) object);

  if (mech_area_get_delegate_property (MECH_AREA (object), pspec, value))
    return;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->active);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_check_mark_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MechCheckMark *mark = (MechCheckMark *) object;

  if (mech_area_set_delegate_property (MECH_AREA (object), pspec, value))
    return;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      _mech_check_mark_set_active (mark, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_check_mark_finalize (GObject *object)
{
  MechCheckMarkPrivate *priv;

  priv = mech_check_mark_get_instance_private ((MechCheckMark *) object);
  g_object_unref (priv->check);
  g_object_unref (priv->label);

  G_OBJECT_CLASS (mech_check_mark_parent_class)->finalize (object);
}

static gboolean
mech_check_mark_handle_event (MechArea  *area,
                              MechEvent *event)
{
  MechCheckMark *mark = (MechCheckMark *) area;
  MechCheckMarkPrivate *priv;

  priv = mech_check_mark_get_instance_private (mark);

  if (mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  switch (event->type)
    {
    case MECH_BUTTON_PRESS:
      _mech_check_mark_set_active (mark, !priv->active);
      break;
    default:
      break;
    }

  return TRUE;
}

static void
mech_check_mark_class_init (MechCheckMarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);

  object_class->get_property = mech_check_mark_get_property;
  object_class->set_property = mech_check_mark_set_property;
  object_class->finalize = mech_check_mark_finalize;

  area_class->handle_event = mech_check_mark_handle_event;

  g_object_class_override_property (object_class,
                                    PROP_ACTIVE,
                                    "active");

  mech_area_class_set_delegate (MECH_AREA_CLASS (klass),
				MECH_TYPE_TEXT,
				G_PRIVATE_OFFSET (MechCheckMark, label));
}

static void
mech_check_mark_init (MechCheckMark *mark)
{
  MechCheckMarkPrivate *priv;

  priv = mech_check_mark_get_instance_private (mark);

  mech_area_add_events (MECH_AREA (mark),
                        MECH_BUTTON_MASK | MECH_CROSSING_MASK);

  priv->check = mech_area_new ("check", MECH_NONE);
  mech_area_add (MECH_AREA (mark), priv->check);
  g_object_ref (priv->check);

  priv->label = mech_text_view_new ();
  mech_area_add (MECH_AREA (mark), priv->label);
  g_object_ref (priv->label);
}

MechArea *
mech_check_mark_new (void)
{
  return g_object_new (MECH_TYPE_CHECK_MARK, NULL);
}
