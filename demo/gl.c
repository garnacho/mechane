/* Gears GL code by :
 *
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <mechane/mechane.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <GL/gl.h>

struct gears
{
  GLfloat angle;

  struct {
    GLfloat rotx;
    GLfloat roty;
  } view;

  GLint gear_list[3];
};

struct gear_template
{
  GLfloat material[4];
  GLfloat inner_radius;
  GLfloat outer_radius;
  GLfloat width;
  GLint teeth;
  GLfloat tooth_depth;
};

static const struct gear_template gear_templates[] = {
  { { 0.8, 0.1, 0.0, 1.0 }, 1.0, 4.0, 1.0, 20, 0.7 },
  { { 0.0, 0.8, 0.2, 1.0 }, 0.5, 2.0, 2.0, 10, 0.7 },
  { { 0.2, 0.2, 1.0, 1.0 }, 1.3, 2.0, 0.5, 10, 0.7 },
};

static GLfloat light_pos[4] = {5.0, 5.0, 10.0, 0.0};

typedef struct {
  MechGLView parent_instance;
  MechAnimation *animation;
  struct gears *gears;
  gdouble last_x;
  gdouble last_y;
  guint moving     : 1;
  guint has_coords : 1;
} GearsArea;

typedef struct {
  MechGLViewClass parent_class;
} GearsAreaClass;

G_DEFINE_TYPE (GearsArea, gears_area, MECH_TYPE_GL_VIEW)

static void
make_gear(const struct gear_template *t)
{
  GLint i;
  GLfloat r0, r1, r2;
  GLfloat angle, da;
  GLfloat u, v, len;

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, t->material);

  r0 = t->inner_radius;
  r1 = t->outer_radius - t->tooth_depth / 2.0;
  r2 = t->outer_radius + t->tooth_depth / 2.0;

  da = 2.0 * M_PI / t->teeth / 4.0;

  glShadeModel(GL_FLAT);

  glNormal3f(0.0, 0.0, 1.0);

  /* draw front face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= t->teeth; i++)
    {
      angle = i * 2.0 * M_PI / t->teeth;
      glVertex3f(r0 * cos(angle), r0 * sin(angle), t->width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), t->width * 0.5);
      if (i < t->teeth)
        {
          glVertex3f(r0 * cos(angle), r0 * sin(angle), t->width * 0.5);
          glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), t->width * 0.5);
        }
    }
  glEnd();

  /* draw front sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * M_PI / t->teeth / 4.0;
  for (i = 0; i < t->teeth; i++)
    {
      angle = i * 2.0 * M_PI / t->teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), t->width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), t->width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), t->width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), t->width * 0.5);
    }
  glEnd();

  glNormal3f(0.0, 0.0, -1.0);

  /* draw back face */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= t->teeth; i++)
    {
      angle = i * 2.0 * M_PI / t->teeth;
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -t->width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -t->width * 0.5);
      if (i < t->teeth)
        {
          glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -t->width * 0.5);
          glVertex3f(r0 * cos(angle), r0 * sin(angle), -t->width * 0.5);
        }
    }
  glEnd();

  /* draw back sides of teeth */
  glBegin(GL_QUADS);
  da = 2.0 * M_PI / t->teeth / 4.0;
  for (i = 0; i < t->teeth; i++)
    {
      angle = i * 2.0 * M_PI / t->teeth;

      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -t->width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -t->width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -t->width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -t->width * 0.5);
    }
  glEnd();

  /* draw outward faces of teeth */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < t->teeth; i++)
    {
      angle = i * 2.0 * M_PI / t->teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), t->width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -t->width * 0.5);
      u = r2 * cos(angle + da) - r1 * cos(angle);
      v = r2 * sin(angle + da) - r1 * sin(angle);
      len = sqrt(u * u + v * v);
      u /= len;
      v /= len;
      glNormal3f(v, -u, 0.0);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), t->width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -t->width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), t->width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -t->width * 0.5);
      u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
      v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
      glNormal3f(v, -u, 0.0);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), t->width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -t->width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
    }

  glVertex3f(r1 * cos(0), r1 * sin(0), t->width * 0.5);
  glVertex3f(r1 * cos(0), r1 * sin(0), -t->width * 0.5);

  glEnd();

  glShadeModel(GL_SMOOTH);

  /* draw inside radius cylinder */
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= t->teeth; i++)
    {
      angle = i * 2.0 * M_PI / t->teeth;
      glNormal3f(-cos(angle), -sin(angle), 0.0);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -t->width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), t->width * 0.5);
    }
  glEnd();
}

static struct gears *
gears_create (void)
{
  struct gears *gears;
  gint i, n_configs;

  gears = g_new0 (struct gears, 1);

  for (i = 0; i < 3; i++)
    {
      gears->gear_list[i] = glGenLists(1);
      glNewList(gears->gear_list[i], GL_COMPILE);
      make_gear(&gear_templates[i]);
      glEndList();
    }

  gears->view.rotx = 20.0;
  gears->view.roty = 30.0;

  return gears;
}

