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

#include <gobject/gvaluecollector.h>
#include <mechane/mechane.h>
#include <string.h>
#include "mech-parser-private.h"
#include "mech-marshal.h"

typedef struct _MechParserPrivate MechParserPrivate;
typedef struct _ParserRule ParserRule;
typedef struct _ParserStackedRule ParserStackedRule;
typedef enum _ParserRuleType ParserRuleType;

enum {
  ERROR,
  N_SIGNALS
};

enum _ParserRuleType {
  RULE_NONE,
  RULE_TOPLEVEL,
  RULE_BASIC,
  RULE_CONVERTER,
  RULE_LITERAL,
  RULE_COMPOUND
};

enum MechParserError {
  MECH_PARSER_ERROR_FAILED
};

struct _ParserRule
{
  ParserRuleType type;
  GArray *child_rules;

  union {
    struct {
      GType gtype;
    } basic;

    struct {
      GType gtype;
      MechParserConvertFunc func;
    } converter;

    struct {
      const gchar *str;
      gsize len;
    } literal;
  } data;
};

struct _ParserStackedRule
{
  const gchar *current_pos;
  const gchar *error_pos;
  GError *error;
  guint rule;
  guint parent_type;
  guint current_child; /* Position in rule->child_rules */
  guint n_iterations; /* Used on multi rules */
  guint started  : 1;
  guint finished : 1;
};

struct _MechParserContext
{
  MechParser *parser;
  gpointer user_data;
  const gchar *data;
  gssize data_len;
  GArray *rule_stack;
  GPtrArray *data_stack;
  GArray *delimiters;
  gchar *delimiters_string;
};

struct _MechParserPrivate
{
  GArray *rules;
  GHashTable *type_parsers;
  GHashTable *types;
  GHashTable *literals;
  GHashTable *toplevel_containers;
  guint root_rule;
};

static guint signals[N_SIGNALS] = { 0 };
static GQuark quark_parser_error = 0;

#define RULE_ID(a,r) ((r - (ParserRule *) (a)->data) + 1)
#define RULE_FOR_ID(a,i) (&g_array_index ((a), ParserRule, (i) - 1))
#define IS_VALID_ID(a,i) ((i) > 0 && (i) <= (a)->len)

G_DEFINE_TYPE_WITH_PRIVATE (MechParser, mech_parser, G_TYPE_OBJECT)

static void
mech_parser_finalize (GObject *object)
{
  MechParserPrivate *priv;
  guint i;

  priv = mech_parser_get_instance_private ((MechParser *) object);
  g_hash_table_unref (priv->type_parsers);
  g_hash_table_unref (priv->types);
  g_hash_table_unref (priv->literals);
  g_hash_table_unref (priv->toplevel_containers);

  for (i = 0; i < priv->rules->len; i++)
    {
      ParserRule *rule;

      rule = &g_array_index (priv->rules, ParserRule, i);

      if (rule->child_rules)
        g_array_unref (rule->child_rules);
    }

  g_array_unref (priv->rules);

  G_OBJECT_CLASS (mech_parser_parent_class)->finalize (object);
}

static gboolean
mech_parser_error_impl (MechParser   *parser,
                        const GError *error)
{
  g_warning ("Parser error: %s", error->message);
  return FALSE;
}

static void
mech_parser_class_init (MechParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_parser_finalize;
  klass->error = mech_parser_error_impl;

  signals[ERROR] =
    g_signal_new ("error",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechParserClass, error),
                  NULL, NULL,
                  _mech_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);

  quark_parser_error = g_quark_from_static_string ("mech-parser-error-quark");
}

static ParserStackedRule *
_parser_context_peek (MechParserContext  *context,
                      ParserRule        **rule)
{
  MechParserPrivate *priv;
  ParserStackedRule *data;

  if (rule)
    *rule = NULL;

  if (context->rule_stack->len == 0)
    return NULL;

  priv = mech_parser_get_instance_private (context->parser);
  data = &g_array_index (context->rule_stack, ParserStackedRule,
                         context->rule_stack->len - 1);
  if (rule)
    *rule = RULE_FOR_ID (priv->rules, data->rule);

  return data;
}

gpointer
_mech_parser_context_get_user_data (MechParserContext *context)
{
  return context->user_data;
}

const gchar *
_mech_parser_context_get_current (MechParserContext *context)
{
  ParserStackedRule *stack_data;

  stack_data = _parser_context_peek (context, NULL);

  if (stack_data)
    return stack_data->current_pos;

  return context->data;
}

