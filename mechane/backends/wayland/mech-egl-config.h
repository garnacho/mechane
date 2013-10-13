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

#ifndef __MECH_EGL_CONFIG_H__
#define __MECH_EGL_CONFIG_H__

#include <EGL/egl.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _MechEGLConfig MechEGLConfig;

struct _MechEGLConfig
{
  EGLDisplay egl_display;
  EGLConfig egl_argb_config;
  EGLContext egl_argb_context;
  cairo_device_t *argb_device;

  guint has_swap_buffers_with_damage_ext : 1;
  guint has_buffer_age_ext               : 1;
};

MechEGLConfig * _mech_egl_config_get (gint     renderable_type,
                                      GError **error);

G_END_DECLS

#endif /* __MECH_EGL_CONFIG_H__ */
