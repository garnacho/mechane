/* Mechane:
 * Copyright (C) 2012 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __MECH_STAGE_H__
#define __MECH_STAGE_H__

#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_STAGE  (mech_stage_get_type ())
#define MECH_STAGE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_STAGE, MechStage))
#define MECH_IS_STAGE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_STAGE))

typedef struct _MechStage MechStage;
typedef struct _MechStageClass MechStageClass;

struct _MechStage
{
  GObject parent_object;
};

struct _MechStageClass
{
  GObjectClass parent_class;
};

GType       _mech_stage_get_type            (void) G_GNUC_CONST;
MechStage * _mech_stage_new                 (void);

void        _mech_stage_get_size            (MechStage       *stage,
                                             gint            *width,
                                             gint            *height);

gboolean    _mech_stage_set_size            (MechStage       *stage,
                                             gint            *width,
                                             gint            *height);

GNode     * _mech_stage_node_new            (MechArea        *area);
void        _mech_stage_node_free           (GNode           *node);

void        _mech_stage_set_root            (MechStage       *stage,
                                             MechArea        *area);

gboolean    _mech_stage_add                 (GNode           *parent_node,
                                             GNode           *child_node);
gboolean    _mech_stage_remove              (GNode           *child_node);


G_END_DECLS

#endif /* __MECH_STAGE_H__ */