void
_parser_context_set_current (MechParserContext *context,
                             const gchar       *pos)
{
  ParserStackedRule *stack_data;

  g_assert (pos >= context->data && pos <= &context->data[context->data_len]);

  stack_data = _parser_context_peek (context, NULL);
  g_assert (stack_data != NULL);
  stack_data->current_pos = pos;
}

void
_parser_context_reset_current (MechParserContext *context)
{
  ParserStackedRule *parent;
  const gchar *pos;

  if (context->rule_stack->len > 1)
    {
      parent = &g_array_index (context->rule_stack, ParserStackedRule,
                               context->rule_stack->len - 2);
      pos = parent->current_pos;
    }
  else
    pos = context->data;

  _parser_context_set_current (context, pos);
}

static gboolean
_parser_context_peek_error (MechParserContext  *context,
                            GError            **error,
                            gchar             **error_pos)
{
  ParserStackedRule *current_rule;

  current_rule = _parser_context_peek (context, NULL);

  if (!current_rule)
    return FALSE;

  if (error)
    *error = current_rule->error;

  if (error_pos)
    *error_pos = (gchar *) current_rule->error_pos;

  return current_rule->error != NULL;
}

static void
_parser_context_take_error (MechParserContext *context,
                            GError            *error,
                            const gchar       *error_pos)
{
  ParserStackedRule *current_rule;

  current_rule = _parser_context_peek (context, NULL);

  if (!current_rule || current_rule->error == error)
    return;

  if (current_rule->error)
    {
      g_error_free (current_rule->error);
      current_rule->error = NULL;
      current_rule->error_pos = NULL;
    }

  if (error)
    {
      current_rule->error = error;
      current_rule->error_pos =
        (error_pos) ? error_pos : _mech_parser_context_get_current (context);
    }
}

static guint
_parser_rule_toplevel_create (MechParser *parser,
                              guint       rule)
{
  ParserRule new_rule = { 0 };
  MechParserPrivate *priv;
  guint toplevel_rule;
  gpointer data;

  priv = mech_parser_get_instance_private (parser);

  if (g_hash_table_lookup_extended (priv->toplevel_containers,
                                    GUINT_TO_POINTER (rule), NULL, &data))
    return GPOINTER_TO_UINT (data);

  new_rule.type = RULE_TOPLEVEL;
  new_rule.child_rules = g_array_new (FALSE, FALSE, sizeof (guint));
  g_array_append_val (new_rule.child_rules, rule);
  g_array_append_val (priv->rules, new_rule);
  toplevel_rule = priv->rules->len;

  g_hash_table_insert (priv->toplevel_containers,
                       GUINT_TO_POINTER (rule),
                       GUINT_TO_POINTER (toplevel_rule));
  return toplevel_rule;
}

static void
_parser_context_push (MechParserContext *context,
                      guint              child_rule)
{
  ParserStackedRule *parent, new_data = { 0 };
  ParserRule *parent_rule;

  parent = _parser_context_peek (context, &parent_rule);
  g_assert (parent != NULL);

  parent->current_child = child_rule;
  parent->started = TRUE;

  new_data.current_pos = _mech_parser_context_get_current (context);
  new_data.parent_type = parent_rule->type;
  new_data.rule = g_array_index (parent_rule->child_rules,
                                 guint, child_rule);
  g_array_append_val (context->rule_stack, new_data);
}

static void
_parser_context_push_toplevel (MechParserContext *context,
                               guint              rule)
{
  ParserStackedRule new_data = { 0 };
  guint toplevel_rule;

  toplevel_rule = _parser_rule_toplevel_create (context->parser, rule);

  new_data.rule = toplevel_rule;
  new_data.current_pos = _mech_parser_context_get_current (context);
  g_array_append_val (context->rule_stack, new_data);
}

static void
_parser_context_pop (MechParserContext *context,
                     guint             *rule,
                     gboolean           advance,
                     gboolean           propagate_error)
{
  const gchar *pos = NULL, *error_pos;
  ParserStackedRule *current_rule;
  GError *error;

  current_rule = _parser_context_peek (context, NULL);
  error_pos = current_rule->error_pos;
  error = current_rule->error;

  if (rule)
    *rule = current_rule->rule;

  if (advance)
    pos = _mech_parser_context_get_current (context);

  g_array_remove_index (context->rule_stack,
                        context->rule_stack->len - 1);

  if (context->rule_stack->len == 0)
    {
      if (error)
        g_error_free (error);
      return;
    }

  if (propagate_error)
    _parser_context_take_error (context, error, error_pos);
  else if (error)
    g_error_free (error);

  if (advance && pos)
    _parser_context_set_current (context, pos);
}

