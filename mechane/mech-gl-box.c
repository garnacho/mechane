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

/* Unproject code taken from GLU, licensed as:
 *
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */

#include <math.h>
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cairo-gl.h>
#include <mechane/mech-gl-box.h>
#include <mechane/mech-stage-private.h>

#define NEAR_PLANE_DISTANCE 11.0

static const gchar *vertex_shader =
  "#version 130\n"
  "in vec2 tex_coord;\n"
  "out vec2 v_tex_coord;\n"
  "void main () {\n"
  "  gl_Position = ftransform ();\n"
  "  v_tex_coord = tex_coord.st;\n"
  "}\n";

static const gchar *fragment_shader =
  "#version 130\n"
  "uniform sampler2D child_tex;\n"
  "uniform float child_id;\n"
  "in vec2 v_tex_coord;\n"
  "void main () {\n"
  "  vec4 color;\n"
  "  color.rgba = texture (child_tex, v_tex_coord.st).rgba;\n"
  "  gl_FragColor = color;\n"
  "}";

static const gchar *picking_fragment_shader =
  "#version 130\n"
  "uniform sampler2D child_tex;\n"
  "uniform float child_id;\n"
  "in vec2 v_tex_coord;\n"
  "void main () {\n"
  "  vec4 color;\n"
  "  color.rgba = vec4 (child_id, v_tex_coord.s, 1 - v_tex_coord.t, 1);\n"
  "  gl_FragColor = color;\n"
  "}";

typedef struct _MechGLBoxPrivate MechGLBoxPrivate;
typedef struct _ChildData ChildData;

#define VERTEX_ARRAY    0
#define TEX_COORD_ARRAY 1

struct _ChildData
{
  GLuint vbo;
  GLuint tex_vertices;
  guint16 id;
};

enum {
  RBO_PICKDATA,
  RBO_DEPTH,
  N_RBOS
};

struct _MechGLBoxPrivate
{
  GHashTable *children;
  GHashTable *children_by_id;

  guint16 child_ids;

  /* Rendering */
  GLuint program_id;
  GLuint vertex_shader_id;
  GLuint fragment_shader_id;

  /* Picking */
  GLuint picking_program_id;
  GLuint picking_fragment_shader_id;
  GLuint picking_fbo;
  GLuint picking_rbos[N_RBOS];
};

enum {
  POSITION_CHILD,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MechGLBox, mech_gl_box, MECH_TYPE_GL_VIEW)

static void
mech_gl_box_finalize (GObject *object)
{
  MechGLBoxPrivate *priv;

  priv = mech_gl_box_get_instance_private ((MechGLBox *) object);
  g_hash_table_unref (priv->children);
  g_hash_table_unref (priv->children_by_id);

  if (priv->picking_rbos[RBO_DEPTH] &&
      priv->picking_rbos[RBO_PICKDATA])
    glDeleteRenderbuffers (N_RBOS, &priv->picking_rbos);

  if (priv->picking_fbo)
    glDeleteFramebuffers (1, &priv->picking_fbo);

  if (priv->program_id)
    glDeleteProgram (priv->program_id);

  if (priv->picking_program_id)
    glDeleteProgram (priv->picking_program_id);

  G_OBJECT_CLASS (mech_gl_box_parent_class)->finalize (object);
}

