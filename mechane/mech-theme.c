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

#include <cairo-gobject.h>
#include <pango/pango.h>
#include "mech-theme.h"
#include "mech-style-private.h"
#include "mech-pattern-private.h"
#include "mech-parser-private.h"

#define RULE_LITERAL(s) _mech_parser_rule_literal_create (parser, (s))
#define RULE_COMPOUND(r) _mech_parser_rule_compound_create (parser, (r), G_N_ELEMENTS ((r)))
#define RULE_OR(r) _mech_parser_rule_or_create (parser, (r), G_N_ELEMENTS ((r)))
#define RULE_TYPE(t) _mech_parser_rule_type_create (parser, (t))
#define RULE_OPTIONAL(r) _mech_parser_rule_optional_create (parser, (r))

typedef struct _MechThemePrivate MechThemePrivate;
typedef struct _ThemeParserData ThemeParserData;

enum {
  RULE_COLOR,
  RULE_URL,
  RULE_ASSET,
  RULE_SIZE,
  RULE_PATTERN,
  RULE_FONT,
  RULE_MARGIN,
  RULE_BORDER,
  RULE_STATE_FLAGS,
  RULE_SIDE_FLAGS,
  RULE_PATH,
  RULE_STYLE_PROPERTIES,
  RULE_STYLE_PROPERTIES_BRACED,
  RULE_LAST
};

struct _ThemeParserData
{
  MechTheme *theme;
  MechStyle *style;
};

struct _MechThemePrivate
{
  MechParser *parser;
  GHashTable *key_rules;
  guint rules[RULE_LAST];
};

static GQuark quark_theme_error = 0;

G_DEFINE_TYPE_WITH_PRIVATE (MechTheme, mech_theme, G_TYPE_OBJECT)

static void
mech_theme_finalize (GObject *object)
{
  MechThemePrivate *priv;

  priv = mech_theme_get_instance_private ((MechTheme *) object);
  g_hash_table_unref (priv->key_rules);
  g_object_unref (priv->parser);

  G_OBJECT_CLASS (mech_theme_parent_class)->finalize (object);
}

static void
mech_theme_class_init (MechThemeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_theme_finalize;

  quark_theme_error = g_quark_from_static_string ("mech-theme-error-quark");
}

static guint
_theme_get_key_rule (MechTheme *theme,
                     GType      type)
{
  MechThemePrivate *priv;
  MechParser *parser;
  gpointer value;

  priv = mech_theme_get_instance_private (theme);
  parser = priv->parser;

  if (!g_hash_table_lookup_extended (priv->key_rules,
                                     GSIZE_TO_POINTER (type),
                                     NULL, &value))
    {
      guint rule;

      rule = _mech_parser_rule_container_create (parser,
                                                 RULE_TYPE (type),
                                                 '[', ']');
      g_hash_table_insert (priv->key_rules, GSIZE_TO_POINTER (type),
                           GUINT_TO_POINTER (rule));
      return rule;
    }
  else
    return GPOINTER_TO_UINT (value);
}

static gboolean
_theme_compose_color (MechParser         *parser,
                      MechParserContext  *context,
                      GPtrArray          *members,
                      GValue             *retval,
                      GError            **error)
{
  MechColor color = { 0 };
  GValue *value;

  if (members->len == 1)
    {
      PangoColor pango_color;

      value = g_ptr_array_index (members, 0);

      if (!G_VALUE_HOLDS_STRING (value))
        return FALSE;

      if (!pango_color_parse (&pango_color, g_value_get_string (value)))
        return FALSE;

      color.r = pango_color.red / 65535.;
      color.g = pango_color.green / 65535.;
      color.b = pango_color.blue / 65535.;
      color.a = 1;
    }
  else if (members->len >= 3)
    {
      color.r = g_value_get_int (g_ptr_array_index (members, 0)) / 255.;
      color.g = g_value_get_int (g_ptr_array_index (members, 1)) / 255.;
      color.b = g_value_get_int (g_ptr_array_index (members, 2)) / 255.;

      if (members->len == 4)
        color.a = g_value_get_double (g_ptr_array_index (members, 3));
      else
        color.a = 1;
    }

  g_value_set_boxed (retval, &color);

  return TRUE;
}

