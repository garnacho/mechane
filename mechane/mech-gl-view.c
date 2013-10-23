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
  MechArea *grab_child;
  GHashTable *touch_children;
  GLuint fbo;
  GLuint depth_rbo;
  GLuint texture_id;
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechGLView, mech_gl_view, MECH_TYPE_AREA)

static void
_mech_gl_view_unset_fbo (MechGLView *view)
{
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private (view);
  glDeleteRenderbuffers (1, &priv->depth_rbo);
  glDeleteFramebuffers (1, &priv->fbo);
  priv->fbo = 0;
}

static void
mech_gl_view_finalize (GObject *object)
{
  MechGLViewPrivate *priv;

  priv = mech_gl_view_get_instance_private ((MechGLView *) object);

  _mech_gl_view_unset_fbo ((MechGLView *) object);
  g_hash_table_unref (priv->children);
  g_hash_table_unref (priv->touch_children);
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

static guint
_mech_gl_view_get_texture_id (MechGLView *view)
{
  MechArea *area = (MechArea *) view;
  MechSurface *surface;
  guint texture_id = 0;
  MechStage *stage;

  stage = _mech_area_get_stage (area);

  if (!stage)
    return 0;

  surface = _mech_stage_get_rendering_surface (stage, area);

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (surface),
                                    "texture-id"))
    g_object_get (surface, "texture-id", &texture_id, NULL);

  return texture_id;
}

static void
_mech_gl_view_check_update_fbo (MechGLView *view)
{
  MechSurfaceType surface_type;
  MechGLViewPrivate *priv;
  guint texture_id = 0;
  MechArea *area;

  area = (MechArea *) view;
  priv = mech_gl_view_get_instance_private (view);
  surface_type = mech_area_get_surface_type (area);
  texture_id = _mech_gl_view_get_texture_id (view);

  if ((!priv->fbo || priv->texture_id != texture_id) &&
      surface_type == MECH_SURFACE_TYPE_OFFSCREEN && texture_id)
    {
      cairo_rectangle_t allocation;
      GLuint status;

      if (priv->fbo)
        _mech_gl_view_unset_fbo (view);

      mech_area_get_allocated_size (area, &allocation);

      glGenRenderbuffers (1, &priv->depth_rbo);
      glBindRenderbuffer (GL_RENDERBUFFER, priv->depth_rbo);
      glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                             (GLsizei) allocation.width,
                             (GLsizei) allocation.height);
      glBindRenderbuffer (GL_RENDERBUFFER, 0);

      glGenFramebuffers (1, &priv->fbo);
      glBindFramebuffer (GL_FRAMEBUFFER, priv->fbo);
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, texture_id, 0);
      glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                 GL_RENDERBUFFER, priv->depth_rbo);
      status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
      glBindFramebuffer (GL_FRAMEBUFFER, 0);

      if (status == GL_FRAMEBUFFER_COMPLETE)
        priv->texture_id = texture_id;
      else
        {
          g_warning ("Could not create FBO, status: %x", status);
          _mech_gl_view_unset_fbo (view);
        }
    }
  else if (priv->fbo && surface_type != MECH_SURFACE_TYPE_OFFSCREEN)
    _mech_gl_view_unset_fbo (view);
}

/* XXX: Hack to cope with cairo not leaving "clean" GL settings */
static void
_gl_status_reset_defaults (void)
{
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity();
  glBlendFunc (GL_ONE, GL_ZERO);
  glEnable (GL_DITHER);
  glEnable (GL_MULTISAMPLE);
  glDisable (GL_STENCIL_TEST);
  glDisable (GL_SCISSOR_TEST);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_DEPTH_CLAMP);
  glDisable (GL_BLEND);
  glDisable (GL_NORMALIZE);
  glEnable (GL_DITHER);
  glDisable (GL_CLIP_DISTANCE0);
  glDepthMask (GL_TRUE);
  glDepthFunc (GL_LESS);
  glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glLogicOp (GL_COPY);
  glDepthRange (0, 1);
  glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc (GL_ALWAYS, 0, (GLuint) -1);
  glClearStencil (0);
  glBindTexture (GL_TEXTURE_2D, 0);
  glBindTexture (GL_TEXTURE_RECTANGLE, 0);

  glBindFramebuffer (GL_FRAMEBUFFER, 0);
  glBindRenderbuffer (GL_RENDERBUFFER, 0);
  glUseProgram (0);
}