static void
_mech_gl_box_update_child_vbo (ChildData         *data,
                               cairo_rectangle_t  allocation)
{
  GLfloat triangles_data[] = {
    0, allocation.height, 0,
    0, 0, 0,
    allocation.width, allocation.height, 0,
    allocation.width, 0, 0
  };
  GLfloat tex_vertices[] = {
    0, 1,
    0, 0,
    1, 1,
    1, 0
  };

  if (data->vbo)
    glDeleteBuffers (1, &data->vbo);

  glGenBuffers (1, &data->vbo);
  glBindBuffer (GL_ARRAY_BUFFER, data->vbo);
  glBufferData (GL_ARRAY_BUFFER, sizeof (triangles_data),
                triangles_data, GL_STATIC_DRAW);

  glGenBuffers (1, &data->tex_vertices);
  glBindBuffer (GL_ARRAY_BUFFER, data->tex_vertices);
  glBufferData (GL_ARRAY_BUFFER, sizeof (tex_vertices),
                tex_vertices, GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
}

static void
_mech_gl_box_ensure_child_vbo (ChildData         *data,
                               cairo_rectangle_t  allocation)
{
  if (!data->vbo)
    _mech_gl_box_update_child_vbo (data, allocation);
}

static ChildData *
_mech_gl_box_lookup_child_data (MechGLBox *box,
                                MechArea  *child)
{
  MechGLBoxPrivate *priv;
  ChildData *data;

  priv = mech_gl_box_get_instance_private (box);
  data = g_hash_table_lookup (priv->children, child);

  if (G_UNLIKELY (!data))
    {
      data = g_new0 (ChildData, 1);
      priv->child_ids = 200;
      data->id = ++priv->child_ids;
      g_hash_table_insert (priv->children, child, data);
      g_hash_table_insert (priv->children_by_id,
                           GUINT_TO_POINTER (data->id),
                           child);
    }

  return data;
}

static GLuint
compile_shader (GLuint       type,
                const gchar *str)
{
  GLuint shader, status;

  shader = glCreateShader (type);

  if (shader == 0)
    return 0;

  glShaderSource (shader, 1, &str, NULL);
  glCompileShader (shader);

  glGetShaderiv (shader, GL_COMPILE_STATUS, &status);

  if (!status)
    {
      char log[200];
      GLsizei len;

      glGetShaderInfoLog (shader, sizeof (log), NULL, log);
      g_warning ("Error compiling %s shader: %s",
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                 log);

      glDeleteShader (shader);
      shader = 0;
    }

  return shader;
}

static gboolean
link_program (MechGLBox *box,
              GLuint     program_id)
{
  MechGLBoxPrivate *priv;
  GLuint status;

  priv = mech_gl_box_get_instance_private (box);

  glLinkProgram (program_id);
  glGetProgramiv (program_id, GL_LINK_STATUS, &status);

  if (!status)
    {
      char log[200];

      glGetProgramInfoLog (program_id, sizeof (log), NULL, log);
      g_warning ("Error linking GLSL program: %s", log);
      return FALSE;
    }

  glBindAttribLocation (program_id, TEX_COORD_ARRAY, "tex_coord");

  return TRUE;
}

static GLuint
compile_program (MechGLBox *box,
                 GLuint     vertex_shader_id,
                 GLuint     fragment_shader_id)
{
  MechGLBoxPrivate *priv;
  GLuint program_id;

  if (!vertex_shader_id || !fragment_shader_id)
    return 0;

  priv = mech_gl_box_get_instance_private (box);
  program_id = glCreateProgram ();
  glAttachShader (program_id, vertex_shader_id);
  glAttachShader (program_id, fragment_shader_id);

  if (!link_program (box, program_id))
    {
      glDeleteProgram (program_id);
      program_id = 0;
    }

  return program_id;
}

static gboolean
_mech_gl_box_ensure_shaders (MechGLBox *box)
{
  MechGLBoxPrivate *priv;

  priv = mech_gl_box_get_instance_private (box);

  if (priv->program_id != 0)
    return TRUE;
  else if (priv->vertex_shader_id != 0 || priv->fragment_shader_id != 0)
    return FALSE;

  priv->vertex_shader_id = compile_shader (GL_VERTEX_SHADER, vertex_shader);
  priv->fragment_shader_id = compile_shader (GL_FRAGMENT_SHADER,
                                             fragment_shader);
  priv->picking_fragment_shader_id = compile_shader (GL_FRAGMENT_SHADER,
                                                     picking_fragment_shader);

  priv->program_id = compile_program (box, priv->vertex_shader_id,
                                      priv->fragment_shader_id);
  priv->picking_program_id = compile_program (box, priv->vertex_shader_id,
                                              priv->picking_fragment_shader_id);

  return priv->program_id != 0 && priv->picking_program_id != 0;
}

static gboolean
_mech_gl_box_ensure_picking (MechGLBox *box)
{
  MechGLBoxPrivate *priv;
  GLuint status;

  priv = mech_gl_box_get_instance_private (box);

  if (priv->picking_fbo != 0)
    return TRUE;

  glGenFramebuffers (1, &priv->picking_fbo);
  glBindFramebuffer (GL_FRAMEBUFFER, priv->picking_fbo);

  /* Create 1x1 render buffers for the pointer position */
  glGenRenderbuffers (N_RBOS, &priv->picking_rbos);

  glBindRenderbuffer (GL_RENDERBUFFER, priv->picking_rbos[RBO_PICKDATA]);
  glRenderbufferStorage (GL_RENDERBUFFER, GL_RGBA16, 1, 1);

  glBindRenderbuffer (GL_RENDERBUFFER, priv->picking_rbos[RBO_DEPTH]);
  glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 1, 1);
  glBindRenderbuffer (GL_RENDERBUFFER, 0);

  glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_RENDERBUFFER, priv->picking_rbos[RBO_PICKDATA]);
  glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                             GL_RENDERBUFFER, priv->picking_rbos[RBO_DEPTH]);

  status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
  glBindFramebuffer (GL_FRAMEBUFFER, 0);

  if (priv->picking_program_id != 0 &&
      status != GL_FRAMEBUFFER_COMPLETE)
    {
      g_warning ("Could not create FBO for picking, status: %x", status);
      glDeleteRenderbuffers (N_RBOS, &priv->picking_rbos);
      glDeleteFramebuffers (1, &priv->picking_fbo);
      return FALSE;
    }

  return TRUE;
}