static gboolean
_theme_compose_file (MechParser         *parser,
                     MechParserContext  *context,
                     GPtrArray          *members,
                     GValue             *value,
                     GError            **error)
{
  const gchar *str;
  GFile *file;

  if (members->len != 1)
    return FALSE;

  str = g_value_get_string (g_ptr_array_index (members, 0));
  file = g_file_new_for_uri (str);
  g_value_take_object (value, file);
  return TRUE;
}

static void
_theme_compose_gradient_color_stops (cairo_pattern_t *pattern,
                                     GPtrArray       *color_stops)
{
  MechColor *color;
  gdouble pos;
  guint i;

  for (i = 0; i < color_stops->len; i += 2)
    {
      pos = g_value_get_double (g_ptr_array_index (color_stops, i));
      color = g_value_get_boxed (g_ptr_array_index (color_stops, i + 1));

      cairo_pattern_add_color_stop_rgba (pattern, pos, color->r, color->g,
                                         color->b, color->a);
    }
}

static gboolean
_theme_compose_pattern (MechParser         *parser,
                        MechParserContext  *context,
                        GPtrArray          *members,
                        GValue             *value,
                        GError            **error)
{
  MechPattern *pattern = NULL;
  cairo_pattern_t *pat = NULL;
  GPtrArray *color_stops;
  gdouble x0, y0, x1, y1;
  guint color_stop_pos;
  GValue *val;

  if (members->len == 0)
    return FALSE;

  val = g_ptr_array_index (members, 0);

  if (members->len == 1 &&
      G_VALUE_HOLDS (val, MECH_TYPE_COLOR))
    {
      MechColor *color;

      color = g_value_get_boxed (g_ptr_array_index (members, 0));
      pat = cairo_pattern_create_rgba (color->r, color->g,
                                       color->b, color->a);
    }
  else if (G_VALUE_HOLDS (val, G_TYPE_FILE))
    {
      cairo_extend_t extend = CAIRO_EXTEND_NONE;
      const gchar *layer = NULL;
      GFile *file;
      gint i = 1;

      file = g_value_get_object (g_ptr_array_index (members, 0));

      if (members->len > i)
        {
          val = g_ptr_array_index (members, i);

          if (G_VALUE_HOLDS (val, G_TYPE_STRING))
            {
              layer = g_value_get_string (val);
              i++;
            }
        }

      if (members->len > i)
        {
          val = g_ptr_array_index (members, i);

          if (G_VALUE_HOLDS (val, CAIRO_GOBJECT_TYPE_EXTEND))
            extend = g_value_get_enum (val);
        }

      pattern = _mech_pattern_new_asset (file, layer, extend);
    }
  else
    {
      if (members->len == 5)
        {
          x0 = g_value_get_double (g_ptr_array_index (members, 0));
          y0 = g_value_get_double (g_ptr_array_index (members, 1));
          x1 = g_value_get_double (g_ptr_array_index (members, 2));
          y1 = g_value_get_double (g_ptr_array_index (members, 3));
          pat = cairo_pattern_create_linear (x0, y0, x1, y1);
          color_stop_pos = 4;
        }
      else if (members->len == 7)
        {
          gdouble r0, r1;

          x0 = g_value_get_double (g_ptr_array_index (members, 0));
          y0 = g_value_get_double (g_ptr_array_index (members, 1));
          r0 = g_value_get_double (g_ptr_array_index (members, 2));
          x1 = g_value_get_double (g_ptr_array_index (members, 3));
          y1 = g_value_get_double (g_ptr_array_index (members, 4));
          r1 = g_value_get_double (g_ptr_array_index (members, 5));
          pat = cairo_pattern_create_radial (x0, y0, r0, x1, y1, r1);
          color_stop_pos = 6;
        }
      else
        return FALSE;

      color_stops =
        g_value_get_boxed (g_ptr_array_index (members, color_stop_pos));
      _theme_compose_gradient_color_stops (pat, color_stops);
    }

  if (!pattern && pat)
    pattern = _mech_pattern_new (pat);

  if (pattern)
    g_value_take_boxed (value, pattern);

  if (pat)
    cairo_pattern_destroy (pat);

  return pattern != NULL;
}

