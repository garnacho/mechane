
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
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cairo-gl.h>

static const EGLint gl_config_attributes[] = {
  EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
  EGL_RED_SIZE, 8,
  EGL_GREEN_SIZE, 8,
  EGL_BLUE_SIZE, 8,
  EGL_ALPHA_SIZE, 8,
  EGL_DEPTH_SIZE, 24,
  EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
  EGL_NONE
};

struct gears {
	struct window *window;
	struct widget *widget;

	struct display *d;

	EGLDisplay display;
	EGLDisplay config;
	EGLContext context;
	GLfloat angle;

	struct {
		GLfloat rotx;
		GLfloat roty;
	} view;

	int button_down;
	int last_x, last_y;

	GLint gear_list[3];
	int fullscreen;
	int frames;
	uint32_t last_fps;
};

struct gear_template {
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
  MechArea parent_instance;
  MechAnimation *animation;
  struct gears *gears;
} EglArea;

typedef struct {
  MechAreaClass parent_class;
} EglAreaClass;

G_DEFINE_TYPE (EglArea, egl_area, MECH_TYPE_AREA)

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
gears_create (cairo_device_t *device,
              EGLContext     *prev_context,
              EGLSurface     *surface)
{
  struct gears *gears;
  gint i, n_configs;

  gears = g_new0 (struct gears, 1);

  eglBindAPI(EGL_OPENGL_API);

  gears->display = cairo_egl_device_get_display (device);

  if (eglChooseConfig (gears->display, gl_config_attributes,
                       &gears->config, 1, &n_configs) == EGL_FALSE)
    {
      g_warning ("Could not choose config");
      return NULL;
    }

  gears->context = eglCreateContext(gears->display, gears->config,
                                    prev_context, NULL);

  if (!gears->context)
    {
      g_warning ("Could not create context");
      return NULL;
    }

  if (!eglMakeCurrent(gears->display, surface, surface, gears->context))
    {
      g_warning ("Could not make current");
      return NULL;
    }

  for (i = 0; i < 3; i++)
    {
      gears->gear_list[i] = glGenLists(1);
      glNewList(gears->gear_list[i], GL_COMPILE);
      make_gear(&gear_templates[i]);
      glEndList();
    }

  gears->button_down = 0;
  gears->last_x = 0;
  gears->last_y = 0;

  gears->view.rotx = 20.0;
  gears->view.roty = 30.0;


  glEnable(GL_NORMALIZE);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 200.0);
  glMatrixMode(GL_MODELVIEW);

  glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);
  glClearColor(0, 0, 0, 0);

  return gears;
}

static void
animation_frame (MechAnimation *animation,
                 gint64         _time,
                 gpointer       user_data)
{
  mech_area_redraw (user_data, NULL);
}

static void
egl_area_visibility_changed (MechArea *area)
{
  MechWindow *window;
  EglArea *egl_area;
  gboolean visible;

  egl_area = (EglArea *) area;
  visible = mech_area_is_visible (area);
  window = mech_area_get_window (area);

  if (!visible && egl_area->animation)
    g_clear_object (&egl_area->animation);
  else if (visible && window)
    {
      /* No timeline yet, meh */
      egl_area->animation = mech_acceleration_new (0, 0, 1);
      mech_animation_run (egl_area->animation,
                          mech_area_get_window (area));
      g_signal_connect (egl_area->animation, "frame",
                        G_CALLBACK (animation_frame), area);
    }
}

static void
egl_area_draw (MechArea *area,
               cairo_t  *cr)
{
  cairo_rectangle_t window_allocation, allocation;
  cairo_surface_t *surface;
  cairo_device_t *device;
  EGLContext *prev_context;
  EGLSurface *prev_surface;
  struct gears *gears;
  MechWindow *window;
  EglArea *egl_area;

  egl_area = (EglArea *) area;
  window = mech_area_get_window (area);

  surface = cairo_get_target (cr);
  mech_area_get_allocated_size (area, &allocation);
  mech_area_get_allocated_size (mech_window_get_root_area (window), &window_allocation);

  mech_area_transform_point (area, NULL, &allocation.x, &allocation.y);

  if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_GL)
    return;

  device = cairo_surface_get_device (surface);
  cairo_surface_flush (surface);

  prev_context = eglGetCurrentContext ();
  prev_surface = eglGetCurrentSurface (EGL_DRAW);

  if (!egl_area->gears)
    egl_area->gears = gears_create (device, prev_context, prev_surface);

  gears = egl_area->gears;
  eglMakeCurrent(gears->display,
                 prev_surface, prev_surface,
                 gears->context);

  glViewport (allocation.x, window_allocation.height - allocation.y - allocation.height, allocation.width, allocation.height);
  glScissor (allocation.x, window_allocation.height - allocation.y - allocation.height, allocation.width, allocation.height);

  glEnable(GL_SCISSOR_TEST);
  glClear(GL_DEPTH_BUFFER_BIT);

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

  glFlush();

  eglMakeCurrent(gears->display, prev_surface, prev_surface, prev_context);

  cairo_surface_mark_dirty (surface);

  gears->angle = (GLfloat) ((g_get_monotonic_time () / 1000) % 8192) * 360 / 8192.0;
}

static void
egl_area_class_init (EglAreaClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);

  area_class->visibility_changed = egl_area_visibility_changed;
  area_class->draw = egl_area_draw;
}

static void
egl_area_init (EglArea *area)
{
}

static MechArea *
egl_area_new (void)
{
  return g_object_new (egl_area_get_type (), NULL);
}

MechArea *
demo_egl (void)
{
  MechArea *box, *area, *button;

  box = mech_fixed_box_new ();

  button = mech_button_new ();
  area = egl_area_new ();
  mech_area_add (button, area);
  mech_area_add (box, button);

  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 10);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 150);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 10);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), button,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 150);

  button = mech_button_new ();
  area = egl_area_new ();
  mech_area_add (button, area);
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