static GPtrArray *
_parser_context_peek_data (MechParserContext *context)
{
  if (context->data_stack->len == 0)
    return NULL;
  return g_ptr_array_index (context->data_stack,
                            context->data_stack->len - 1);
}

static void
_parser_context_data_append_value (MechParserContext *context,
                                   GValue            *value)
{
  GPtrArray *array;
  GValue *copy;

  copy = g_new0 (GValue, 1);
  *copy = *value;

  array = _parser_context_peek_data (context);
  g_ptr_array_add (array, copy);
}

static void
_parser_context_get_data_value (MechParserContext *context,
                                GValue            *value)
{
  GValue *array_value;
  GPtrArray *array;

  array = _parser_context_peek_data (context);
  array_value = g_ptr_array_index (array, array->len - 1);

  if (!G_IS_VALUE (value))
    g_value_init (value, G_VALUE_TYPE (array_value));

  if (G_VALUE_TYPE (value) == G_VALUE_TYPE (array_value) ||
      g_value_type_compatible (G_VALUE_TYPE (array_value),
                               G_VALUE_TYPE (value)))
    g_value_copy (array_value, value);
  else if (g_value_type_transformable (G_VALUE_TYPE (array_value),
                                       G_VALUE_TYPE (value)))
    g_value_transform (array_value, value);
}

static void
_parser_context_data_remove_value (MechParserContext *context)
{
  GPtrArray *array;

  array = _parser_context_peek_data (context);
  g_ptr_array_remove_index (array, array->len - 1);
}

static void
_property_data_free_func (gpointer user_data)
{
  GValue *value = user_data;

  g_value_unset (value);
  g_free (value);
}

static void
_parser_context_push_data (MechParserContext *context)
{
  GPtrArray *array;

  array = g_ptr_array_new_with_free_func (_property_data_free_func);

  if (context->data_stack->len > 0)
    {
      GValue value = { 0 };

      g_value_init (&value, G_TYPE_PTR_ARRAY);
      g_value_set_boxed (&value, array);
      _parser_context_data_append_value (context, &value);
    }

  g_ptr_array_add (context->data_stack, array);
}

static void
_parser_context_pop_data (MechParserContext  *context,
                          GPtrArray         **data,
                          GValue             *value)
{
  GPtrArray *array;

  g_assert (context->data_stack->len > 0);

  array = _parser_context_peek_data (context);

  if (data)
    *data = g_ptr_array_ref (array);

  if (value && (context->data_stack->len == 1 || array->len == 1))
    _parser_context_get_data_value (context, value);

  g_ptr_array_remove_index (context->data_stack,
                            context->data_stack->len - 1);

  if (value && array->len > 1)
    _parser_context_get_data_value (context, value);
}

static void
_parser_context_update_delimiters_string (MechParserContext *context)
{
  GString *string = NULL;
  gint i;

  if (context->delimiters_string)
    {
      g_free (context->delimiters_string);
      context->delimiters_string = NULL;
    }

  if (context->delimiters->len == 0)
    return;

  for (i = context->delimiters->len - 1; i >= 0; i--)
    {
      gunichar ch;

      ch = g_array_index (context->delimiters, guchar, i);

      if (ch == 0)
        break;

      if (!string)
        string = g_string_new ("");

      g_string_append_unichar (string, ch);
    }

  if (string)
    context->delimiters_string = g_string_free (string, FALSE);
}

static void
_parser_context_push_delimiter (MechParserContext *context,
                                guchar             delimiter,
                                gboolean           clear)
{
  g_assert (delimiter != 0);

  if (clear)
    {
      guchar clear_char = 0;
      g_array_append_val (context->delimiters, clear_char);
    }

  g_array_append_val (context->delimiters, delimiter);
  _parser_context_update_delimiters_string (context);
}

static void
_parser_context_pop_delimiter (MechParserContext *context)
{
  g_assert (context->delimiters->len > 0);

  g_array_remove_index (context->delimiters, context->delimiters->len - 1);

  if (context->delimiters->len > 0 &&
      g_array_index (context->delimiters, guchar, context->delimiters->len - 1) == 0)
    g_array_remove_index (context->delimiters, context->delimiters->len - 1);

  _parser_context_update_delimiters_string (context);
}

