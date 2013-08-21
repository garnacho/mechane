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


#include <mechane/mech-area-private.h>
#include <mechane/mech-container.h>
#include <mechane/mechane.h>

typedef struct _MechContainerPrivate MechContainerPrivate;

struct _MechContainerPrivate
{
  MechArea *root;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechContainer, mech_container,
                                     G_TYPE_OBJECT)

static void
mech_container_constructed (GObject *object)
{
  MechContainer *container = (MechContainer *) object;
  MechContainerPrivate *priv;
  MechContainerClass *klass;

  priv = mech_container_get_instance_private (container);
  klass = MECH_CONTAINER_GET_CLASS (container);

  if (!klass->create_root)
    g_warning (G_STRLOC
               ": %s has no MechContainerClass->create_root() implementation",
               G_OBJECT_CLASS_NAME (klass));
  else
    priv->root = klass->create_root (container);
}

static void
mech_container_finalize (GObject *object)
{
  MechContainerPrivate *priv;

  priv = mech_container_get_instance_private ((MechContainer *) object);
  g_object_unref (priv->root);

  G_OBJECT_CLASS (mech_container_parent_class)->finalize (object);
}

static void
mech_container_class_init (MechContainerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructed = mech_container_constructed;
  object_class->finalize = mech_container_finalize;
}

static void
mech_container_init (MechContainer *container)
{
}

MechArea *
mech_container_get_root (MechContainer *container)
{
  MechContainerPrivate *priv;

  g_return_val_if_fail (MECH_IS_CONTAINER (container), NULL);

  priv = mech_container_get_instance_private (container);

  return priv->root;
}