static guint
_mech_theme_register_color_type (MechParser *parser)
{
  guint rgba_args[] = {
    /* rgba ( r , g , b , a ) */
    RULE_LITERAL ("rgba"), RULE_LITERAL ("("),
    RULE_TYPE (G_TYPE_INT), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_INT), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_INT), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (")")
  };
  guint rgb_args[] = {
    /* rgb ( r , g , b ) */
    RULE_LITERAL ("rgb"), RULE_LITERAL ("("),
    RULE_TYPE (G_TYPE_INT), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_INT), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_INT), RULE_LITERAL (")")
  };
  guint combined[] = {
    RULE_COMPOUND (rgba_args),
    RULE_COMPOUND (rgb_args),
    RULE_TYPE (G_TYPE_STRING)
  };

  return _mech_parser_rule_converter_create (parser, MECH_TYPE_COLOR,
                                             _theme_compose_color,
                                             RULE_OR (combined));
}


static guint
_mech_theme_register_url (MechParser *parser)
{
  guint url_args[] = {
    RULE_LITERAL ("url"),
    _mech_parser_rule_container_create (parser, RULE_TYPE (G_TYPE_STRING),
                                        '(', ')')
  };

  return _mech_parser_rule_converter_create (parser, G_TYPE_FILE,
                                             _theme_compose_file,
                                             RULE_COMPOUND (url_args));
  return RULE_COMPOUND (url_args);
}

static guint
_mech_theme_register_asset (MechParser *parser,
                            MechTheme  *theme)
{
  MechThemePrivate *priv = mech_theme_get_instance_private (theme);
  guint layer_name =
    _mech_parser_rule_container_create (parser, RULE_TYPE (G_TYPE_STRING),
                                        '[', ']');
  guint asset_args[] = {
    priv->rules[RULE_URL],
    RULE_OPTIONAL (layer_name),
    RULE_OPTIONAL (RULE_TYPE (CAIRO_GOBJECT_TYPE_EXTEND))
  };

  return RULE_COMPOUND (asset_args);
}

static guint
_mech_theme_register_size_type (MechParser *parser)
{
  guint size_args[] = {
    /* <size> <unit> */
    RULE_TYPE (G_TYPE_DOUBLE), RULE_TYPE (MECH_TYPE_UNIT)
  };

  return RULE_COMPOUND (size_args);
}

static guint
_mech_theme_register_pattern (MechParser *parser,
                              MechTheme  *theme)
{
  MechThemePrivate *priv = mech_theme_get_instance_private (theme);
  guint color_stop_args[] = {
    /* <percentage> % : <color> */
    RULE_TYPE (G_TYPE_DOUBLE),
    RULE_LITERAL ("%"), RULE_LITERAL (":"),
    RULE_TYPE (MECH_TYPE_COLOR)
  };
  guint linear_gradient_args[] = {
    /* linear x1 , y1 , x2 , y2 */
    RULE_LITERAL ("linear"),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE)
  };
  guint radial_gradient_args[] = {
    /* radial x1 , y1 , r1 , x2 , y2 , r2 */
    RULE_LITERAL ("radial"),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE), RULE_LITERAL (","),
    RULE_TYPE (G_TYPE_DOUBLE)
  };
  guint gradient_type_args[] = {
    RULE_COMPOUND (linear_gradient_args),
    RULE_COMPOUND (radial_gradient_args)
  };
  guint gradient_colors_rule =
    _mech_parser_rule_multi_create (parser, RULE_COMPOUND (color_stop_args),
                                    ';', 2, -1);
  guint gradient_args[] = {
    RULE_OR (gradient_type_args),
    _mech_parser_rule_container_create (parser, gradient_colors_rule, '{', '}')
  };
  guint pattern_args[] = {
    priv->rules[RULE_ASSET],
    RULE_TYPE (MECH_TYPE_COLOR),
    RULE_COMPOUND (gradient_args),
  };

  return _mech_parser_rule_converter_create (parser, MECH_TYPE_PATTERN,
                                             _theme_compose_pattern,
                                             RULE_OR (pattern_args));
}

