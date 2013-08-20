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
#include "mech-backend-wayland.h"
#include "mech-surface-wayland-egl.h"

static void mech_surface_wayland_egl_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MechSurfaceWaylandEGL, mech_surface_wayland_egl,
                         MECH_TYPE_SURFACE_WAYLAND,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                mech_surface_wayland_egl_initable_iface_init))

G_DEFINE_QUARK (MechSurfaceEGLError, mech_surface_egl_error)

typedef struct _MechGLConfig MechGLConfig;

struct _MechGLConfig
{
  EGLDisplay egl_display;
  EGLConfig egl_argb_config;
  EGLContext egl_argb_context;
  cairo_device_t *argb_device;
  GError *error;

  guint has_swap_buffers_with_damage_ext : 1;
  guint has_buffer_age_ext               : 1;
};

struct _MechSurfaceWaylandEGLPriv
{
  MechGLConfig *gl_config;
  struct wl_egl_window *wl_egl_window;
  EGLSurface egl_surface;
  cairo_surface_t *surface;
  int cached_age;
  int tx;
  int ty;
};

MechGLConfig *
_create_gl_config (void)
{
  EGLint major, minor, n_configs;
  MechBackendWayland *backend;
  const gchar *extensions;
  MechGLConfig *config;
  GError *error;
  static const EGLint argb_config_attributes[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_NONE
  };

  config = g_new0 (MechGLConfig, 1);
  backend = _mech_backend_wayland_get ();
  config->egl_display = eglGetDisplay (backend->wl_display);

  if (eglInitialize (config->egl_display, &major, &minor) == EGL_FALSE)
    {
      error = g_error_new (mech_surface_egl_error_quark (), 0,
                           "Could not initialize EGL");
      goto init_failed;
    }

  if (eglBindAPI (EGL_OPENGL_API) == EGL_FALSE)
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

  return config;

 init_failed:
  if (config->egl_argb_context != EGL_NO_CONTEXT)
    eglDestroyContext (config->egl_display, config->egl_argb_context);

  if (config->argb_device)
    cairo_device_destroy (config->argb_device);

  config->error = error;

  return config;
}

static MechGLConfig *
_get_gl_config (void)
{
  static MechGLConfig *gl_config = NULL;

  if (g_once_init_enter (&gl_config))
    {
      MechGLConfig *config;

      config = _create_gl_config ();
      g_once_init_leave (&gl_config, config);
    }

  return gl_config;
}

static gboolean
mech_surface_wayland_egl_initable_init (GInitable     *initable,
                                        GCancellable  *cancellable,
                                        GError       **error)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) initable;
  MechGLConfig *config;

  if (cancellable)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "surfaces' initialization can't be cancelled");
      return FALSE;
    }

  config = _get_gl_config ();

  if (config->error)
    {
      if (error)
        *error = g_error_copy (config->error);
      return FALSE;
    }

  surface_egl->_priv->gl_config = config;
  return TRUE;
}

static void
mech_surface_wayland_egl_initable_iface_init (GInitableIface *iface)
{
  iface->init = mech_surface_wayland_egl_initable_init;
}

static void
mech_surface_wayland_egl_finalize (GObject *object)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) object;
  MechSurfaceWaylandEGLPriv *priv = surface_egl->_priv;

  if (priv->surface)
    cairo_surface_destroy (priv->surface);

  if (priv->wl_egl_window)
    wl_egl_window_destroy (priv->wl_egl_window);

  if (priv->egl_surface)
    eglDestroySurface (priv->gl_config->egl_display,
                       priv->egl_surface);

  G_OBJECT_CLASS (mech_surface_wayland_egl_parent_class)->finalize (object);
}

static gboolean
mech_surface_wayland_egl_acquire (MechSurface *surface)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = surface_egl->_priv;
  cairo_device_t *surface_device;

  if (priv->surface)
    {
      surface_device = cairo_surface_get_device (priv->surface);

      if (surface_device != priv->gl_config->argb_device ||
          cairo_device_status (surface_device) != CAIRO_STATUS_SUCCESS)
        return FALSE;
    }

  cairo_device_acquire (priv->gl_config->argb_device);

  return cairo_device_status (priv->gl_config->argb_device) == CAIRO_STATUS_SUCCESS;
}

static void
mech_surface_wayland_egl_release (MechSurface *surface)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = surface_egl->_priv;
  cairo_device_t *surface_device;

  if (priv->surface)
    {
      surface_device = cairo_surface_get_device (priv->surface);

      if (surface_device != priv->gl_config->argb_device ||
          cairo_device_status (surface_device) != CAIRO_STATUS_SUCCESS)
        return;
    }

  cairo_device_release (priv->gl_config->argb_device);
  priv->cached_age = -1;
}

static cairo_surface_t *
mech_surface_wayland_egl_get_surface (MechSurface *surface)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) surface;

  return surface_egl->_priv->surface;
}