static MechParserContext *
_parser_context_new (MechParser  *parser,
                     const gchar *data,
                     gssize       len,
                     gpointer     user_data)
{
  MechParserContext *context;

  if (len < 0)
    len = strlen (data);

  context = g_new0 (MechParserContext, 1);
  context->user_data = user_data;
  context->data = data;
  context->data_len = len;
  context->parser = parser;

  context->rule_stack = g_array_new (FALSE, FALSE, sizeof (ParserStackedRule));
  context->delimiters = g_array_new (FALSE, FALSE, sizeof (guchar));
  context->data_stack =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_ptr_array_unref);

  _parser_context_push_data (context);

  return context;
}

static void
_parser_context_free (MechParserContext *context)
{
  g_array_free (context->rule_stack, TRUE);
  g_array_free (context->delimiters, TRUE);
  g_ptr_array_free (context->data_stack, TRUE);
  g_free (context);
}

static gboolean
_char_is_delimiter (const gchar *ptr,
                    const gchar *delimiters,
                    const gchar *context_delimiters)
{
  guint i = 0;

  if (delimiters)
    {
      for (i = 0; delimiters[i]; i++)
        {
          if (*ptr == delimiters[i])
            return TRUE;
        }
    }

  if (context_delimiters)
    {
      for (i = 0; context_delimiters[i]; i++)
        {
          if (*ptr == context_delimiters[i])
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
_advance_while_valid (MechParserContext  *context,
                      gchar             **end,
                      const gchar        *delimiters,
                      gboolean            add_context_delimiters,
                      gboolean            equal)
{
  const gchar *context_delimiters = NULL, *start;

  start = *end = (gchar *) _mech_parser_context_get_current (context);

  if (add_context_delimiters)
    context_delimiters = context->delimiters_string;

  while (**end)
    {
      gboolean match = FALSE;

      match = _char_is_delimiter (*end, delimiters, context_delimiters);

      if (match == equal)
        return TRUE;

      *end = g_utf8_next_char (*end);
    }

  return **end == '\0' && *end != start;
}

static gboolean
_advance_till_delimiter (MechParserContext  *context,
                         gchar             **end,
                         const gchar        *delimiters,
                         gboolean            add_context_delimiters)
{
  return _advance_while_valid (context, end, delimiters,
                               add_context_delimiters, TRUE);
}

static gboolean
_skip_delimiter (MechParserContext  *context,
                 gchar             **end,
                 const gchar        *delimiters,
                 gboolean            add_context_delimiters)
{
  return _advance_while_valid (context, end, delimiters,
                               add_context_delimiters, FALSE);
}

static gboolean
_parse_number (MechParser         *parser,
               MechParserContext  *context,
               gsize              *offset_end,
               GValue             *value,
               GError            **error)
{
  const gchar *data;
  gdouble val;
  gchar *end;

  data = _mech_parser_context_get_current (context);
  val = g_ascii_strtod (data, &end);

  if (data == end)
    return FALSE;

  if (G_VALUE_TYPE (value) == G_TYPE_INT)
    g_value_set_int (value, (int) val);
  else if (G_VALUE_TYPE (value) == G_TYPE_DOUBLE)
    g_value_set_double (value, val);

  *offset_end = end - data;

  return TRUE;
}

static gboolean
_parse_enum (MechParser         *parser,
             MechParserContext  *context,
             gsize              *offset_end,
             GValue             *value,
             GError            **error)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  const gchar *data;
  gchar *str, *end;

  if (!g_type_is_a (G_VALUE_TYPE (value), G_TYPE_ENUM))
    return FALSE;

  if (!_advance_till_delimiter (context, &end, " \r\n", TRUE))
    return FALSE;

  data = _mech_parser_context_get_current (context);

  if (end == data)
    return FALSE;

  str = g_strndup (data, end - data);
  enum_class = g_type_class_ref (G_VALUE_TYPE (value));
  enum_value = g_enum_get_value_by_nick (enum_class, str);

  if (!enum_value)
    {
      g_warning ("Invalid value '%s' for enum type '%s'",
                 str, G_VALUE_TYPE_NAME (value));
      g_free (str);
      return FALSE;
    }

  g_value_set_enum (value, enum_value->value);
  g_type_class_unref (enum_class);
  g_free (str);

  *offset_end = end - data;

  return TRUE;
}

static gboolean
_parse_flags (MechParser         *parser,
              MechParserContext  *context,
              gsize              *offset_end,
              GValue             *value,
              GError            **error)
{
  GFlagsClass *flags_class;
  GFlagsValue *flag_value;
  const gchar *data, *cur;
  gchar *str, *end;
  guint flags= 0;

  if (!g_type_is_a (G_VALUE_TYPE (value), G_TYPE_FLAGS))
    return FALSE;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));
  data = cur = _mech_parser_context_get_current (context);

  while (TRUE)
    {
      if (!_advance_till_delimiter (context, &end, ", \r\n", TRUE))
        return FALSE;

      str = g_strndup (cur, end - cur);
      flag_value = g_flags_get_value_by_nick (flags_class, str);

      if (!flag_value)
        {
          g_warning ("Invalid value '%s' for flags type '%s'",
                     str, G_VALUE_TYPE_NAME (value));
          g_free (str);
          return FALSE;
        }

      flags |= flag_value->value;
      g_free (str);

      if (end[0] != ',')
        break;

      cur = end + 1;
      _parser_context_set_current (context, cur);
    }

  g_value_set_flags (value, flags);
  g_type_class_unref (flags_class);
  *offset_end = end - data;

  return TRUE;
}

