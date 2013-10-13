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

#define EGL_EGLEXT_PROTOTYPES

#include <string.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cairo-gl.h>
#include "mech-egl-config.h"
#include "mech-backend-wayland.h"

G_DEFINE_QUARK (MechSurfaceEGLError, mech_surface_egl_error)

typedef struct _MechEGLConfigPrivate MechEGLConfigPrivate;

struct _MechEGLConfigPrivate
{
  MechEGLConfig config;
  GError *error;
};

static MechEGLConfigPrivate *
_create_egl_config (gint renderable_type)
{
  EGLint major, minor, n_configs;
  MechBackendWayland *backend;
  MechEGLConfigPrivate *priv;
  const gchar *extensions;
  MechEGLConfig *config;
  GError *error;
  EGLenum api;
  const EGLint argb_config_attributes[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_RENDERABLE_TYPE, renderable_type,
    EGL_NONE
  };

  priv = g_new0 (MechEGLConfigPrivate, 1);
  config = (MechEGLConfig *) priv;

  if (renderable_type != EGL_OPENGL_BIT &&
      renderable_type != EGL_OPENGL_ES_BIT &&
      renderable_type != EGL_OPENGL_ES2_BIT)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Not a valid renderable type");
      goto init_failed;
    }

  backend = _mech_backend_wayland_get ();
  config->egl_display = eglGetDisplay (backend->wl_display);

  if (eglInitialize (config->egl_display, &major, &minor) == EGL_FALSE)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Could not initialize EGL");
      goto init_failed;
    }

  api = (renderable_type == EGL_OPENGL_BIT) ?
    EGL_OPENGL_API : EGL_OPENGL_ES_API;

  if (eglBindAPI (api) == EGL_FALSE)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Could not bind a rendering API");
      goto init_failed;
    }

  if (eglChooseConfig (config->egl_display, argb_config_attributes,
                       &config->egl_argb_config, 1, &n_configs) == EGL_FALSE)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Could not find a matching configuration");
      goto init_failed;
    }

  config->egl_argb_context = eglCreateContext (config->egl_display,
                                               config->egl_argb_config,
                                               EGL_NO_CONTEXT, NULL);

  if (config->egl_argb_context == EGL_NO_CONTEXT)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Could not create an EGL context");
      goto init_failed;
    }

  if (eglMakeCurrent (config->egl_display, EGL_NO_SURFACE,
                      EGL_NO_SURFACE, config->egl_argb_context) == EGL_FALSE)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Could not make EGL context current");
      goto init_failed;
    }

  config->argb_device = cairo_egl_device_create (config->egl_display,
                                                 config->egl_argb_context);

  if (cairo_device_status (config->argb_device) != CAIRO_STATUS_SUCCESS)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Could not create a cairo EGL device");
      goto init_failed;
    }

  extensions = eglQueryString (config->egl_display, EGL_EXTENSIONS);

  if (strstr (extensions, "EGL_EXT_buffer_age"))
    config->has_buffer_age_ext = TRUE;

  if (strstr (extensions, "EGL_EXT_swap_buffers_with_damage"))
    config->has_swap_buffers_with_damage_ext = TRUE;

  return priv;

 init_failed:
  if (config->egl_argb_context != EGL_NO_CONTEXT)
    {
      eglDestroyContext (config->egl_display, config->egl_argb_context);
      config->egl_argb_context = EGL_NO_CONTEXT;
    }

  if (config->argb_device)
    {
      cairo_device_destroy (config->argb_device);
      config->argb_device = NULL;
    }

  priv->error = error;

  return priv;
}

MechEGLConfig *
_mech_egl_config_get (gint     renderable_type,
                      GError **error)
{
  static GHashTable *egl_configs = NULL;
  MechEGLConfigPrivate *egl_config;

  if (G_UNLIKELY (!egl_configs))
    egl_configs = g_hash_table_new (NULL, NULL);

  egl_config = g_hash_table_lookup (egl_configs,
                                    GINT_TO_POINTER (renderable_type));

  if (!egl_config)
    {
      egl_config = _create_egl_config (renderable_type);
      g_hash_table_insert (egl_configs, GINT_TO_POINTER (renderable_type),
                           egl_config);
    }

  if (G_UNLIKELY (egl_config->error))
    {
      if (error)
        *error = g_error_copy (egl_config->error);

      return NULL;
    }

  return (MechEGLConfig *) egl_config;
}