static guint
_mech_theme_register_font (MechParser *parser,
                           MechTheme  *theme)
{
  MechThemePrivate *priv = mech_theme_get_instance_private (theme);
  guint font_args[] = {
    /* <fontname> <size> */
    RULE_TYPE (G_TYPE_STRING), priv->rules[RULE_SIZE]
  };

  return RULE_COMPOUND (font_args);
}

static gboolean
_theme_compose_margin (MechParser         *parser,
                       MechParserContext  *context,
                       GPtrArray          *members,
                       GValue             *value,
                       GError            **error)
{
  MechBorder border;

  border.left = g_value_get_double (g_ptr_array_index (members, 0));
  border.left_unit = g_value_get_double (g_ptr_array_index (members, 1));
  border.left = g_value_get_double (g_ptr_array_index (members, 2));
  border.left_unit = g_value_get_double (g_ptr_array_index (members, 3));

  if (members->len == 8)
    {
      border.right = g_value_get_double (g_ptr_array_index (members, 4));
      border.right_unit = g_value_get_double (g_ptr_array_index (members, 5));
      border.bottom = g_value_get_double (g_ptr_array_index (members, 6));
      border.bottom_unit = g_value_get_double (g_ptr_array_index (members, 7));
    }
  else
    {
      border.right = border.left;
      border.right_unit = border.left_unit;
      border.bottom = border.top;
      border.bottom_unit = border.top_unit;
    }

  g_value_set_boxed (value, &border);
  return TRUE;
}

static guint
_mech_theme_register_margin_type (MechParser *parser,
                                  MechTheme  *theme)
{
  MechThemePrivate *priv = mech_theme_get_instance_private (theme);
  guint margin_rule =
    _mech_parser_rule_multi_create (parser, priv->rules[RULE_SIZE], ',', 2, 4);

  return _mech_parser_rule_converter_create (parser, MECH_TYPE_BORDER,
                                             _theme_compose_margin,
                                             margin_rule);
}

static guint
_mech_theme_register_border_type (MechParser *parser,
                                  MechTheme  *theme)
{
  MechThemePrivate *priv = mech_theme_get_instance_private (theme);
  guint source_args[] = {
    priv->rules[RULE_PATTERN],
    RULE_TYPE (MECH_TYPE_COLOR)
  };
  guint border_args[] = {
    _mech_parser_rule_multi_create (parser, RULE_TYPE (G_TYPE_DOUBLE), ',', 2, 4),
    RULE_OR (source_args)
  };

  return RULE_COMPOUND (border_args);
}

