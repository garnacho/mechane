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

#include <pango/pango.h>
#include "mech-text-attributes.h"
#include "mech-types.h"

typedef struct _MechTextAttributesPrivate MechTextAttributesPrivate;

enum {
  PROP_FONT_DESCRIPTION = 1,
  PROP_BACKGROUND,
  PROP_FOREGROUND,
#if 0
  /* FIXME: plenty of missing properties */
  PROP_FAMILY,
  PROP_LANGUAGE,
  PROP_LETTER_SPACING,
  PROP_RISE,
  PROP_SCALE,
  PROP_SIZE,
  PROP_SLANT,
  PROP_STRETCH,
  PROP_STRIKETHROUGH,
  PROP_UNDERLINE,
  PROP_VARIANT,
  PROP_WEIGHT
#endif
};

struct _MechTextAttributesPrivate
{
  PangoFontDescription *font_desc;
  MechColor background;
  MechColor foreground;
  guint fields;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechTextAttributes, mech_text_attributes,
                            G_TYPE_OBJECT)

static void
mech_text_attributes_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MechTextAttributesPrivate *priv;

  priv = mech_text_attributes_get_instance_private ((MechTextAttributes *) object);

  switch (prop_id)
    {
    case PROP_FONT_DESCRIPTION:
      if (priv->fields & MECH_TEXT_ATTRIBUTE_FONT_DESCRIPTION)
        g_value_set_boxed (value, priv->font_desc);
      break;
    case PROP_BACKGROUND:
      if (priv->fields & MECH_TEXT_ATTRIBUTE_BACKGROUND)
        g_value_set_boxed (value, &priv->background);
      break;
    case PROP_FOREGROUND:
      if (priv->fields & MECH_TEXT_ATTRIBUTE_FOREGROUND)
        g_value_set_boxed (value, &priv->foreground);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_text_attributes_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MechTextAttributesPrivate *priv;
  MechColor *color;

  priv = mech_text_attributes_get_instance_private ((MechTextAttributes *) object);

  switch (prop_id)
    {
    case PROP_FONT_DESCRIPTION:
      if (priv->font_desc)
        pango_font_description_free (priv->font_desc);

      priv->font_desc = g_value_dup_boxed (value);

      if (priv->font_desc)
        priv->fields |= MECH_TEXT_ATTRIBUTE_FONT_DESCRIPTION;
      else
        priv->fields &= ~(MECH_TEXT_ATTRIBUTE_FONT_DESCRIPTION);
      break;
    case PROP_BACKGROUND:
      color = g_value_get_boxed (value);

      if (color)
        {
          priv->background = *color;
          priv->fields |= MECH_TEXT_ATTRIBUTE_BACKGROUND;
        }
      else
        priv->fields &= ~(MECH_TEXT_ATTRIBUTE_BACKGROUND);
      break;
    case PROP_FOREGROUND:
      color = g_value_get_boxed (value);

      if (color)
        {
          priv->foreground = *color;
          priv->fields |= MECH_TEXT_ATTRIBUTE_FOREGROUND;
        }
      else
        priv->fields &= ~(MECH_TEXT_ATTRIBUTE_FOREGROUND);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_text_attributes_finalize (GObject *object)
{
  MechTextAttributesPrivate *priv;

  priv = mech_text_attributes_get_instance_private ((MechTextAttributes *) object);

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);

  G_OBJECT_CLASS (mech_text_attributes_parent_class)->finalize (object);
}

static void
mech_text_attributes_class_init (MechTextAttributesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mech_text_attributes_get_property;
  object_class->set_property = mech_text_attributes_set_property;
  object_class->finalize = mech_text_attributes_finalize;

  g_object_class_install_property (object_class,
                                   PROP_FONT_DESCRIPTION,
                                   g_param_spec_boxed ("font-description",
                                                       "Font description",
                                                       "Font description",
                                                       PANGO_TYPE_FONT_DESCRIPTION,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_boxed ("background",
                                                       "Background color",
                                                       "Background color",
                                                       MECH_TYPE_COLOR,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND,
                                   g_param_spec_boxed ("foreground",
                                                       "Foreground color",
                                                       "Foreground color",
                                                       MECH_TYPE_COLOR,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));
}

static void
mech_text_attributes_init (MechTextAttributes *attributes)
{
}

MechTextAttributes *
mech_text_attributes_new (void)
{
  return g_object_new (MECH_TYPE_TEXT_ATTRIBUTES, NULL);
}

MechTextAttributeFields
mech_text_attributes_get_set_fields (MechTextAttributes *attributes)
{
  MechTextAttributesPrivate *priv;

  g_return_val_if_fail (MECH_IS_TEXT_ATTRIBUTES (attributes), 0);

  priv = mech_text_attributes_get_instance_private (attributes);
  return priv->fields;
}

void
mech_text_attributes_unset_fields (MechTextAttributes      *attributes,
                                   MechTextAttributeFields  fields)
{
  MechTextAttributesPrivate *priv;

  g_return_if_fail (MECH_IS_TEXT_ATTRIBUTES (attributes));

  priv = mech_text_attributes_get_instance_private (attributes);
  priv->fields &= ~(fields & MECH_TEXT_ATTRIBUTE_ALL_FIELDS);
}

void
mech_text_attributes_combine (MechTextAttributes       *attributes,
                              const MechTextAttributes *other)
{
  MechTextAttributesPrivate *priv, *other_priv;
  MechTextAttributeFields fields;

  g_return_if_fail (MECH_IS_TEXT_ATTRIBUTES (attributes));
  g_return_if_fail (MECH_IS_TEXT_ATTRIBUTES (other));

  priv = mech_text_attributes_get_instance_private (attributes);
  other_priv = mech_text_attributes_get_instance_private ((MechTextAttributes *) other);

  fields = other_priv->fields;

  if (fields & MECH_TEXT_ATTRIBUTE_FONT_DESCRIPTION)
    {
      if (!priv->font_desc && other_priv->font_desc)
        priv->font_desc = pango_font_description_copy (other_priv->font_desc);
      else if (priv->font_desc)
        pango_font_description_merge (priv->font_desc,
                                      other_priv->font_desc, TRUE);

      g_object_notify ((GObject *) attributes, "font-description");
    }

  if (fields & MECH_TEXT_ATTRIBUTE_FOREGROUND)
    {
      priv->foreground = other_priv->foreground;
      g_object_notify ((GObject *) attributes, "foreground");
    }

  if (fields & MECH_TEXT_ATTRIBUTE_BACKGROUND)
    {
      priv->background = other_priv->background;
      g_object_notify ((GObject *) attributes, "background");
    }

  priv->fields |= fields;
}