static void
mech_surface_wayland_egl_set_size (MechSurface *surface,
                                   gint         width,
                                   gint         height)
{
  MechSurfaceWaylandEGL *egl_surface = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = egl_surface->_priv;

  if (!priv->wl_egl_window)
    {
      struct wl_surface *wl_surface;

      g_object_get (surface, "wl-surface", &wl_surface, NULL);
      priv->wl_egl_window = wl_egl_window_create (wl_surface, width, height);

      priv->egl_surface =
        eglCreateWindowSurface (priv->gl_config->egl_display,
                                priv->gl_config->egl_argb_config,
                                priv->wl_egl_window, NULL);
      priv->surface =
        cairo_gl_surface_create_for_egl (priv->gl_config->argb_device,
                                         priv->egl_surface,
                                         width, height);
    }
  else
    {
      cairo_gl_surface_set_size (priv->surface, width, height);
      wl_egl_window_resize (priv->wl_egl_window, width, height,
                            priv->tx, priv->ty);
      priv->tx = priv->ty = 0;
    }
}

static gint
mech_surface_wayland_egl_get_age (MechSurface *surface)
{
  MechSurfaceWaylandEGL *egl_surface = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = egl_surface->_priv;

  if (priv->cached_age < 0)
    {
      EGLint buffer_age = 0;

      if (priv->gl_config &&
          priv->egl_surface &&
          priv->gl_config->has_buffer_age_ext)
        eglQuerySurface(priv->gl_config->egl_display,
                        priv->egl_surface,
                        EGL_BUFFER_AGE_EXT, &buffer_age);

      priv->cached_age = (gint) buffer_age;
    }

  return priv->cached_age;
}

static void
mech_surface_wayland_egl_translate (MechSurfaceWayland *surface,
                                    gint                tx,
                                    gint                ty)
{
  MechSurfaceWaylandEGL *egl_surface = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = egl_surface->_priv;

  priv->tx += tx;
  priv->ty += ty;
}

static void
mech_surface_wayland_egl_damage (MechSurfaceWayland   *surface,
                                 const cairo_region_t *region)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = surface_egl->_priv;

  if (priv->gl_config->has_swap_buffers_with_damage_ext &&
      region && !cairo_region_is_empty (region))
    {
      cairo_rectangle_int_t region_rect;
      gint n_rects, i;
      EGLint *rects;

      n_rects = cairo_region_num_rectangles (region);
      rects = g_new0 (EGLint, n_rects * 4);

      for (i = 0; i < n_rects; i++)
        {
          cairo_region_get_rectangle (region, i, &region_rect);
          rects[i * 4] = region_rect.x;
          rects[i * 4 + 1] = cairo_gl_surface_get_height (priv->surface) -
            (region_rect.y + region_rect.height);
          rects[i * 4 + 2] = region_rect.width;
          rects[i * 4 + 3] = region_rect.height;
        }

      eglSwapBuffersWithDamageEXT (priv->gl_config->egl_display,
                                   priv->egl_surface, rects, n_rects);

      for (i = 0; i < n_rects; i++)
        {
          cairo_region_get_rectangle (region, i, &region_rect);
          cairo_surface_mark_dirty_rectangle (priv->surface,
                                              region_rect.x, region_rect.y,
                                              region_rect.width,
                                              region_rect.height);
        }

      g_free (rects);
    }
  else
    cairo_gl_surface_swapbuffers (priv->surface);
}

static void
mech_surface_wayland_egl_class_init (MechSurfaceWaylandEGLClass *klass)
{
  MechSurfaceWaylandClass *surface_wayland_class;
  MechSurfaceClass *surface_class;
  GObjectClass *object_class;

  object_class = (GObjectClass *) klass;
  object_class->finalize = mech_surface_wayland_egl_finalize;

  surface_class = (MechSurfaceClass *) klass;
  surface_class->acquire = mech_surface_wayland_egl_acquire;
  surface_class->release = mech_surface_wayland_egl_release;
  surface_class->get_surface = mech_surface_wayland_egl_get_surface;
  surface_class->set_size = mech_surface_wayland_egl_set_size;
  surface_class->get_age = mech_surface_wayland_egl_get_age;

  surface_wayland_class = (MechSurfaceWaylandClass *) klass;
  surface_wayland_class->translate = mech_surface_wayland_egl_translate;
  surface_wayland_class->damage = mech_surface_wayland_egl_damage;

  g_type_class_add_private (klass, sizeof (MechSurfaceWaylandEGLPriv));
}

static void
mech_surface_wayland_egl_init (MechSurfaceWaylandEGL *surface)
{
  surface->_priv = G_TYPE_INSTANCE_GET_PRIVATE (surface,
                                                MECH_TYPE_SURFACE_WAYLAND_EGL,
                                                MechSurfaceWaylandEGLPriv);
}