static gboolean
_theme_path_func (MechParser         *parser,
                  MechParserContext  *context,
                  GError            **error)
{
  MechThemePrivate *priv;
  ThemeParserData *data;
  gboolean retval;
  gint i = 0;

  data = _mech_parser_context_get_user_data (context);
  priv = mech_theme_get_instance_private (data->theme);

  while (TRUE)
    {
      gchar *element;
      guint state = 0;

      if (!_mech_parser_context_apply (context, RULE_TYPE (G_TYPE_STRING),
                                      error, &element))
        return FALSE;

      if (_mech_parser_context_test_rule (context, RULE_LITERAL ("["), NULL))
        {
          if (!_mech_parser_context_apply (context,
                                           priv->rules[RULE_STATE_FLAGS],
                                           error, &state))
            {
              g_free (element);
              return FALSE;
            }
        }

      _mech_style_push_path (data->style, element, state);
      g_free (element);
      i++;

      if (_mech_parser_context_test_rule (context, RULE_LITERAL ("{"), NULL))
        break;
    }

  retval = _mech_parser_context_apply_rule (context,
                                            priv->rules[RULE_STYLE_PROPERTIES_BRACED],
                                            NULL, error);
  while (i)
    {
      _mech_style_pop_path (data->style);
      i--;
    }

  return retval;
}

static gboolean
_theme_property_apply (MechParser              *parser,
                       MechParserContext       *context,
                       const MechPropertyInfo  *info,
                       gint                     layer,
                       gint                     key,
                       GError                 **error)
{
  guint i, n_children, property;
  MechPropertyInfo **children;
  ThemeParserData *data;

  data = _mech_parser_context_get_user_data (context);

  if (_mech_style_property_info_get_subproperties (info, &children, &n_children))
    {
      for (i = 0; i < n_children; i++)
        {
          if (!_theme_property_apply (parser, context, children[i], layer, key, error))
            return FALSE;

          if (_mech_parser_context_test_rule (context, RULE_LITERAL (";"), NULL) ||
              _mech_parser_context_test_rule (context, RULE_LITERAL ("}"), NULL))
            break;
        }
    }
  else
    {
      GValue value = { 0 };
      GType type;

      type = _mech_style_property_info_get_property_type (info);
      property = _mech_style_property_info_get_id (info);

      if (key < 0)
        _mech_style_property_info_is_associative (info, NULL, &key);

      if (!_mech_parser_context_apply_rule (context, RULE_TYPE (type),
                                            &value, error))
        return FALSE;

      _mech_style_set_property (data->style, property, layer, key, &value);
      g_value_unset (&value);
    }

  return TRUE;
}

static gboolean
_theme_property_func (MechParser         *parser,
                      MechParserContext  *context,
                      GError            **error)
{
  const MechPropertyInfo *parent = NULL, *info;
  gint key = -1, layer = 0;
  ThemeParserData *data;
  gchar *property;
  GType type;

  data = _mech_parser_context_get_user_data (context);

  while (TRUE)
    {
      gint *value = NULL;

      _mech_parser_context_apply (context, RULE_TYPE (G_TYPE_STRING),
                                  NULL, &property);
      info = _mech_style_property_info_lookup (parent, property);

      if (!info)
        {
          g_set_error (error, quark_theme_error, 0,
                       "'%s' is not a valid property",
                       property);
          g_free (property);
          return FALSE;
        }

      g_free (property);

      if (_mech_style_property_info_is_layered (info))
        {
          value = &layer;
          type = G_TYPE_INT;
        }
      else if (_mech_style_property_info_is_associative (info, &type, &key))
        value = &key;

      if (value &&
          _mech_parser_context_test_rule (context, RULE_LITERAL ("["), NULL))
        {
          if (!_mech_parser_context_apply (context,
                                          _theme_get_key_rule (data->theme,
                                                               type),
                                          error, value))
            return FALSE;
        }

      if (!_mech_parser_context_apply (context, RULE_LITERAL ("."), NULL))
        break;

      parent = info;
    }

  if (!info)
    return FALSE;

  if (!_mech_parser_context_apply (context, RULE_LITERAL (":"), error))
    return FALSE;

  return _theme_property_apply (parser, context, info, layer, key, error);
}