static gboolean
_parse_string (MechParser         *parser,
               MechParserContext  *context,
               gsize              *offset_end,
               GValue             *value,
               GError            **error)
{
  const gchar *data;
  gchar *str, *end;
  gunichar ch;

#define CHAR_VALID(c)                        \
  ((c) == '#' || (c) == '-' || (c) == '_' || \
   g_unichar_isalnum ((c)))

  data = _mech_parser_context_get_current (context);
  end = (gchar *) data;
  *offset_end = 0;

  if (data[0] == '\0')
    return FALSE;

  if (data[0] == '"' || data[0] == '\'')
    {
      gchar delimiter[2] = { data[0], 0 };

      _parser_context_set_current (context, data + 1);

      if (!_advance_till_delimiter (context, &end,
                                    delimiter, TRUE) || end[0] != data[0])
        return FALSE;

      str = g_strndup (data + 1, end - data - 1);
      end++;
    }
  else
    {
      while (end && *end)
        {
          ch = g_utf8_get_char (end);

          if (!CHAR_VALID (ch))
            break;

          end = g_utf8_next_char (end);
        }

      str = g_strndup (data, end - data);
    }

  if (end == data)
    {
      g_free (str);
      return FALSE;
    }

  g_value_take_string (value, str);
  *offset_end = end - data;

#undef CHAR_VALID

  return TRUE;
}

static guint
_mech_parser_rule_literal_char (MechParser *parser,
                                guchar      ch)
{
  gchar buf[6] = { 0 };

  g_unichar_to_utf8 (ch, (gchar *) &buf);
  return _mech_parser_rule_literal_create (parser, (gchar *) &buf);
}

static void
mech_parser_init (MechParser *parser)
{
  MechParserPrivate *priv;

  priv = mech_parser_get_instance_private (parser);
  priv->rules = g_array_new (FALSE, FALSE, sizeof (ParserRule));
  priv->type_parsers = g_hash_table_new (NULL, NULL);
  priv->types = g_hash_table_new (NULL, NULL);
  priv->toplevel_containers = g_hash_table_new (NULL, NULL);
  priv->literals = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          (GDestroyNotify) g_free,
                                          NULL);

  _mech_parser_register_type_parser (parser, G_TYPE_INT, _parse_number);
  _mech_parser_register_type_parser (parser, G_TYPE_DOUBLE, _parse_number);
  _mech_parser_register_type_parser (parser, G_TYPE_ENUM, _parse_enum);
  _mech_parser_register_type_parser (parser, G_TYPE_FLAGS, _parse_flags);
  _mech_parser_register_type_parser (parser, G_TYPE_STRING, _parse_string);
}

void
_mech_parser_register_type_parser (MechParser     *parser,
                                   GType           type,
                                   MechParserFunc  func)
{
  MechParserPrivate *priv;
  gpointer prev;

  g_return_if_fail (MECH_IS_PARSER (parser));
  g_return_if_fail (type != G_TYPE_NONE);
  g_return_if_fail (func != NULL);

  priv = mech_parser_get_instance_private (parser);
  prev = g_hash_table_lookup (priv->type_parsers, GSIZE_TO_POINTER (type));

  if (prev)
    {
      g_warning ("There is already a registered parser for type '%s'",
                 g_type_name (type));
      return;
    }

  g_hash_table_insert (priv->type_parsers, GSIZE_TO_POINTER (type), func);
}

