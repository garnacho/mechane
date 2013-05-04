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

#ifndef __MECH_PARSER_PRIVATE_H__
#define __MECH_PARSER_PRIVATE_H__

#include <stdarg.h>
#include <gio/gio.h>
#include <pango/pango.h>
#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_PARSER         (mech_parser_get_type ())
#define MECH_PARSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_PARSER, MechParser))
#define MECH_PARSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_PARSER, MechParserClass))
#define MECH_IS_PARSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_PARSER))
#define MECH_IS_PARSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_PARSER))
#define MECH_PARSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_PARSER, MechParserClass))

typedef struct _MechParser MechParser;
typedef struct _MechParserClass MechParserClass;
typedef struct _MechParserContext MechParserContext;

typedef gboolean (* MechParserFunc)        (MechParser         *parser,
                                            MechParserContext  *context,
                                            gsize              *offset_end,
                                            GValue             *value,
                                            GError            **error);
typedef gboolean (* MechParserConvertFunc) (MechParser         *parser,
                                            MechParserContext  *context,
                                            GPtrArray          *members,
                                            GValue             *value,
                                            GError            **error);
typedef gboolean (* MechParserCustomFunc)  (MechParser         *parser,
                                            MechParserContext  *context,
                                            GError            **error);

struct _MechParser
{
  GObject parent_instance;
};

struct _MechParserClass
{
  GObjectClass parent_class;

  gboolean (* error) (MechParser   *parser,
                      const GError *error);
};

/* Context */
gpointer      _mech_parser_context_get_user_data (MechParserContext  *context);
const gchar * _mech_parser_context_get_current   (MechParserContext  *context);
gboolean      _mech_parser_context_test_rule     (MechParserContext  *context,
                                                  guint               rule,
                                                  gsize              *end_offset);
gboolean      _mech_parser_context_apply_rule    (MechParserContext  *context,
                                                  guint               rule,
                                                  GValue             *value,
                                                  GError            **error);
gboolean      _mech_parser_context_apply         (MechParserContext  *context,
                                                  guint               rule,
                                                  GError            **error,
                                                  ...);

/* Parser */
GType         _mech_parser_get_type              (void) G_GNUC_CONST;

MechParser *  _mech_parser_new                   (void);

void          _mech_parser_set_root_rule         (MechParser         *parser,
                                                  guint               rule);

gboolean      _mech_parser_load_from_data        (MechParser         *parser,
                                                  const gchar        *data,
                                                  gssize              len,
                                                  gpointer            user_data,
                                                  GError            **error);
gboolean      _mech_parser_load_from_file        (MechParser         *parser,
                                                  GFile              *file,
                                                  gpointer            user_data,
                                                  GError            **error);

/* GType basic parsing */
void     _mech_parser_register_type_parser  (MechParser            *parser,
                                             GType                  type,
                                             MechParserFunc         func);
guint    _mech_parser_lookup_type_rule      (MechParser            *parser,
                                             GType                  type);

/* Parser rules */
guint    _mech_parser_rule_type_create      (MechParser            *parser,
                                             GType                  type);
guint    _mech_parser_rule_converter_create (MechParser            *parser,
                                             GType                  type,
                                             MechParserConvertFunc  func,
                                             guint                  child_rule);
guint    _mech_parser_rule_literal_create   (MechParser            *parser,
                                             const gchar           *string);
guint    _mech_parser_rule_compound_create  (MechParser            *parser,
                                             guint                 *rules,
                                             guint                  n_rules);
guint    _mech_parser_rule_or_create        (MechParser            *parser,
                                             guint                 *rules,
                                             guint                  n_rules);
guint    _mech_parser_rule_multi_create     (MechParser            *parser,
                                             guint                  child_rule,
                                             guchar                 separator,
                                             gint                   min,
                                             gint                   max);
guint    _mech_parser_rule_container_create (MechParser            *parser,
                                             guint                  child,
                                             guchar                 open_container,
                                             guchar                 close_container);
guint    _mech_parser_rule_optional_create  (MechParser            *parser,
                                             guint                  child);
guint    _mech_parser_rule_custom_create    (MechParser            *parser,
                                             MechParserCustomFunc   func);

G_END_DECLS

#endif /* __MECH_PARSER_PRIVATE_H__ */
