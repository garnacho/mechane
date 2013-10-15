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

#include <cairo/cairo-gobject.h>
#include <GL/gl.h>

#include <mechane/mech-marshal.h>
#include <mechane/mech-gl-view.h>
#include <mechane/mech-gl-container-private.h>
#include <mechane/mech-area-private.h>

enum {
  RENDER_SCENE,
  PICK_CHILD,
  CHILD_COORDS,
  N_SIGNALS
};

typedef struct _MechGLViewPrivate MechGLViewPrivate;
typedef struct _GLStatus GLStatus;

struct _GLStatus
{
  GLuint prev_program;
  GLuint prev_framebuffer;
  GLuint prev_renderbuffer;
};

struct _MechGLViewPrivate
{
  GHashTable *children;
  MechArea *focus_child;
  MechArea *pointer_child;
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechGLView, mech_gl_view, MECH_TYPE_AREA)

static void
mech_gl_view_finalize (GObject *object)
{
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private ((MechGLView *) object);
  g_hash_table_unref (priv->children);
  G_OBJECT_CLASS (mech_gl_view_parent_class)->finalize (object);
}

static void
mech_gl_view_dispose (GObject *object)
{
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private ((MechGLView *) object);
  g_hash_table_remove_all (priv->children);

  G_OBJECT_CLASS (mech_gl_view_parent_class)->dispose (object);
}

static void
_mech_gl_view_update_children (MechGLView *view)
{
  MechGLViewPrivate *priv;
  GHashTableIter iter;
  gpointer value;

  priv = mech_gl_view_get_instance_private (view);
  g_hash_table_iter_init (&iter, priv->children);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      gint width, height;

      /* Fetch texture to ensure a surface is set */
      _mech_gl_container_get_texture_id (value);
      mech_container_get_size (value, &width, &height);
      mech_container_queue_resize (value, width, height);
      mech_container_process_updates (value);
    }
}

/* XXX: Hack to cope with cairo not leaving "clean" GL settings */
static void
_gl_status_save (GLStatus *status)
{
  glLoadIdentity();
  glDisable (GL_STENCIL_TEST);
  glDisable (GL_BLEND);
  glDepthFunc (GL_LESS);
  glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glLogicOp (GL_COPY);
  glDepthRange (0, 1);
  glDepthMask (GL_TRUE);
  glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc (GL_ALWAYS, 0, (GLuint) -1);
  glClearStencil (0);

  glGetIntegerv (GL_FRAMEBUFFER_BINDING, &status->prev_framebuffer);
  glBindFramebuffer (GL_FRAMEBUFFER, 0);

  glGetIntegerv (GL_RENDERBUFFER_BINDING, &status->prev_renderbuffer);
  glBindRenderbuffer (GL_RENDERBUFFER, 0);

  glGetIntegerv (GL_CURRENT_PROGRAM, &status->prev_program);
}

static void
_gl_status_restore (GLStatus *status,
                    MechArea *area)
{
  cairo_rectangle_t allocation;

  glUseProgram (status->prev_program);
  glBindFramebuffer (GL_FRAMEBUFFER, status->prev_framebuffer);
  glBindRenderbuffer (GL_RENDERBUFFER, status->prev_renderbuffer);

  mech_area_get_allocated_size (area, &allocation);
  glViewport (0, 0, allocation.width, allocation.height);
  glScissor (0, 0, allocation.width, allocation.height);
}

static void
mech_gl_view_draw (MechArea *area,
                   cairo_t  *cr)
{
  cairo_rectangle_t allocation;
  GLStatus gl_status = { 0 };
  cairo_surface_t *surface;
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private ((MechGLView *) area);
  surface = cairo_get_target (cr);
  mech_area_get_allocated_size (area, &allocation);

  if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_GL)
    return;

  _mech_gl_view_update_children ((MechGLView *) area);

  _gl_status_save (&gl_status);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glViewport (0, 0, allocation.width, allocation.height);
  glScissor (0, 0, allocation.width, allocation.height);

  g_signal_emit (area, signals[RENDER_SCENE], 0,
                 cairo_surface_get_device (surface));

  glFlush();
  _gl_status_restore (&gl_status, area);
  cairo_surface_mark_dirty (surface);
}

static void
_mech_gl_view_leave_pointer_child (MechGLView *view)
{
  MechContainer *container;
  MechGLViewPrivate *priv;
  MechEvent *event;

  priv = mech_gl_view_get_instance_private (view);

  if (!priv->pointer_child)
    return;

  container = g_hash_table_lookup (priv->children, priv->pointer_child);
  event = mech_event_new (MECH_LEAVE);
  mech_event_set_area (event, priv->pointer_child);
  mech_container_handle_event (container, event);
  mech_event_free (event);
  priv->pointer_child = NULL;
}

static void
_mech_gl_view_update_pointer_child (MechGLView *view,
                                    MechArea   *child,
                                    MechEvent  *event)
{
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private (view);

  if (priv->pointer_child == child)
    return;

  _mech_gl_view_leave_pointer_child (view);
  priv->pointer_child = child;
}