MechParserFunc
_mech_parser_lookup_type_parser (MechParser *parser,
                                 GType       type)
{
  MechParserPrivate *priv;
  gpointer func;

  priv = mech_parser_get_instance_private (parser);

  for (; type != 0; type = g_type_parent (type))
    {
      func = g_hash_table_lookup (priv->type_parsers, GSIZE_TO_POINTER (type));
      if (func)
        return func;
    }

  return NULL;
}

guint
_mech_parser_rule_type_create (MechParser *parser,
                               GType       type)
{
  MechParserPrivate *priv;
  ParserRule rule = { 0 };
  gpointer prev;

  g_return_val_if_fail (MECH_IS_PARSER (parser), 0);
  g_return_val_if_fail (type != G_TYPE_NONE && type != G_TYPE_INVALID, 0);

  priv = mech_parser_get_instance_private (parser);

  if (g_hash_table_lookup_extended (priv->types, GSIZE_TO_POINTER (type),
                                    NULL, &prev))
    return GPOINTER_TO_UINT (prev);

  rule.type = RULE_BASIC;
  rule.data.basic.gtype = type;
  g_array_append_val (priv->rules, rule);

  g_hash_table_insert (priv->types, GSIZE_TO_POINTER (type),
                       GUINT_TO_POINTER (priv->rules->len));

  return priv->rules->len;
}

guint
_mech_parser_rule_converter_create (MechParser            *parser,
                                    GType                  type,
                                    MechParserConvertFunc  func,
                                    guint                  child_rule)
{
  MechParserPrivate *priv;
  ParserRule rule = { 0 };
  gpointer prev;

  g_return_val_if_fail (MECH_IS_PARSER (parser), 0);
  g_return_val_if_fail (type != G_TYPE_INVALID, 0);
  g_return_val_if_fail (child_rule != 0, 0);
  g_return_val_if_fail (func != NULL, 0);

  priv = mech_parser_get_instance_private (parser);

  if (g_hash_table_lookup_extended (priv->types, GSIZE_TO_POINTER (type),
                                    NULL, &prev))
    return GPOINTER_TO_UINT (prev);

  rule.type = RULE_CONVERTER;
  rule.child_rules = g_array_new (FALSE, FALSE, sizeof (guint));
  g_array_append_val (rule.child_rules, child_rule);
  rule.data.converter.func = func;
  rule.data.converter.gtype = type;
  g_array_append_val (priv->rules, rule);

  g_hash_table_insert (priv->types, GSIZE_TO_POINTER (type),
                       GUINT_TO_POINTER (priv->rules->len));

  return priv->rules->len;
}

guint
_mech_parser_rule_literal_create (MechParser  *parser,
                                  const gchar *string)
{
  MechParserPrivate *priv;
  ParserRule rule = { 0 };
  gpointer prev;
  gchar *copy;

  g_return_val_if_fail (MECH_IS_PARSER (parser), 0);
  g_return_val_if_fail (string && *string, 0);

  priv = mech_parser_get_instance_private (parser);

  if (g_hash_table_lookup_extended (priv->literals, string, NULL, &prev))
    return GPOINTER_TO_UINT (prev);

  copy = g_strdup (string);

  rule.type = RULE_LITERAL;
  rule.data.literal.str = copy;
  rule.data.literal.len = strlen (copy);
  g_array_append_val (priv->rules, rule);

  g_hash_table_insert (priv->literals, copy,
                       GUINT_TO_POINTER (priv->rules->len));

  return priv->rules->len;
}

guint
_mech_parser_rule_compound_create (MechParser *parser,
                                   guint      *rules,
                                   guint       n_rules)
{
  MechParserPrivate *priv;
  ParserRule rule = { 0 };

  g_return_val_if_fail (MECH_IS_PARSER (parser), 0);
  g_return_val_if_fail (rules != NULL, 0);

  priv = mech_parser_get_instance_private (parser);

  rule.type = RULE_COMPOUND;
  rule.child_rules = g_array_new (FALSE, FALSE, sizeof (guint));
  g_array_insert_vals (rule.child_rules, 0, rules, n_rules);
  g_array_append_val (priv->rules, rule);

  return priv->rules->len;
}

void
_mech_parser_set_root_rule (MechParser *parser,
                            guint       rule)
{
  MechParserPrivate *priv;

  g_return_if_fail (MECH_IS_PARSER (parser));
  g_return_if_fail (rule != 0);

  priv = mech_parser_get_instance_private (parser);
  g_return_if_fail (IS_VALID_ID (priv->rules, rule));

  priv->root_rule = rule;
}

