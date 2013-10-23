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

#include <mechane/mech-container-private.h>
#include <mechane/mech-gl-container-private.h>
#include <mechane/mech-backend-private.h>
#include <mechane/mech-area-private.h>

enum {
  PROP_PARENT = 1
};

typedef struct _MechGLContainerPrivate MechGLContainerPrivate;

struct _MechGLContainerPrivate
{
  MechArea *parent;
  MechArea *area;
  MechSurface *surface;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechGLContainer, mech_gl_container,
                            MECH_TYPE_CONTAINER)

static void
_mech_gl_container_update_surface (MechGLContainer *container)
{
  MechSurface *parent_surface, *surface = NULL;
  MechGLContainerPrivate *priv;

  priv = mech_gl_container_get_instance_private (container);

  if (priv->parent && mech_area_is_visible (priv->parent))
    {
      MechBackend *backend;
      MechWindow *window;
      MechStage *stage;

      backend = mech_backend_get ();
      stage = _mech_area_get_stage (priv->parent);
      parent_surface = _mech_stage_get_rendering_surface (stage, priv->parent);

      if (_mech_surface_get_renderer_type (parent_surface) == MECH_RENDERER_TYPE_GL)
        {
          surface = mech_backend_create_surface (backend, parent_surface,
                                                 MECH_SURFACE_TYPE_OFFSCREEN);
          g_initable_init (G_INITABLE (surface), NULL, NULL);
        }

      _mech_container_set_surface (MECH_CONTAINER (container), surface);
      window = mech_area_get_window (priv->parent);
      _mech_area_set_window (priv->area, window);
    }
  else
    {
      _mech_container_set_surface (MECH_CONTAINER (container), NULL);
      _mech_area_set_window (priv->area, NULL);
    }

  if (priv->surface)
    g_object_unref (priv->surface);

  priv->surface = surface;
}

static void
_parent_visibility_changed (MechArea        *parent,
                            MechGLContainer *container)
{
  _mech_gl_container_update_surface (container);
}

static void
_mech_gl_container_set_parent (MechGLContainer *container,
                               MechArea        *parent)
{
  MechGLContainerPrivate *priv;
  MechSurface *surface = NULL;

  priv = mech_gl_container_get_instance_private (container);

  if (priv->parent == parent)
    return;

  if (priv->parent)
    g_signal_handlers_disconnect_by_data (priv->parent, container);

  priv->parent = parent;

  if (priv->parent)
    {
      g_signal_connect_after (priv->parent, "visibility-changed",
                              G_CALLBACK (_parent_visibility_changed),
                              container);

      if (mech_area_is_visible (priv->parent))
        _mech_gl_container_update_surface (container);
    }
}

static void
mech_gl_container_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_PARENT:
      _mech_gl_container_set_parent (MECH_GL_CONTAINER (object),
                                     g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_gl_container_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MechGLContainerPrivate *priv;

  priv = mech_gl_container_get_instance_private ((MechGLContainer *) object);

  switch (prop_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_gl_container_finalize (GObject *object)
{
  MechGLContainerPrivate *priv;

  priv = mech_gl_container_get_instance_private ((MechGLContainer *) object);

  if (priv->surface)
    g_object_unref (priv->surface);

  G_OBJECT_CLASS (mech_gl_container_parent_class)->finalize (object);
}

static MechArea *
mech_gl_container_create_root (MechContainer *container)
{
  MechGLContainerPrivate *priv;

  priv = mech_gl_container_get_instance_private ((MechGLContainer *) container);

  if (!priv->area)
    priv->area = mech_area_new (NULL, MECH_NONE);

  return g_object_ref (priv->area);
}

static void
mech_gl_container_class_init (MechGLContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechContainerClass *container_class = MECH_CONTAINER_CLASS (klass);

  object_class->set_property = mech_gl_container_set_property;
  object_class->get_property = mech_gl_container_get_property;
  object_class->finalize = mech_gl_container_finalize;

  container_class->create_root = mech_gl_container_create_root;

  g_object_class_install_property (object_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        "Parent",
                                                        "Parent area of the GL container",
                                                        MECH_TYPE_AREA,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
mech_gl_container_init (MechGLContainer *container)
{
}

MechGLContainer *
_mech_gl_container_new (MechArea *parent)
{
  return g_object_new (MECH_TYPE_GL_CONTAINER,
                       "parent", parent,
                       NULL);
}

guint
_mech_gl_container_get_texture_id (MechGLContainer *container)
{
  MechGLContainerPrivate *priv;
  guint texture_id = 0;

  g_return_val_if_fail (MECH_IS_GL_CONTAINER (container), 0);

  priv = mech_gl_container_get_instance_private (container);

  if (!priv->surface)
    _mech_gl_container_update_surface (container);

  if (priv->surface &&
      g_object_class_find_property (G_OBJECT_GET_CLASS (priv->surface),
                                    "texture-id"))
    g_object_get (priv->surface, "texture-id", &texture_id, NULL);

  return texture_id;
}