static void
_apply_uniforms (GLuint program_id,
                 GLuint texture_id,
                 guint  id)
{
  GLfloat float_id;
  GLint loc;

  loc = glGetUniformLocation (program_id, "child_tex");
  glUniform1i (loc, texture_id);

  float_id = (GLfloat) id / G_MAXUINT16;
  loc = glGetUniformLocation (program_id, "child_id");
  glUniform1fv (loc, 1, &float_id);
}

static void
_mech_gl_box_render_children (MechGLBox *box,
                              GLuint     program_id)
{
  cairo_rectangle_t allocation;
  MechGLBoxPrivate *priv;
  MechArea **children;
  gint i, n_children;
  ChildData *data;

  priv = mech_gl_box_get_instance_private (box);
  n_children = mech_area_get_children ((MechArea *) box, &children);
  mech_area_get_allocated_size ((MechArea *) box, &allocation);

  glDepthFunc (GL_ALWAYS);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum (-allocation.width / 2, allocation.width / 2,
             -allocation.height / 2, allocation.height / 2,
             NEAR_PLANE_DISTANCE - 0.001, 200);
  glMatrixMode(GL_MODELVIEW);

  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  for (i = 0; i < n_children; i++)
    {
      data = _mech_gl_box_lookup_child_data ((MechGLBox *) box, children[i]);
      mech_area_get_allocated_size (children[i], &allocation);

      _mech_gl_box_ensure_child_vbo (data, allocation);

      glActiveTexture (GL_TEXTURE0);

      if (!mech_gl_view_bind_child_texture ((MechGLView *) box, children[i]))
        continue;

      glPushMatrix ();
      g_signal_emit (box, signals[POSITION_CHILD], 0, children[i]);
      glTranslatef (-allocation.width / 2, -allocation.height / 2,
                    -NEAR_PLANE_DISTANCE);

      _apply_uniforms (program_id, 0, data->id);

      glEnableVertexAttribArray (TEX_COORD_ARRAY);
      glBindBuffer (GL_ARRAY_BUFFER, data->tex_vertices);
      glVertexAttribPointer (TEX_COORD_ARRAY, 2, GL_FLOAT,
                             GL_FALSE, 0, (void *) 0);

      glEnableVertexAttribArray (VERTEX_ARRAY);
      glBindBuffer (GL_ARRAY_BUFFER, data->vbo);
      glVertexAttribPointer (VERTEX_ARRAY, 3, GL_FLOAT,
                             GL_FALSE, 0, (void *) 0);

      glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

      glBindBuffer (GL_ARRAY_BUFFER, 0);
      glDisableVertexAttribArray (TEX_COORD_ARRAY);
      glDisableVertexAttribArray (VERTEX_ARRAY);
      glBindTexture (GL_TEXTURE_2D, 0);

      glPopMatrix ();
    }

  glDisable (GL_BLEND);

  g_free (children);
}

