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

#include <sys/mman.h>
#include <wayland-client.h>
#include <glib/gstdio.h>
#include <cairo/cairo-gobject.h>
#include "mech-backend-wayland.h"
#include "mech-surface-wayland-shm.h"

G_DEFINE_TYPE (MechSurfaceWaylandSHM, mech_surface_wayland_shm,
               MECH_TYPE_SURFACE_WAYLAND)

struct _MechSurfaceWaylandSHMPriv
{
  struct wl_shm_pool *wl_pool;
  struct wl_buffer *wl_buffer;
  cairo_surface_t *surface;
  gpointer data;
  gsize data_len;
  gint tx;
  gint ty;
};

static gint
_create_temporary_file (gint size)
{
  gchar *path;
  gint fd;

  path = g_build_filename (g_get_tmp_dir (), "mechane-buffer-XXXXXX", NULL);
  fd = g_mkstemp (path);
  g_unlink (path);
  g_free (path);

  if (ftruncate (fd, size) < 0)
    {
      g_critical ("Truncating temporary file failed: %m");
      close(fd);
      return -1;
    }

  return fd;
}

static void
_surface_unset_surface (MechSurfaceWaylandSHM *surface)
{
  MechSurfaceWaylandSHMPriv *priv = surface->_priv;

  if (priv->surface)
    {
      cairo_surface_destroy (priv->surface);
      priv->surface = NULL;
    }
  if (priv->wl_buffer)
    {
      wl_buffer_destroy (priv->wl_buffer);
      priv->wl_buffer = NULL;
    }
  if (priv->wl_pool)
    {
      wl_shm_pool_destroy (priv->wl_pool);
      priv->wl_pool = NULL;
    }

  if (priv->data && priv->data_len)
    munmap (priv->data, priv->data_len);
}

static void
_surface_ensure_surface (MechSurfaceWaylandSHM *surface,
                         gint                   width,
                         gint                   height)
{
  MechSurfaceWaylandSHMPriv *priv = surface->_priv;
  MechBackendWayland *backend;
  gint stride, fd;

  _surface_unset_surface (surface);

  backend = _mech_backend_wayland_get ();
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
  priv->data_len = stride * height;

  fd = _create_temporary_file (priv->data_len);
  priv->data = mmap (NULL, priv->data_len,
                     PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd, 0);

  if (priv->data == MAP_FAILED)
    {
      g_critical ("Failed to mmap SHM region: %m");
      close(fd);
      return;
    }

  priv->wl_pool = wl_shm_create_pool (backend->wl_shm, fd, priv->data_len);
  close(fd);

  priv->wl_buffer = wl_shm_pool_create_buffer (priv->wl_pool, 0,
                                               width, height, stride,
                                               WL_SHM_FORMAT_ARGB8888);
  priv->surface = cairo_image_surface_create_for_data (priv->data,
                                                       CAIRO_FORMAT_ARGB32,
                                                       width, height, stride);
}

static void
mech_surface_wayland_shm_finalize (GObject *object)
{
  _surface_unset_surface ((MechSurfaceWaylandSHM *) object);

  G_OBJECT_CLASS (mech_surface_wayland_shm_parent_class)->finalize (object);
}

static void
mech_surface_wayland_shm_set_size (MechSurface *surface,
                                   gint         width,
                                   gint         height)
{
  MechSurfaceWaylandSHM *surface_shm;

  surface_shm = (MechSurfaceWaylandSHM *) surface;
  _surface_ensure_surface (surface_shm, width, height);
}

static cairo_surface_t *
mech_surface_wayland_shm_get_surface (MechSurface *surface)
{
  MechSurfaceWaylandSHM *surface_shm;

  surface_shm = (MechSurfaceWaylandSHM *) surface;
  return surface_shm->_priv->surface;
}

static void
mech_surface_wayland_shm_translate (MechSurfaceWayland *surface,
                                    gint                tx,
                                    gint                ty)
{
  MechSurfaceWaylandSHM *shm_surface = (MechSurfaceWaylandSHM *) surface;
  MechSurfaceWaylandSHMPriv *priv = shm_surface->_priv;

  priv->tx += tx;
  priv->ty += ty;
}

static gboolean
mech_surface_wayland_shm_attach (MechSurfaceWayland *surface)
{
  MechSurfaceWaylandSHM *shm_surface = (MechSurfaceWaylandSHM *) surface;
  MechSurfaceWaylandSHMPriv *priv = shm_surface->_priv;
  struct wl_surface *wl_surface;

  if (!priv->wl_buffer)
    return FALSE;

  g_object_get (surface, "wl-surface", &wl_surface, NULL);
  wl_surface_attach (wl_surface, priv->wl_buffer, priv->tx, priv->ty);
  priv->tx = priv->ty = 0;

  return TRUE;
}

static void
mech_surface_wayland_shm_damage (MechSurfaceWayland   *surface,
                                 const cairo_region_t *region)
{
  struct wl_surface *wl_surface;
  cairo_rectangle_int_t rect;
  gint i;

  g_object_get (surface, "wl-surface", &wl_surface, NULL);

  for (i = 0; i < cairo_region_num_rectangles (region); i++)
    {
      cairo_region_get_rectangle (region, i, &rect);
      wl_surface_damage (wl_surface, rect.x, rect.y,
                         rect.width, rect.height);
    }
}

static void
mech_surface_wayland_shm_class_init (MechSurfaceWaylandSHMClass *klass)
{
  MechSurfaceWaylandClass *surface_wayland_class;
  MechSurfaceClass *surface_class;
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = mech_surface_wayland_shm_finalize;

  surface_class = MECH_SURFACE_CLASS (klass);
  surface_class->set_size = mech_surface_wayland_shm_set_size;
  surface_class->get_surface = mech_surface_wayland_shm_get_surface;

  surface_wayland_class = MECH_SURFACE_WAYLAND_CLASS (klass);
  surface_wayland_class->translate = mech_surface_wayland_shm_translate;
  surface_wayland_class->attach = mech_surface_wayland_shm_attach;
  surface_wayland_class->damage = mech_surface_wayland_shm_damage;

  g_type_class_add_private (klass, sizeof (MechSurfaceWaylandSHMPriv));
}

static void
mech_surface_wayland_shm_init (MechSurfaceWaylandSHM *surface)
{
  surface->_priv = G_TYPE_INSTANCE_GET_PRIVATE (surface,
                                                MECH_TYPE_SURFACE_WAYLAND_SHM,
                                                MechSurfaceWaylandSHMPriv);
}