static gboolean
mech_gl_view_handle_event (MechArea  *area,
                           MechEvent *event)
{
  MechContainer *container = NULL;
  MechGLViewPrivate *priv;
  MechArea *child = NULL;
  gdouble x, y;

  if (event->any.target != area ||
      mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  priv = mech_gl_view_get_instance_private ((MechGLView *) area);

  if (mech_event_pointer_get_coords (event, &x, &y))
    {
      GLStatus gl_status = { 0 };
      gdouble child_x, child_y;
      MechSurface *surface;
      MechStage *stage;

      stage = _mech_area_get_stage (area);

      /* FIXME: we shouldn't even need to check for this in the event handler */
      if (!stage)
        return FALSE;

      surface = _mech_stage_get_rendering_surface (stage, area);
      _mech_surface_acquire (surface);
      _gl_status_save (&gl_status);

      g_signal_emit (area, signals[PICK_CHILD], 0,
                     x, y, &child_x, &child_y, &child);

      _gl_status_restore (&gl_status, area);
      _mech_surface_release (surface);

      _mech_gl_view_update_pointer_child ((MechGLView *) area, child, event);

      if (!child)
        return FALSE;

      event->pointer.x = child_x;
      event->pointer.y = child_y;
    }
  else if (event->type == MECH_FOCUS_IN ||
           event->type == MECH_FOCUS_OUT ||
           event->type == MECH_KEY_PRESS ||
           event->type == MECH_KEY_RELEASE)
    {
      /* Keyboard events go to priv->focus_child */
      child = priv->focus_child;

      if (event->type == MECH_FOCUS_OUT)
        priv->focus_child = NULL;
    }
  else if (event->type == MECH_LEAVE)
    _mech_gl_view_leave_pointer_child ((MechGLView *) area);

  if (child)
    container = g_hash_table_lookup (priv->children, child);

  if (!container)
    return FALSE;

  mech_event_set_area (event, child);

  return mech_container_handle_event (container, event);
}

static void
_child_container_grab_focus (MechContainer *container,
                             MechArea      *area,
                             MechSeat      *seat,
                             MechArea      *gl_view)
{
  MechArea *focus_child = NULL;
  MechGLViewPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;

  priv = mech_gl_view_get_instance_private ((MechGLView *) gl_view);
  mech_area_grab_focus (gl_view, seat);

  g_hash_table_iter_init (&iter, priv->children);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (value && value == container)
        focus_child = key;
    }

  priv->focus_child = focus_child;
}

static void
_child_container_update_notify (MechContainer *container,
                                MechArea      *gl_view)
{
  mech_area_redraw (gl_view, NULL);
}

static void
mech_gl_view_add (MechArea *area,
                  MechArea *child)
{
  MechContainer *container;
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private ((MechGLView *) area);

  container = MECH_CONTAINER (_mech_gl_container_new (area));
  mech_area_add (mech_container_get_root (container), child);
  g_signal_connect (container, "grab-focus",
                    G_CALLBACK (_child_container_grab_focus), area);
  g_signal_connect (container, "update-notify",
                    G_CALLBACK (_child_container_update_notify), area);
  g_hash_table_insert (priv->children, child, container);

  mech_area_redraw (area, NULL);
}

static void
mech_gl_view_remove (MechArea *area,
                     MechArea *child)
{
  MechContainer *container;
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private ((MechGLView *) area);
  container = g_hash_table_lookup (priv->children, child);

  if (!container)
    return;

  mech_area_remove (mech_container_get_root (container), child);
  g_hash_table_remove (priv->children, child);
}

static void
mech_gl_view_class_init (MechGLViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);

  object_class->finalize = mech_gl_view_finalize;
  object_class->dispose = mech_gl_view_dispose;

  area_class->draw = mech_gl_view_draw;
  area_class->handle_event = mech_gl_view_handle_event;
  area_class->add = mech_gl_view_add;
  area_class->remove = mech_gl_view_remove;

  signals[RENDER_SCENE] =
    g_signal_new ("render-scene",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechGLViewClass, render_scene),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CAIRO_GOBJECT_TYPE_DEVICE | G_SIGNAL_TYPE_STATIC_SCOPE);
  signals[PICK_CHILD] =
    g_signal_new ("pick-child",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechGLViewClass, pick_child),
                  NULL, NULL,
                  _mech_marshal_OBJECT__DOUBLE_DOUBLE_POINTER_POINTER,
                  MECH_TYPE_AREA, 4,
                  G_TYPE_DOUBLE, G_TYPE_DOUBLE,
                  G_TYPE_POINTER, G_TYPE_POINTER);
  signals[CHILD_COORDS] =
    g_signal_new ("child-coords",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechGLViewClass, child_coords),
                  NULL, NULL,
                  _mech_marshal_VOID__OBJECT_DOUBLE_DOUBLE_POINTER_POINTER,
                  G_TYPE_NONE, 5,
                  MECH_TYPE_AREA, G_TYPE_DOUBLE, G_TYPE_DOUBLE,
                  G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
mech_gl_view_init (MechGLView *view)
{
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private ((MechGLView *) view);
  priv->children = g_hash_table_new_full (NULL, NULL, NULL,
                                          (GDestroyNotify) g_object_unref);

  mech_area_set_surface_type (MECH_AREA (view), MECH_SURFACE_TYPE_GL);
}

MechArea *
mech_gl_view_new (void)
{
  return g_object_new (MECH_TYPE_GL_VIEW, NULL);
}

gboolean
mech_gl_view_bind_child_texture (MechGLView *view,
                                 MechArea   *child)
{
  MechGLContainer *container;
  MechGLViewPrivate *priv;
  guint texture_id;

  g_return_val_if_fail (MECH_IS_GL_VIEW (view), FALSE);
  g_return_val_if_fail (MECH_IS_AREA (view), FALSE);

  priv = mech_gl_view_get_instance_private (view);
  container = g_hash_table_lookup (priv->children, child);

  if (!container)
    return FALSE;

  texture_id = _mech_gl_container_get_texture_id (container);

  if (texture_id == 0)
    return FALSE;

  glBindTexture (GL_TEXTURE_2D, texture_id);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  return TRUE;
}