static void
mech_gl_box_update_pick_buffer (MechGLBox *box,
                                gdouble    x,
                                gdouble    y)
{
  cairo_rectangle_t allocation;
  MechGLBoxPrivate *priv;
  MechArea **children;
  gint i, n_children;

  priv = mech_gl_box_get_instance_private (box);
  n_children = mech_area_get_children ((MechArea *) box, &children);
  mech_area_get_allocated_size ((MechArea *) box, &allocation);

  if (!_mech_gl_box_ensure_picking (box))
    return;

  if (!_mech_gl_box_ensure_shaders (box))
    return;

  glBindFramebuffer (GL_FRAMEBUFFER, priv->picking_fbo);
  glUseProgram (priv->picking_program_id);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* Update viewport to have the single picking renderbuffer
   * pixel pointing to the current pointer position.
   */
  glViewport (-x, - (allocation.height - y), allocation.width, allocation.height);
  glScissor (0, 0, 1, 1);

  _mech_gl_box_render_children (box, priv->picking_program_id);

  glUseProgram (0);
  glBindFramebuffer (GL_FRAMEBUFFER, 0);
}

static void
mech_gl_box_render_scene (MechGLView     *view,
                          cairo_device_t *device)
{
  MechGLBoxPrivate *priv;
  MechArea **children;
  gint i, n_children;

  priv = mech_gl_box_get_instance_private ((MechGLBox *) view);
  n_children = mech_area_get_children ((MechArea *) view, &children);

  if (!_mech_gl_box_ensure_shaders ((MechGLBox *) view))
    return;

  glUseProgram (priv->program_id);
  _mech_gl_box_render_children ((MechGLBox *) view, priv->program_id);
  glUseProgram (0);

  g_free (children);
}

static MechArea *
mech_gl_box_pick_child (MechGLView *view,
                        gdouble     x,
                        gdouble     y,
                        gdouble    *child_x,
                        gdouble    *child_y)
{
  cairo_rectangle_t allocation;
  GLuint buffer[4] = { 0 };
  MechGLBoxPrivate *priv;
  guint16 child_id;
  MechArea *child;

  priv = mech_gl_box_get_instance_private ((MechGLBox *) view);
  mech_gl_box_update_pick_buffer ((MechGLBox *) view, x, y);

  /* FIXME: Use pbos for async reads */
  glBindFramebuffer (GL_FRAMEBUFFER, priv->picking_fbo);
  glReadPixels (0, 0, 1, 1, GL_BGRA, GL_UNSIGNED_INT, &buffer);

  /* Alpha=0 means no item was there */
  if (buffer[3] == 0)
    return NULL;

  /* R contains child ID with 16 bit resolution */
  child_id = buffer[2] >> 16;
  child = g_hash_table_lookup (priv->children_by_id,
                               GUINT_TO_POINTER (child_id));
  if (!child)
    return NULL;

  /* G and B contain child X/Y as a [0..1] range */
  mech_area_get_allocated_size (child, &allocation);

  if (child_x)
    *child_x = allocation.width * ((gdouble) buffer[1] / G_MAXUINT);

  if (child_y)
    *child_y = allocation.height * ((gdouble) buffer[0] / G_MAXUINT);

  return child;
}

static void
_multiply_matrix_vector (const GLdouble matrix[16],
                         const GLdouble in[4],
                         GLdouble       result[4])
{
  int i;

  for (i = 0; i < 4; i++)
    {
      result[i] =
        in[0] * matrix[0 * 4 + i] + in[1] * matrix[1 * 4 + i] +
        in[2] * matrix[2 * 4 + i] + in[3] * matrix[3 * 4 + i];
    }
}

static void
_multiply_matrices (const GLdouble a[16],
                    const GLdouble b[16],
                    GLdouble       result[16])
{
  int i, j;

  for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
        {
          result[i * 4 + j] =
            a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] +
            a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
	}
    }
}

static gboolean
_invert_matrix (const GLdouble m[16],
                GLdouble       result[16])
{
  double inv[16], det;
  int i;

  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
    m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
    m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
    m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
    m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
    m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
    m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
    m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
    m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
    m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
    m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] =  m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
    m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
    m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] =  -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
    m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] =   m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
    m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
    m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] =  m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
    m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

  det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

  if (det == 0)
    return FALSE;

  det = 1.0 / det;

  for (i = 0; i < 16; i++)
    result[i] = inv[i] * det;

  return TRUE;
}

