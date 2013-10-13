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

#include <string.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cairo-gl.h>
#include "mech-backend-wayland.h"
#include "mech-surface-wayland-texture.h"
#include "mech-egl-config.h"

enum {
  PROP_TEXTURE_ID = 1
};

typedef struct _MechSurfaceWaylandTexturePrivate MechSurfaceWaylandTexturePrivate;

struct _MechSurfaceWaylandTexturePrivate
{
  MechEGLConfig *egl_config;
  cairo_surface_t *surface;
  GLuint texture_id;
  gint width;
  gint height;

  guint initialized : 1;
};

static void mech_surface_wayland_texture_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MechSurfaceWaylandTexture, mech_surface_wayland_texture,
                         MECH_TYPE_SURFACE_WAYLAND,
                         G_ADD_PRIVATE (MechSurfaceWaylandTexture)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                mech_surface_wayland_texture_initable_iface_init))

static gboolean
mech_surface_wayland_texture_initable_init (GInitable     *initable,
                                            GCancellable  *cancellable,
                                            GError       **error)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) initable;
  MechSurfaceWaylandTexturePrivate *priv;
  GError *config_error = NULL;
  MechEGLConfig *config;

  if (cancellable)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "surfaces' initialization can't be cancelled");
      return FALSE;
    }

  priv = mech_surface_wayland_texture_get_instance_private (texture);
  config = _mech_egl_config_get (EGL_OPENGL_BIT, &config_error);

  if (config_error)
    {
      g_propagate_error (error, config_error);
      return FALSE;
    }

  priv->egl_config = config;
  return TRUE;
}

static void
mech_surface_wayland_texture_initable_iface_init (GInitableIface *iface)
{
  iface->init = mech_surface_wayland_texture_initable_init;
}

static void
mech_surface_wayland_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) object;
  MechSurfaceWaylandTexturePrivate *priv;

  priv = mech_surface_wayland_texture_get_instance_private (texture);

  switch (prop_id)
    {
    case PROP_TEXTURE_ID:
      g_value_set_uint (value, priv->texture_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_surface_wayland_texture_finalize (GObject *object)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) object;
  MechSurfaceWaylandTexturePrivate *priv;

  priv = mech_surface_wayland_texture_get_instance_private (texture);

  if (priv->surface)
    cairo_surface_destroy (priv->surface);

  if (priv->texture_id)
    glDeleteTextures (1, &priv->texture_id);

  G_OBJECT_CLASS (mech_surface_wayland_texture_parent_class)->finalize (object);
}

static gboolean
mech_surface_wayland_texture_acquire (MechSurface *surface)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) surface;
  MechSurfaceWaylandTexturePrivate *priv;
  cairo_device_t *surface_device;

  priv = mech_surface_wayland_texture_get_instance_private (texture);

  if (priv->surface)
    {
      surface_device = cairo_surface_get_device (priv->surface);

      if (surface_device != priv->egl_config->argb_device ||
          cairo_device_status (surface_device) != CAIRO_STATUS_SUCCESS)
        return FALSE;
    }

  cairo_device_acquire (priv->egl_config->argb_device);

  return cairo_device_status (priv->egl_config->argb_device) == CAIRO_STATUS_SUCCESS;
}

static void
mech_surface_wayland_texture_release (MechSurface *surface)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) surface;
  MechSurfaceWaylandTexturePrivate *priv;
  cairo_device_t *surface_device;

  priv = mech_surface_wayland_texture_get_instance_private (texture);
  priv->initialized = TRUE;

  if (priv->surface)
    {
      cairo_surface_flush (priv->surface);
      surface_device = cairo_surface_get_device (priv->surface);

      if (surface_device != priv->egl_config->argb_device ||
          cairo_device_status (surface_device) != CAIRO_STATUS_SUCCESS)
        return;
    }

  cairo_device_release (priv->egl_config->argb_device);
}

static cairo_surface_t *
mech_surface_wayland_texture_get_surface (MechSurface *surface)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) surface;
  MechSurfaceWaylandTexturePrivate *priv;

  priv = mech_surface_wayland_texture_get_instance_private (texture);

  return priv->surface;
}

static void
mech_surface_wayland_texture_set_size (MechSurface *surface,
                                       gint         width,
                                       gint         height)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) surface;
  MechSurfaceWaylandTexturePrivate *priv;
  GLenum status;

  priv = mech_surface_wayland_texture_get_instance_private (texture);

  if (priv->width == width && priv->height == height)
    return;

  if (priv->surface)
    cairo_surface_destroy (priv->surface);

  if (priv->texture_id)
    glDeleteTextures (1, &priv->texture_id);

  /* Generate the texture */
  glGenTextures (1, &priv->texture_id);
  glBindTexture (GL_TEXTURE_2D, priv->texture_id);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  priv->surface =
    cairo_gl_surface_create_for_texture (priv->egl_config->argb_device,
                                         CAIRO_CONTENT_COLOR_ALPHA,
                                         priv->texture_id,
                                         width, height);
  priv->width = width;
  priv->height = height;
  priv->initialized = FALSE;
}

static gint
mech_surface_wayland_texture_get_age (MechSurface *surface)
{
  MechSurfaceWaylandTexture *texture = (MechSurfaceWaylandTexture *) surface;
  MechSurfaceWaylandTexturePrivate *priv;

  priv = mech_surface_wayland_texture_get_instance_private (texture);
  return priv->initialized ? 1 : 0;
}

static void
mech_surface_wayland_texture_class_init (MechSurfaceWaylandTextureClass *klass)
{
  MechSurfaceWaylandClass *surface_wayland_class;
  MechSurfaceClass *surface_class;
  GObjectClass *object_class;

  object_class = (GObjectClass *) klass;
  object_class->get_property = mech_surface_wayland_get_property;
  object_class->finalize = mech_surface_wayland_texture_finalize;

  surface_class = (MechSurfaceClass *) klass;
  surface_class->acquire = mech_surface_wayland_texture_acquire;
  surface_class->release = mech_surface_wayland_texture_release;
  surface_class->get_surface = mech_surface_wayland_texture_get_surface;
  surface_class->set_size = mech_surface_wayland_texture_set_size;
  surface_class->get_age = mech_surface_wayland_texture_get_age;

  g_object_class_install_property (object_class,
                                   PROP_TEXTURE_ID,
                                   g_param_spec_uint ("texture-id",
                                                      "Texture ID",
                                                      "Texture ID",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
mech_surface_wayland_texture_init (MechSurfaceWaylandTexture *surface)
{
  g_object_set (surface,
                "renderer-type", MECH_RENDERER_TYPE_GL,
                NULL);
}
