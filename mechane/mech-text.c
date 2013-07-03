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

#include <mechane/mech-text.h>

G_DEFINE_INTERFACE (MechText, mech_text, MECH_TYPE_AREA)

static void
mech_text_default_init (MechTextInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("buffer",
                                                            "Buffer",
                                                            "Text buffer",
                                                            MECH_TYPE_TEXT_BUFFER,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
}

void
mech_text_set_buffer (MechText       *text,
                      MechTextBuffer *buffer)
{
  g_return_if_fail (MECH_IS_TEXT (text));
  g_return_if_fail (!buffer || MECH_IS_TEXT_BUFFER (buffer));

  g_object_set (text, "buffer", buffer, NULL);
}

MechTextBuffer *
mech_text_get_buffer (MechText *text)
{
  MechTextBuffer *buffer;

  g_return_val_if_fail (MECH_IS_TEXT (text), NULL);

  g_object_get (text, "buffer", &buffer, NULL);

  return buffer;
}

void
mech_text_set_string (MechText    *text,
                      const gchar *str)
{
  MechTextBuffer *buffer = NULL;

  g_return_if_fail (MECH_IS_TEXT (text));

  if (str)
    {
      buffer = mech_text_buffer_new ();
      mech_text_buffer_set_text (buffer, str, -1);
    }

  mech_text_set_buffer (text, buffer);

  if (buffer)
    g_object_unref (buffer);
}

gchar *
mech_text_get_string (MechText *text)
{
  MechTextBuffer *buffer;

  g_return_val_if_fail (MECH_IS_TEXT (text), NULL);

  buffer = mech_text_get_buffer (text);

  if (!buffer)
    return NULL;

  return mech_text_buffer_get_text (buffer, NULL, NULL);
}
