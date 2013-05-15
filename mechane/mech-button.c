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

#include <mechane/mech-button.h>
#include <mechane/mech-activatable.h>

static void mech_button_activatable_init (MechActivatableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MechButton, mech_button, MECH_TYPE_AREA,
                         G_IMPLEMENT_INTERFACE (MECH_TYPE_ACTIVATABLE,
                                                mech_button_activatable_init))

static gboolean
mech_button_handle_event (MechArea  *area,
                          MechEvent *event)
{
  MECH_AREA_CLASS (mech_button_parent_class)->handle_event (area, event);

  if (event->any.target != area ||
      mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  if (event->type == MECH_BUTTON_PRESS)
    return TRUE;
  else if (event->type == MECH_BUTTON_RELEASE)
    {
      cairo_rectangle_t rect;

      mech_area_get_allocated_size (area, &rect);

      if (event->pointer.x >= 0 && event->pointer.x < rect.width &&
          event->pointer.y >= 0 && event->pointer.y < rect.height)
        mech_activatable_activate (MECH_ACTIVATABLE (area));

      return TRUE;
    }

  return FALSE;
}

static void
mech_button_constructed (GObject *object)
{
  mech_area_add_events ((MechArea *) object,
                        MECH_BUTTON_MASK |
                        MECH_CROSSING_MASK);
}

static void
mech_button_init (MechButton *button)
{
  mech_area_set_name ((MechArea * ) button, "button");
}

static void
mech_button_class_init (MechButtonClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = mech_button_constructed;

  area_class->handle_event = mech_button_handle_event;
}

static void
mech_button_activatable_init (MechActivatableInterface *iface)
{
}

MechArea *
mech_button_new (void)
{
  return g_object_new (MECH_TYPE_BUTTON, NULL);
}