static gboolean
_unproject_point (GLdouble        win_x,
                  GLdouble        win_y,
                  GLdouble        win_z,
                  const GLdouble  model_matrix[16],
                  const GLdouble  projection_matrix[16],
                  const GLint     viewport[4],
                  GLdouble       *obj_x,
                  GLdouble       *obj_y,
                  GLdouble       *obj_z)
{
  double result[16];
  double in[4];
  double out[4];

  _multiply_matrices (model_matrix, projection_matrix, result);

  if (!_invert_matrix (result, result))
    return FALSE;

  in[0] = win_x;
  in[1] = win_y;
  in[2] = win_z;
  in[3] = 1.0;

  /* Map x and y from window coordinates */
  in[0] = (in[0] - viewport[0]) / viewport[2];
  in[1] = (in[1] - viewport[1]) / viewport[3];

  /* Map to range -1 to 1 */
  in[0] = in[0] * 2 - 1;
  in[1] = in[1] * 2 - 1;
  in[2] = in[2] * 2 - 1;

  _multiply_matrix_vector (result, in, out);

  if (out[3] == 0.0)
    return FALSE;

  out[0] /= out[3];
  out[1] /= out[3];
  out[2] /= out[3];
  *obj_x = out[0];
  *obj_y = out[1];
  *obj_z = out[2];

  return TRUE;
}

static void
mech_gl_box_child_coords (MechGLView *view,
                          MechArea   *child,
                          gdouble     x,
                          gdouble     y,
                          gdouble    *child_x,
                          gdouble    *child_y)
{
  GLdouble model_matrix[16], projection_matrix[16];
  cairo_rectangle_t allocation, child_alloc;
  GLdouble near_x, near_y, near_z;
  GLdouble far_x, far_y, far_z;
  GLint viewport[4];

  /* Invert Y coordinate */
  mech_area_get_allocated_size ((MechArea *) view, &allocation);
  mech_area_get_allocated_size (child, &child_alloc);
  y = allocation.height - y;

  viewport[0] = viewport[1] = 0;
  viewport[2] = (GLint) allocation.width;
  viewport[3] = (GLint) allocation.height;

  glPushMatrix ();

  g_signal_emit (view, signals[POSITION_CHILD], 0, child);
  glTranslatef (-child_alloc.width / 2, child_alloc.height / 2,
		-NEAR_PLANE_DISTANCE);

  glGetDoublev (GL_MODELVIEW_MATRIX, model_matrix);
  glGetDoublev (GL_PROJECTION_MATRIX, projection_matrix);

  _unproject_point (x, y, 0, model_matrix,
                    projection_matrix, viewport,
                    &near_x, &near_y, &near_z);
  _unproject_point (x, y, 1, model_matrix,
                    projection_matrix, viewport,
                    &far_x, &far_y, &far_z);

  if (near_z == far_z)
    {
      /* The ray is parallel to the z=0 plane, set child
       * coords to +/- maxdouble based on directionality.
       */
      *child_x = (near_x < far_x) ? -G_MAXDOUBLE : G_MAXDOUBLE;
      *child_y = (near_y < far_y) ? -G_MAXDOUBLE : G_MAXDOUBLE;
    }
  else
    {
      gdouble z_diff;

      /* near and far points fall on different sides of the plane. */
      z_diff = - (near_z / (far_z - near_z));
      *child_x = near_x + ((far_x - near_x) * z_diff);
      *child_y = -(near_y + ((far_y - near_y) * z_diff));
    }

  glPopMatrix ();
}

static void
mech_gl_box_class_init (MechGLBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechGLViewClass *gl_view_class = MECH_GL_VIEW_CLASS (klass);

  object_class->finalize = mech_gl_box_finalize;

  gl_view_class->render_scene = mech_gl_box_render_scene;
  gl_view_class->pick_child = mech_gl_box_pick_child;
  gl_view_class->child_coords = mech_gl_box_child_coords;

  signals[POSITION_CHILD] =
    g_signal_new ("position-child",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechGLBoxClass, position_child),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, MECH_TYPE_AREA);
}

static void
mech_gl_box_init (MechGLBox *box)
{
  MechGLBoxPrivate *priv;

  priv = mech_gl_box_get_instance_private (box);
  priv->children = g_hash_table_new_full (NULL, NULL, NULL,
                                          (GDestroyNotify) g_free);
  priv->children_by_id = g_hash_table_new (NULL, NULL);
}

MechArea *
mech_gl_box_new (void)
{
  return g_object_new (MECH_TYPE_GL_BOX, NULL);
}
