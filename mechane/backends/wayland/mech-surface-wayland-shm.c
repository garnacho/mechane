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

#define N_BUFFERS 2

G_DEFINE_TYPE (MechSurfaceWaylandSHM, mech_surface_wayland_shm,
               MECH_TYPE_SURFACE_WAYLAND)

typedef struct _BufferData BufferData;

struct _BufferData
{
  struct wl_shm_pool *wl_pool;
  struct wl_buffer *wl_buffer;
  cairo_surface_t *surface;
  gpointer data;
  gsize data_len;
  guint blank    : 1;
  guint released : 1;
  guint disposed : 1;
};

struct _MechSurfaceWaylandSHMPriv
{
  BufferData *buffers[N_BUFFERS];
  gint cur_buffer;
  gint prev_buffer;
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
_destroy_buffer (BufferData *buffer)
{
  buffer->disposed = TRUE;

  /* If the buffer is acquired by the compositor, don't free it
   * yet, but wait until it's been released.
   */
  if (!buffer->released)
    return;

  if (buffer->surface)
    cairo_surface_destroy (buffer->surface);

  if (buffer->wl_buffer)
    wl_buffer_destroy (buffer->wl_buffer);

  if (buffer->wl_pool)
    wl_shm_pool_destroy (buffer->wl_pool);

  if (buffer->data && buffer->data_len)
    munmap (buffer->data, buffer->data_len);

  g_free (buffer);
}

static void
_buffer_release (gpointer          data,
                 struct wl_buffer *wl_buffer)
{
  BufferData *buffer = data;

  buffer->released = TRUE;

