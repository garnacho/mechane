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
#include "mech-egl-config.h"

static void mech_surface_wayland_egl_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MechSurfaceWaylandEGL, mech_surface_wayland_egl,
                         MECH_TYPE_SURFACE_WAYLAND,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                mech_surface_wayland_egl_initable_iface_init))

struct _MechSurfaceWaylandEGLPriv
{
  MechEGLConfig *egl_config;
  struct wl_egl_window *wl_egl_window;
  EGLSurface egl_surface;
  cairo_surface_t *surface;
  int cached_age;
  int tx;
  int ty;
};

static gboolean
mech_surface_wayland_egl_initable_init (GInitable     *initable,
                                        GCancellable  *cancellable,
                                        GError       **error)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) initable;
  GError *config_error = NULL;
  MechEGLConfig *config;

  if (cancellable)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "surfaces' initialization can't be cancelled");
      return FALSE;
    }

  config = _mech_egl_config_get (EGL_OPENGL_BIT, &config_error);

  if (config_error)
    {
      g_propagate_error (error, config_error);
      return FALSE;
    }

  surface_egl->_priv->egl_config = config;
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
    eglDestroySurface (priv->egl_config->egl_display,
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

      if (surface_device != priv->egl_config->argb_device ||
          cairo_device_status (surface_device) != CAIRO_STATUS_SUCCESS)
        return FALSE;
    }

  cairo_device_acquire (priv->egl_config->argb_device);

  return cairo_device_status (priv->egl_config->argb_device) == CAIRO_STATUS_SUCCESS;
}

static void
mech_surface_wayland_egl_release (MechSurface *surface)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = surface_egl->_priv;
  cairo_device_t *surface_device;

  if (priv->surface)
    {
      cairo_surface_flush (priv->surface);
      surface_device = cairo_surface_get_device (priv->surface);

      if (surface_device != priv->egl_config->argb_device ||
          cairo_device_status (surface_device) != CAIRO_STATUS_SUCCESS)
        return;
    }

  cairo_device_release (priv->egl_config->argb_device);
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
        eglCreateWindowSurface (priv->egl_config->egl_display,
                                priv->egl_config->egl_argb_config,
                                priv->wl_egl_window, NULL);
      priv->surface =
        cairo_gl_surface_create_for_egl (priv->egl_config->argb_device,
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

      if (priv->egl_config &&
          priv->egl_surface &&
          priv->egl_config->has_buffer_age_ext)
        eglQuerySurface(priv->egl_config->egl_display,
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
mech_surface_wayland_egl_push_update (MechSurface          *surface,
                                      const cairo_region_t *region)
{
  MechSurfaceWaylandEGL *surface_egl = (MechSurfaceWaylandEGL *) surface;
  MechSurfaceWaylandEGLPriv *priv = surface_egl->_priv;

  if (priv->egl_config->has_swap_buffers_with_damage_ext &&
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

      eglSwapBuffersWithDamageEXT (priv->egl_config->egl_display,
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

  MECH_SURFACE_CLASS (mech_surface_wayland_egl_parent_class)->push_update (surface, region);
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
  surface_class->push_update = mech_surface_wayland_egl_push_update;

  surface_wayland_class = (MechSurfaceWaylandClass *) klass;
  surface_wayland_class->translate = mech_surface_wayland_egl_translate;

  g_type_class_add_private (klass, sizeof (MechSurfaceWaylandEGLPriv));
}

static void
mech_surface_wayland_egl_init (MechSurfaceWaylandEGL *surface)
{
  surface->_priv = G_TYPE_INSTANCE_GET_PRIVATE (surface,
                                                MECH_TYPE_SURFACE_WAYLAND_EGL,
                                                MechSurfaceWaylandEGLPriv);
  g_object_set (surface,
                "renderer-type", MECH_RENDERER_TYPE_GL,
                NULL);
}