static gboolean
_parser_context_next_child_rule (MechParserContext *context,
                                 guint             *next)
{
  ParserStackedRule *stack_data;
  ParserRule *rule;
  guint pos;

  stack_data = _parser_context_peek (context, &rule);

  if (!rule->child_rules ||
      rule->child_rules->len == 0)
    return FALSE;

  if (stack_data->finished)
    return FALSE;
  else if (!stack_data->started)
    {
      stack_data->n_iterations++;
      pos = 0;
    }
  else
    {
      pos = stack_data->current_child + 1;

      if (pos >= rule->child_rules->len)
        {
          stack_data->finished = TRUE;
          return FALSE;
        }
    }

  *next = pos;
  return TRUE;
}

static gboolean
_parser_context_visit_rule (MechParserContext *context,
                            ParserRule        *rule)
{
  ParserStackedRule *stack_data;
  GValue value = { 0 };
  gboolean retval = TRUE;
  gsize offset_end = 0;
  GError *error = NULL;
  const gchar *cur;
  GPtrArray *data;

  data = _parser_context_peek_data (context);
  cur = _mech_parser_context_get_current (context);

  switch (rule->type)
    {
    case RULE_BASIC:
      {
        MechParserFunc parser_func;

        parser_func = _mech_parser_lookup_type_parser (context->parser,
                                                       rule->data.basic.gtype);
        g_value_init (&value, rule->data.basic.gtype);

        retval = parser_func (context->parser, context,
                              &offset_end, &value, &error);
        if (retval)
          {
            _parser_context_data_append_value (context, &value);
            cur += offset_end;
          }
        else if (!error)
          error = g_error_new (quark_parser_error, MECH_PARSER_ERROR_FAILED,
                               "Failed to parse type '%s'",
                               g_type_name (rule->data.basic.gtype));
        break;
      }
    case RULE_LITERAL:
      if (strncmp (_mech_parser_context_get_current (context),
                   rule->data.literal.str, rule->data.literal.len) == 0)
        cur += rule->data.literal.len;
      else
        error = g_error_new (quark_parser_error, MECH_PARSER_ERROR_FAILED,
                             "Failed to parse literal '%s'",
                             rule->data.literal.str);
      break;
    default:
      break;
    }

  _parser_context_set_current (context, cur);

  if (error)
    retval = FALSE;
  else if (!retval)
    error = g_error_new (quark_parser_error, MECH_PARSER_ERROR_FAILED,
                          "Unknown parsing error");

  if (error)
    _parser_context_take_error (context, error, NULL);

  return retval;
}

static gboolean
_parser_context_check_finished (MechParserContext *context)
{
  ParserStackedRule *stack_data;

  stack_data = _parser_context_peek (context, NULL);

  if (!stack_data->started)
    return FALSE;

  return stack_data->finished;
}

static gboolean
_parser_context_apply_current_rule (MechParserContext  *context)
{
  gboolean retval = TRUE, fatal_error = FALSE, finished;
  ParserStackedRule *current_rule;
  ParserRule *rule;
  guint next_rule;

  current_rule = _parser_context_peek (context, &rule);

  /* This function may unset the context error */
  finished = _parser_context_check_finished (context);

  if (_parser_context_peek_error (context, NULL, NULL))
    retval = FALSE;
  else if (!finished)
    retval = _parser_context_visit_rule (context, rule);

  if (retval && _parser_context_next_child_rule (context, &next_rule))
    _parser_context_push (context, next_rule);
  else
    {
      /* Refetch the rule as visit() could have changed the address,
       * say a custom rule creating new child rules and
       * expanding the array
       */
      _parser_context_peek (context, &rule);

      /* There has been an error, a last iteration
       * on the rule, or the rule is not a container
       */
      _parser_context_pop (context, NULL, retval, TRUE);
    }

  return TRUE;
}

static gboolean
_parser_context_process_toplevel (MechParserContext *context)
{
  ParserRule *current_rule;
  guint level;
  gchar *cur;

  _parser_context_peek (context, &current_rule);
  g_assert (current_rule->type == RULE_TOPLEVEL);

  /* Push and process the only child of this rule */
  _parser_context_push (context, 0);
  level = context->rule_stack->len;

  while (context->rule_stack->len >= level)
    {
      _skip_delimiter (context, &cur, " \r\n", FALSE);
      _parser_context_set_current (context, cur);

      if (!_parser_context_apply_current_rule (context))
        return FALSE;
    }

  return TRUE;
}