  if (buffer->disposed)
    _destroy_buffer (buffer);
}

static struct wl_buffer_listener buffer_listener_funcs = {
  _buffer_release
};

static BufferData *
_create_buffer (gint width,
                gint height)
{
  MechBackendWayland *backend;
  BufferData *buffer;
  gint stride, fd;

  buffer = g_new0 (BufferData, 1);

  backend = _mech_backend_wayland_get ();
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
  buffer->data_len = stride * height;

  fd = _create_temporary_file (buffer->data_len);
  buffer->data = mmap (NULL, buffer->data_len,
                     PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd, 0);

  if (buffer->data == MAP_FAILED)
    {
      g_critical ("Failed to mmap SHM region: %m");
      g_free (buffer);
      close(fd);
      return;
    }

  buffer->wl_pool = wl_shm_create_pool (backend->wl_shm, fd, buffer->data_len);
  close (fd);

  buffer->wl_buffer = wl_shm_pool_create_buffer (buffer->wl_pool, 0,
                                                 width, height, stride,
                                                 WL_SHM_FORMAT_ARGB8888);
  wl_buffer_add_listener (buffer->wl_buffer, &buffer_listener_funcs, buffer);

  buffer->surface = cairo_image_surface_create_for_data (buffer->data,
                                                         CAIRO_FORMAT_ARGB32,
                                                         width, height, stride);
  buffer->blank = TRUE;
  buffer->released = TRUE;

  return buffer;
}

BufferData *
_create_buffer_similar (BufferData *buffer)
{
  return _create_buffer (cairo_image_surface_get_width (buffer->surface),
                         cairo_image_surface_get_height (buffer->surface));
}

static void
_surface_ensure_buffers (MechSurfaceWaylandSHM *surface_shm,
                         gint                   width,
                         gint                   height)
{
  MechSurfaceWaylandSHMPriv *priv = surface_shm->_priv;
  gint i;

  for (i = 0; i < N_BUFFERS; i++)
    {
      if (priv->buffers[i])
        _destroy_buffer (priv->buffers[i]);

      priv->buffers[i] = _create_buffer (width, height);
    }
}

static void
mech_surface_wayland_shm_finalize (GObject *object)
{
  MechSurfaceWaylandSHM *surface_shm;
  MechSurfaceWaylandSHMPriv *priv;
  gint i;

  surface_shm = (MechSurfaceWaylandSHM *) object;
  priv = surface_shm->_priv;

  for (i = 0; i < N_BUFFERS; i++)
    {
      if (priv->buffers[i])
        _destroy_buffer (priv->buffers[i]);
    }

  G_OBJECT_CLASS (mech_surface_wayland_shm_parent_class)->finalize (object);
}

static void
mech_surface_wayland_shm_set_size (MechSurface *surface,
                                   gint         width,
                                   gint         height)
{
  MechSurfaceWaylandSHM *surface_shm;

  surface_shm = (MechSurfaceWaylandSHM *) surface;
  _surface_ensure_buffers (surface_shm, width, height);
}

static cairo_surface_t *
mech_surface_wayland_shm_get_surface (MechSurface *surface)
{
  MechSurfaceWaylandSHM *surface_shm;
  gint cur_buffer, prev_buffer = 0;
  MechSurfaceWaylandSHMPriv *priv;
  BufferData *buffer;

  surface_shm = (MechSurfaceWaylandSHM *) surface;
  priv = surface_shm->_priv;

  if (priv->prev_buffer >= 0)
    prev_buffer = priv->prev_buffer;

  cur_buffer = prev_buffer;
  buffer = priv->buffers[cur_buffer];

  if (!buffer->released)
    {
      /* Pick the next buffer if the current
       * one wasn't released yet.
       */
      cur_buffer = (cur_buffer + 1) % N_BUFFERS;
      buffer = priv->buffers[cur_buffer];

      if (!buffer->released)
        {
          /* If the second buffer is still acquired by the compositor,
           * create a replacement buffer and dispose the original one,
           * it will be truly freed after being released by the
           * compositor.
           */
          priv->buffers[cur_buffer] = _create_buffer_similar (buffer);
          _destroy_buffer (buffer);
          buffer = priv->buffers[cur_buffer];
        }
    }

  priv->cur_buffer = cur_buffer;

  return buffer->surface;
}

static void
mech_surface_wayland_shm_release (MechSurface *surface)
{
  MechSurfaceWaylandSHM *surface_shm;
  MechSurfaceWaylandSHMPriv *priv;

  surface_shm = (MechSurfaceWaylandSHM *) surface;
  priv = surface_shm->_priv;

  if (priv->cur_buffer >= 0)
    {
      /* The buffer has been rendered now for sure */
      priv->buffers[priv->cur_buffer]->blank = FALSE;
    }

  /* Store the now previous buffer, unset the current
   * one so it is chosen again at the next render cycle.
   */
  priv->prev_buffer = priv->cur_buffer;
  priv->cur_buffer = -1;
}

static gint
mech_surface_wayland_shm_get_age (MechSurface *surface)
{
  MechSurfaceWaylandSHM *surface_shm;
  MechSurfaceWaylandSHMPriv *priv;
  BufferData *buffer;

  surface_shm = (MechSurfaceWaylandSHM *) surface;
  priv = surface_shm->_priv;
  buffer = priv->buffers[priv->cur_buffer];

  if (buffer->blank)
    return 0;
  else if (priv->cur_buffer == priv->prev_buffer)
    return 1;
  else
    return 2;
}

static void
mech_surface_wayland_shm_push_update (MechSurface          *surface,
                                      const cairo_region_t *region)
{
  MechSurfaceWaylandSHM *shm_surface = (MechSurfaceWaylandSHM *) surface;
  MechSurfaceWaylandSHMPriv *priv = shm_surface->_priv;
  struct wl_surface *wl_surface;
  cairo_rectangle_int_t rect;
  BufferData *buffer;
  gint i;

  g_object_get (surface, "wl-surface", &wl_surface, NULL);
  buffer = priv->buffers[priv->cur_buffer];

  if (wl_surface)
    {
      for (i = 0; i < cairo_region_num_rectangles (region); i++)
        {
          cairo_region_get_rectangle (region, i, &rect);
          wl_surface_damage (wl_surface, rect.x, rect.y,
                             rect.width, rect.height);
        }

      wl_surface_attach (wl_surface, buffer->wl_buffer, priv->tx, priv->ty);
      priv->tx = priv->ty = 0;

      /* The current buffer is now acquired by the compositor */
      buffer->released = FALSE;
    }

  MECH_SURFACE_CLASS (mech_surface_wayland_shm_parent_class)->push_update (surface, region);
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
  surface_class->release = mech_surface_wayland_shm_release;
  surface_class->get_age = mech_surface_wayland_shm_get_age;
  surface_class->push_update = mech_surface_wayland_shm_push_update;

  surface_wayland_class = MECH_SURFACE_WAYLAND_CLASS (klass);
  surface_wayland_class->translate = mech_surface_wayland_shm_translate;

  g_type_class_add_private (klass, sizeof (MechSurfaceWaylandSHMPriv));
}

static void
mech_surface_wayland_shm_init (MechSurfaceWaylandSHM *surface)
{
  surface->_priv = G_TYPE_INSTANCE_GET_PRIVATE (surface,
                                                MECH_TYPE_SURFACE_WAYLAND_SHM,
                                                MechSurfaceWaylandSHMPriv);
  surface->_priv->cur_buffer = -1;
  surface->_priv->prev_buffer = -1;

  g_object_set (surface,
                "renderer-type", MECH_RENDERER_TYPE_SOFTWARE,
                NULL);
}