static guint
_mech_theme_register_style_properties (MechParser *parser,
                                       MechTheme  *theme)
{
  MechThemePrivate *priv = mech_theme_get_instance_private (theme);
  guint property_rule_args[] = {
    RULE_LITERAL ("-"),
    _mech_parser_rule_custom_create (parser, _theme_property_func)
  };
  guint properties_or_paths_args[] = {
    RULE_COMPOUND (property_rule_args),
    priv->rules[RULE_PATH]
  };

  return _mech_parser_rule_multi_create (parser, RULE_OR (properties_or_paths_args),
                                         ';', -1, -1);
}

static void
mech_theme_init (MechTheme *theme)
{
  MechThemePrivate *priv;
  MechParser *parser;

  priv = mech_theme_get_instance_private (theme);
  priv->key_rules = g_hash_table_new (NULL, NULL);
  priv->parser = parser = _mech_parser_new ();

  /* create rules used on theme parsing */
  priv->rules[RULE_COLOR] = _mech_theme_register_color_type (priv->parser);
  priv->rules[RULE_URL] = _mech_theme_register_url (priv->parser);
  priv->rules[RULE_ASSET] = _mech_theme_register_asset (priv->parser, theme);
  priv->rules[RULE_SIZE] = _mech_theme_register_size_type (priv->parser);
  priv->rules[RULE_PATTERN] = _mech_theme_register_pattern (priv->parser, theme);
  priv->rules[RULE_FONT] = _mech_theme_register_font (priv->parser, theme);
  priv->rules[RULE_MARGIN] = _mech_theme_register_margin_type (priv->parser, theme);
  priv->rules[RULE_BORDER] = _mech_theme_register_border_type (priv->parser, theme);
  priv->rules[RULE_STATE_FLAGS] =
    _mech_parser_rule_container_create (priv->parser,
                                        RULE_TYPE (MECH_TYPE_STATE_FLAGS),
                                        '[', ']');
  priv->rules[RULE_SIDE_FLAGS] =
    _mech_parser_rule_container_create (priv->parser,
                                        RULE_TYPE (MECH_TYPE_SIDE_FLAGS),
                                        '[', ']');
  priv->rules[RULE_PATH] =
    _mech_parser_rule_custom_create (priv->parser, _theme_path_func);

  priv->rules[RULE_STYLE_PROPERTIES] =
    _mech_theme_register_style_properties (parser, theme);
  priv->rules[RULE_STYLE_PROPERTIES_BRACED] =
    _mech_parser_rule_container_create (priv->parser,
                                        priv->rules[RULE_STYLE_PROPERTIES],
                                        '{', '}');

  /* set the root parser rule */
  _mech_parser_set_root_rule (priv->parser,
                              priv->rules[RULE_STYLE_PROPERTIES]);
}

MechTheme *
mech_theme_new (void)
{
  return g_object_new (MECH_TYPE_THEME, NULL);
}

gboolean
mech_theme_load_style_from_data (MechTheme    *theme,
                                 MechStyle    *style,
                                 const gchar  *data,
                                 gssize        len,
                                 GError      **error)
{
  ThemeParserData parser_data;
  MechThemePrivate *priv;

  g_return_val_if_fail (MECH_IS_THEME (theme), FALSE);
  g_return_val_if_fail (MECH_IS_STYLE (style), FALSE);

  priv = mech_theme_get_instance_private (theme);
  parser_data.theme = theme;
  parser_data.style = style;

  return _mech_parser_load_from_data (priv->parser, data, len,
                                      &parser_data, error);
}

gboolean
mech_theme_load_style_from_file (MechTheme    *theme,
                                 MechStyle    *style,
                                 GFile        *file,
                                 GError      **error)
{
  ThemeParserData parser_data;
  MechThemePrivate *priv;

  g_return_val_if_fail (MECH_IS_THEME (theme), FALSE);
  g_return_val_if_fail (MECH_IS_STYLE (style), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  priv = mech_theme_get_instance_private (theme);
  parser_data.theme = theme;
  parser_data.style = style;

  return _mech_parser_load_from_file (priv->parser, file, &parser_data, error);
}