static gboolean
_parser_context_run_rule (MechParserContext  *context,
                          guint               rule_id,
                          gboolean            test,
                          GValue             *value,
                          gsize              *end_offset,
                          GError            **error)
{
  const gchar *prev;
  gboolean retval;

  prev = _mech_parser_context_get_current (context);
  _parser_context_push_data (context);

  _parser_context_push_toplevel (context, rule_id);
  retval = _parser_context_process_toplevel (context);

  if (end_offset)
    *end_offset = _mech_parser_context_get_current (context) - prev;

  if (_parser_context_peek_error (context, error, NULL))
    retval = FALSE;

  _parser_context_pop (context, NULL, !test, !test);
  _parser_context_pop_data (context, NULL, value);
  _parser_context_data_remove_value (context);

  return retval;
}

gboolean
_mech_parser_context_test_rule (MechParserContext *context,
                                guint              rule_id,
                                gsize             *end_offset)
{
  MechParserPrivate *priv;

  g_return_val_if_fail (context != NULL &&
                        MECH_IS_PARSER (context->parser), FALSE);

  priv = mech_parser_get_instance_private (context->parser);
  g_return_val_if_fail (rule_id > 0 && rule_id <= priv->rules->len, FALSE);

  return _parser_context_run_rule (context, rule_id, TRUE, NULL,
                                   end_offset, NULL);
}

gboolean
_mech_parser_context_apply_rule (MechParserContext  *context,
                                 guint               rule_id,
                                 GValue             *value,
                                 GError            **error)
{
  MechParserPrivate *priv;

  g_return_val_if_fail (context != NULL && MECH_IS_PARSER (context->parser), FALSE);
  g_return_val_if_fail (!error || !*error, FALSE);

  priv = mech_parser_get_instance_private (context->parser);
  g_return_val_if_fail (rule_id > 0 && rule_id <= priv->rules->len, FALSE);

  return _parser_context_run_rule (context, rule_id, FALSE, value, NULL, error);
}

gboolean
_mech_parser_context_apply (MechParserContext  *context,
                            guint               rule_id,
                            GError            **error,
                            ...)
{
  GValue value = { 0 };
  gchar *err = NULL;
  va_list varargs;

  if (!_parser_context_run_rule (context, rule_id, FALSE,
                                 &value, NULL, error))
    return FALSE;

  if (!G_IS_VALUE (&value))
    return TRUE;

  va_start (varargs, error);

  if (G_VALUE_HOLDS (&value, G_TYPE_PTR_ARRAY))
    {
      GPtrArray *array;
      guint i;

      array = g_value_get_boxed (&value);

      for (i = 0; i < array->len; i++)
        {
          G_VALUE_LCOPY (g_ptr_array_index (array, i),
                         varargs, 0, &err);
          if (err)
            break;
        }
    }
  else
    G_VALUE_LCOPY (&value, varargs, 0, &err);

  g_value_unset (&value);
  va_end (varargs);

  if (err)
    {
      g_set_error_literal (error, quark_parser_error,
                           MECH_PARSER_ERROR_FAILED,
                           err);
      g_free (err);
      return FALSE;
    }

  return TRUE;
}

gboolean
_mech_parser_load_from_data (MechParser   *parser,
                             const gchar  *data,
                             gssize        len,
                             gpointer      user_data,
                             GError      **error)
{
  MechParserContext *context;
  MechParserPrivate *priv;
  gsize end_offset;
  gboolean retval;

  g_return_val_if_fail (MECH_IS_PARSER (parser), 0);
  g_return_val_if_fail (data != NULL, 0);

  priv = mech_parser_get_instance_private (parser);
  context = _parser_context_new (parser, data, len, user_data);
  retval = _parser_context_run_rule (context, priv->root_rule,
                                     FALSE, NULL, &end_offset, error);

  if (retval && end_offset != context->data_len)
    {
      g_set_error (error, quark_parser_error,
                   MECH_PARSER_ERROR_FAILED,
                   "Not all data was handled by the parser");
      retval = FALSE;
    }

  _parser_context_free (context);

  return retval;
}

gboolean
_mech_parser_load_from_file (MechParser  *parser,
                             GFile       *file,
                             gpointer     user_data,
                             GError     **error)
{
  gboolean retval;
  gchar *contents;
  gsize len;

  g_return_val_if_fail (MECH_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (!g_file_load_contents (file, NULL, &contents,
                             &len, NULL, error))
    return FALSE;

  retval = _mech_parser_load_from_data (parser, contents, len,
                                        user_data, error);
  g_free (contents);

  return retval;
}

MechParser *
_mech_parser_new (void)
{
  return g_object_new (MECH_TYPE_PARSER, NULL);
}