static void
animation_frame (MechAnimation *animation,
                 gint64         _time,
                 gpointer       user_data)
{
  GearsArea *gears_area = user_data;
  struct gears *gears = gears_area->gears;

  if (gears)
    gears->angle = (GLfloat) ((_time / 1000) % 8192) * 360 / 8192.0;

  mech_area_redraw (user_data, NULL);
}

static void
gears_area_visibility_changed (MechArea *area)
{
  GearsArea *gears_area;
  MechWindow *window;
  gboolean visible;

  gears_area = (GearsArea *) area;
  visible = mech_area_is_visible (area);
  window = mech_area_get_window (area);

  MECH_AREA_CLASS (gears_area_parent_class)->visibility_changed (area);

  if (!visible && gears_area->animation)
    g_clear_object (&gears_area->animation);
  else if (visible && window)
    {
      /* No timeline yet, meh */
      gears_area->animation = mech_acceleration_new (0, 0, 1);
      mech_animation_run (gears_area->animation,
                          mech_area_get_window (area));
      g_signal_connect (gears_area->animation, "frame",
                        G_CALLBACK (animation_frame), area);
    }
}

static void
gears_area_render_scene (MechGLView     *view,
                         cairo_device_t *device)
{
  struct gears *gears;
  GearsArea *gears_area;
  MechArea *area;

  gears_area = (GearsArea *) view;
  area = (MechArea *) view;

  if (!gears_area->gears)
    gears_area->gears = gears_create ();

  gears = gears_area->gears;

  /* Set projection and settings */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 200.0);
  glMatrixMode(GL_MODELVIEW);

  glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHTING);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  /* Render the gears */
  glPushMatrix();

  glTranslatef(0.0, 0.0, -38);

  glRotatef(gears->view.rotx, 1.0, 0.0, 0.0);
  glRotatef(gears->view.roty, 0.0, 1.0, 0.0);

  glPushMatrix();
  glTranslatef(-3.0, -2.0, 0.0);
  glRotatef(gears->angle, 0.0, 0.0, 1.0);
  glCallList(gears->gear_list[0]);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(3.1, -2.0, 0.0);
  glRotatef(-2.0 * gears->angle - 9.0, 0.0, 0.0, 1.0);
  glCallList(gears->gear_list[1]);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-3.1, 4.2, 0.0);
  glRotatef(-2.0 * gears->angle - 25.0, 0.0, 0.0, 1.0);
  glCallList(gears->gear_list[2]);
  glPopMatrix();

  glPopMatrix();

  glDisable (GL_LIGHTING);
  glDisable (GL_LIGHT0);
  glDisable (GL_CULL_FACE);
  glDisable (GL_DEPTH_TEST);
}

static gboolean
gears_area_handle_event (MechArea  *area,
                         MechEvent *event)
{
  GearsArea *gears_area = (GearsArea *) area;
  struct gears *gears = gears_area->gears;
  gdouble x, y;

  MECH_AREA_CLASS (gears_area_parent_class)->handle_event (area, event);

  if (event->any.target != area ||
      mech_event_has_flags (event, MECH_EVENT_FLAG_CAPTURE_PHASE))
    return FALSE;

  if (!gears)
    return TRUE;

  if (event->type == MECH_BUTTON_PRESS ||
      event->type == MECH_TOUCH_DOWN)
    {
      gears_area->moving = TRUE;
      gears_area->has_coords = FALSE;
    }
  else if (event->type == MECH_BUTTON_RELEASE ||
           event->type == MECH_TOUCH_UP)
    {
      gears_area->moving = FALSE;
      gears_area->has_coords = FALSE;
    }
  else if (gears_area->moving &&
           (event->type == MECH_TOUCH_MOTION ||
            event->type == MECH_MOTION))
    {
      mech_event_pointer_get_coords (event, &x, &y);

      if (gears_area->has_coords)
        {
          /* Things are inverted here as the gears rotate
           * along the given axis, so it looks more natural
           * to rotate along the opposite axis when the mouse
           * moves.
           */
          gears->view.roty += (x - gears_area->last_x);
          gears->view.rotx += (y - gears_area->last_y);
          mech_area_redraw (area, NULL);
        }

      gears_area->last_x = x;
      gears_area->last_y = y;
      gears_area->has_coords = TRUE;
    }

  return TRUE;
}

static void
gears_area_class_init (GearsAreaClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  MechGLViewClass *gl_view_class = MECH_GL_VIEW_CLASS (klass);

  area_class->visibility_changed = gears_area_visibility_changed;
  area_class->handle_event = gears_area_handle_event;

  gl_view_class->render_scene = gears_area_render_scene;
}

static void
gears_area_init (GearsArea *area)
{
  mech_area_set_name (MECH_AREA (area), "button");
  mech_area_add_events (MECH_AREA (area),
                        MECH_CROSSING_MASK |
                        MECH_MOTION_MASK |
                        MECH_BUTTON_MASK);
}

static MechArea *
gears_area_new (void)
{
  return g_object_new (gears_area_get_type (), NULL);
}

MechArea *
demo_gl (void)
{
  MechArea *box, *button;

  box = mech_fixed_box_new ();

  button = gears_area_new ();
  mech_area_add (box, button);

  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 10);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 150);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 10);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 150);

  button = gears_area_new ();
  mech_area_add (box, button);

  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 150);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 10);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 150);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 10);

  return box;
}