static void
_gl_status_save (GLStatus *status)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);

  glGetIntegerv (GL_FRAMEBUFFER_BINDING, &status->prev_framebuffer);
  glGetIntegerv (GL_RENDERBUFFER_BINDING, &status->prev_renderbuffer);
  glGetIntegerv (GL_CURRENT_PROGRAM, &status->prev_program);

  _gl_status_reset_defaults ();
}

static void
_gl_status_restore (GLStatus *status)
{
  glUseProgram (status->prev_program);
  glBindFramebuffer (GL_FRAMEBUFFER, status->prev_framebuffer);
  glBindRenderbuffer (GL_RENDERBUFFER, status->prev_renderbuffer);

  glPopAttrib ();
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

  cairo_surface_flush (surface);
  _mech_gl_view_update_children ((MechGLView *) area);
  _gl_status_save (&gl_status);

  _mech_gl_view_check_update_fbo ((MechGLView *) area);
  glBindFramebuffer (GL_FRAMEBUFFER, priv->fbo);

  glClear (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glViewport (0, 0, allocation.width, allocation.height);
  glScissor (0, 0, allocation.width, allocation.height);

  g_signal_emit (area, signals[RENDER_SCENE], 0,
                 cairo_surface_get_device (surface));

  glFlush ();
  _gl_status_restore (&gl_status);
  cairo_surface_mark_dirty (surface);
}

static void
_mech_gl_view_leave_child (MechGLView *view,
                           MechEvent  *orig,
                           MechArea   *child)
{
  MechContainer *container;
  MechGLViewPrivate *priv;
  MechEvent *event;

  priv = mech_gl_view_get_instance_private (view);

  container = g_hash_table_lookup (priv->children, child);
  event = mech_event_new (MECH_LEAVE);
  mech_event_set_seat (event, mech_event_get_seat (orig));
  mech_event_set_area (event, child);
  event->any.serial = orig->any.serial;
  mech_container_handle_event (container, event);
  mech_event_free (event);
}

static void
_mech_gl_view_enter_child (MechGLView *view,
                           MechEvent  *orig,
                           MechArea   *child,
                           gdouble     child_x,
                           gdouble     child_y)
{
  MechContainer *container;
  MechGLViewPrivate *priv;
  MechEvent *event;

  priv = mech_gl_view_get_instance_private (view);

  container = g_hash_table_lookup (priv->children, child);
  event = mech_event_new (MECH_MOTION);
  mech_event_set_seat (event, mech_event_get_seat (orig));
  mech_event_set_area (event, child);
  event->input.evtime = orig->input.evtime;
  event->input.modifiers = orig->input.modifiers;
  event->pointer.x = child_x;
  event->pointer.y = child_y;

  mech_container_handle_event (container, event);
  mech_event_free (event);
}

static MechArea *
_mech_gl_view_get_grab_child (MechGLView *view,
                              MechEvent  *event)
{
  MechGLViewPrivate *priv;
  gint id;

  priv = mech_gl_view_get_instance_private (view);

  switch (event->type)
    {
    case MECH_FOCUS_IN:
    case MECH_FOCUS_OUT:
    case MECH_KEY_PRESS:
    case MECH_KEY_RELEASE:
      return priv->focus_child;
    case MECH_MOTION:
    case MECH_BUTTON_PRESS:
    case MECH_BUTTON_RELEASE:
    case MECH_ENTER:
    case MECH_LEAVE:
    case MECH_SCROLL:
      return priv->grab_child;
    case MECH_TOUCH_DOWN:
    case MECH_TOUCH_UP:
    case MECH_TOUCH_MOTION:
      mech_event_touch_get_id (event, &id);
      return g_hash_table_lookup (priv->touch_children,
                                  GINT_TO_POINTER (id));
    }

  return NULL;
}

static void
_mech_gl_view_update_seat_state (MechGLView *view,
                                 MechEvent  *event,
                                 MechArea   *grab,
                                 MechArea   *child,
                                 gdouble     child_x,
                                 gdouble     child_y)
{
  MechContainer *container = NULL;
  MechArea *new_grab = NULL;
  MechGLViewPrivate *priv;
  gint id;

  priv = mech_gl_view_get_instance_private (view);

  if (grab || child)
    container = g_hash_table_lookup (priv->children,
                                     grab ? grab : child);

  if (container &&
      mech_container_has_grab_for_event (container, event))
    new_grab = grab ? grab : child;

  switch (event->type)
    {
    case MECH_FOCUS_IN:
    case MECH_FOCUS_OUT:
    case MECH_KEY_PRESS:
    case MECH_KEY_RELEASE:
      priv->focus_child = new_grab;
      break;
    case MECH_LEAVE:
      if (priv->pointer_child)
        _mech_gl_view_leave_child (view, event, priv->pointer_child);

      priv->pointer_child = NULL;
      /* Fall through */
    case MECH_ENTER:
      priv->grab_child = new_grab;
      break;
    case MECH_MOTION:
    case MECH_BUTTON_PRESS:
    case MECH_BUTTON_RELEASE:
    case MECH_SCROLL:
      if (!new_grab)
        {
          if (priv->pointer_child &&
              priv->pointer_child != child)
            _mech_gl_view_leave_child (view, event, priv->pointer_child);

          priv->pointer_child = child;
        }

      if (priv->grab_child && !new_grab &&
          child && child != priv->grab_child)
        {
          /* Send motion event into the child below now that the grab is gone */
          _mech_gl_view_enter_child (view, event, child, child_x, child_y);
        }

      priv->grab_child = new_grab;
      break;
    case MECH_TOUCH_DOWN:
    case MECH_TOUCH_UP:
    case MECH_TOUCH_MOTION:
      mech_event_touch_get_id (event, &id);

      if (new_grab)
        g_hash_table_insert (priv->touch_children,
                             GINT_TO_POINTER (id),
                             new_grab);
      else
        g_hash_table_remove (priv->touch_children,
                             GINT_TO_POINTER (id));
      break;
    }
}

static gboolean
mech_gl_view_handle_event (MechArea  *area,
                           MechEvent *event)
{
  MechArea *child = NULL, *under_pointer = NULL, *grab_child;
  MechGLView *view = (MechGLView *) area;
  MechContainer *container = NULL;
  gdouble x, y, child_x, child_y;
  MechGLViewPrivate *priv;
  gboolean retval = FALSE;

  if (event->any.target != area ||
      mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  MECH_AREA_CLASS (mech_gl_view_parent_class)->handle_event (area, event);

  child_x = child_y = 0;
  priv = mech_gl_view_get_instance_private (view);
  grab_child = _mech_gl_view_get_grab_child (view, event);

  if (mech_event_pointer_get_coords (event, &x, &y))
    {
      GLStatus gl_status = { 0 };
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

      if (child && (!grab_child || grab_child == child))
        {
          event->pointer.x = child_x;
          event->pointer.y = child_y;
        }
      else if (grab_child)
        {
          g_signal_emit (area, signals[CHILD_COORDS], 0,
                         grab_child, x, y,
                         &event->pointer.x, &event->pointer.y);
        }

      _gl_status_restore (&gl_status);
      _mech_surface_release (surface);
    }

  if (grab_child || child)
    container = g_hash_table_lookup (priv->children,
                                     (grab_child) ? grab_child : child);
  if (container)
    {
      mech_event_set_area (event, (grab_child) ? grab_child : child);
      retval = mech_container_handle_event (container, event);
    }

  _mech_gl_view_update_seat_state (view, event, grab_child, child,
                                   child_x, child_y);
  return retval;
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
mech_gl_view_visibility_changed (MechArea *area)
{
  if (!mech_area_is_visible (area))
    return;

  /* Ensure there is a suitable GL surface */
  if (mech_area_get_effective_surface_type (area) == MECH_SURFACE_TYPE_SOFTWARE)
    mech_area_set_surface_type (area, MECH_SURFACE_TYPE_GL);
  else if (mech_area_get_surface_type (area) != MECH_SURFACE_TYPE_GL &&
           mech_area_get_effective_surface_type (area) == MECH_SURFACE_TYPE_GL)
    mech_area_set_surface_type (area, MECH_SURFACE_TYPE_OFFSCREEN);
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
  area_class->visibility_changed = mech_gl_view_visibility_changed;

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

  priv = mech_gl_view_get_instance_private (view);
  priv->children = g_hash_table_new_full (NULL, NULL, NULL,
                                          (GDestroyNotify) g_object_unref);
  priv->touch_children = g_hash_table_new (NULL, NULL);
  mech_area_set_clip ((MechArea *) view, TRUE);
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
