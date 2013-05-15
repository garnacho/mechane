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

#include "mech-activatable.h"
#include "mech-toggle.h"
#include "mech-toggle-button.h"

typedef struct _MechToggleButtonPrivate MechToggleButtonPrivate;

enum {
  PROP_ACTIVE = 1
};

struct _MechToggleButtonPrivate
{
  guint active : 1;
};

static void mech_toggle_button_activatable_init (MechActivatableInterface *iface);
static void mech_toggle_button_toggle_init (MechToggleInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MechToggleButton, mech_toggle_button, MECH_TYPE_BUTTON,
                         G_ADD_PRIVATE (MechToggleButton)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_ACTIVATABLE,
                                                mech_toggle_button_activatable_init)
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_TOGGLE,
                                                mech_toggle_button_toggle_init))

static void
_mech_toggle_button_set_active (MechToggleButton *button,
                                gboolean          active)
{
  MechToggleButtonPrivate *priv;

  priv = mech_toggle_button_get_instance_private (button);

  if (priv->active == active)
    return;

  priv->active = active;

  if (active)
    mech_area_set_state_flags (MECH_AREA (button), MECH_STATE_FLAG_ACTIVE);
  else
    mech_area_unset_state_flags (MECH_AREA (button), MECH_STATE_FLAG_ACTIVE);

  g_object_notify (G_OBJECT (button), "active");
  g_signal_emit_by_name (button, "toggled");
}

static void
mech_toggle_button_toggle_init (MechToggleInterface *iface)
{
}

static void
mech_toggle_button_activated (MechActivatable *activatable)
{
  MechToggleButtonPrivate *priv;
  MechToggleButton *button;

  button = (MechToggleButton *) activatable;
  priv = mech_toggle_button_get_instance_private (button);
  _mech_toggle_button_set_active (button, !priv->active);
}

static void
mech_toggle_button_activatable_init (MechActivatableInterface *iface)
{
  iface->activated = mech_toggle_button_activated;
}

static void
mech_toggle_button_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  MechToggleButtonPrivate *priv;

  priv = mech_toggle_button_get_instance_private ((MechToggleButton *) object);

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
mech_toggle_button_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  MechToggleButton *button = (MechToggleButton *) object;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      _mech_toggle_button_set_active (button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_toggle_button_class_init (MechToggleButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mech_toggle_button_get_property;
  object_class->set_property = mech_toggle_button_set_property;

  g_object_class_override_property (object_class,
                                    PROP_ACTIVE,
                                    "active");
}

static void
mech_toggle_button_init (MechToggleButton *button)
{
  mech_area_set_name ((MechArea * ) button, "toggle-button");
}

MechArea *
mech_toggle_button_new (void)
{
  return g_object_new (MECH_TYPE_TOGGLE_BUTTON, NULL);
}
