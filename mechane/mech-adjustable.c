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

#include <mechane/mech-adjustable.h>
#include <mechane/mechane.h>

enum {
  VALUE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE (MechAdjustable, mech_adjustable, MECH_TYPE_AREA)

static void
mech_adjustable_default_init (MechAdjustableInterface *iface)
{
  signals[VALUE_CHANGED] =
    g_signal_new ("value-changed",
                  MECH_TYPE_ADJUSTABLE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MechAdjustableInterface, value_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_interface_install_property (iface,
				       g_param_spec_double ("minimum-value",
                                                            "Minimum value",
                                                            "Minimum value",
                                                            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface,
				       g_param_spec_double ("maximum-value",
                                                            "Maximum value",
                                                            "Maximum value",
                                                            -G_MAXDOUBLE, G_MAXDOUBLE, 1,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface,
				       g_param_spec_double ("value",
                                                            "Value",
                                                            "Value",
                                                            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface,
				       g_param_spec_double ("selection-size",
                                                            "Selection size",
                                                            "Selection size",
                                                            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));
}

void
mech_adjustable_set_bounds (MechAdjustable *adjustable,
                            gdouble         min,
                            gdouble         max)
{
  g_return_if_fail (MECH_IS_ADJUSTABLE (adjustable));
  g_return_if_fail (max >= min);

  g_object_set (adjustable,
                "minimum-value", min,
                "maximum-value", max,
                NULL);
}

void
mech_adjustable_get_bounds (MechAdjustable *adjustable,
                            gdouble        *min,
                            gdouble        *max)
{
  gdouble min_value, max_value;

  g_return_if_fail (MECH_IS_ADJUSTABLE (adjustable));

  g_object_get (adjustable,
                "minimum-value", &min_value,
                "maximum-value", &max_value,
                NULL);
  if (min)
    *min = min_value;

  if (max)
    *max = max_value;
}

void
mech_adjustable_set_value (MechAdjustable *adjustable,
                           gdouble         value)
{
  g_return_if_fail (MECH_IS_ADJUSTABLE (adjustable));

  g_object_set (adjustable, "value", value, NULL);
}

gdouble
mech_adjustable_get_value (MechAdjustable *adjustable)
{
  gdouble value;

  g_return_val_if_fail (MECH_IS_ADJUSTABLE (adjustable), 0);

  g_object_get (adjustable, "value", &value, NULL);

  return value;
}

void
mech_adjustable_set_selection_size (MechAdjustable *adjustable,
                                    gdouble         size)
{
  g_return_if_fail (MECH_IS_ADJUSTABLE (adjustable));

  g_object_set (adjustable, "selection-size", size, NULL);
}

gdouble
mech_adjustable_get_selection_size (MechAdjustable *adjustable)
{
  gdouble size;

  g_return_val_if_fail (MECH_IS_ADJUSTABLE (adjustable), 0);

  g_object_get (adjustable, "selection-size", &size, NULL);

  return size;
}
