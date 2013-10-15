typedef struct {
  MechGLBox parent_instance;
  gdouble angle_x;
  gdouble angle_y;
} RotatedView;

typedef struct {
  MechGLBoxClass parent_class;
} RotatedViewClass;

G_DEFINE_TYPE (RotatedView, rotated_view, MECH_TYPE_GL_BOX)

static void
rotated_view_position_child (MechGLBox *box,
                             MechArea  *child)
{
  RotatedView *view = (RotatedView *) box;

  glRotated (view->angle_x, 1, 0, 0);
  glRotated (view->angle_y, 0, 1, 0);
  glTranslatef (0, 0, - MAX (ABS (view->angle_x), ABS (view->angle_y)) * 9);
}

static void
rotated_view_class_init (RotatedViewClass *klass)
{
  MechGLBoxClass *gl_box_class = MECH_GL_BOX_CLASS (klass);

  gl_box_class->position_child = rotated_view_position_child;
}

static void
rotated_view_init (RotatedView *view)
{
  mech_area_add_events (MECH_AREA (view),
                        MECH_CROSSING_MASK |
                        MECH_MOTION_MASK |
                        MECH_BUTTON_MASK |
                        MECH_TOUCH_MASK |
                        MECH_SCROLL_MASK |
                        MECH_KEY_MASK |
                        MECH_FOCUS_MASK);
}

static MechArea *
rotated_view_new (void)
{
  return g_object_new (rotated_view_get_type (), NULL);
}

static void
rotated_view_set_angle (RotatedView     *view,
                        MechOrientation  orientation,
                        gdouble          angle)
{
  if (orientation == MECH_ORIENTATION_VERTICAL)
    view->angle_x = angle;
  else if (orientation == MECH_ORIENTATION_HORIZONTAL)
    view->angle_y = angle;

  mech_area_redraw (MECH_AREA (view), NULL);
}

static void
rotate_slider_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  RotatedView *view = user_data;
  MechOrientation orientation;
  gdouble value;

  orientation = mech_orientable_get_orientation (MECH_ORIENTABLE (object));
  value = mech_adjustable_get_value (MECH_ADJUSTABLE (object));
  rotated_view_set_angle (view, orientation, value);
}

MechArea *
demo_gl_box (void)
{
  MechArea *demo, *slider, *sw_box, *gl_box, *child;

  demo = mech_area_new (NULL, MECH_NONE);

  /* Add GL rotated view */
  gl_box = rotated_view_new ();
  mech_area_add (demo, gl_box);

  /* Add child to rotated view */
  child = demo_scroll_area ();
  mech_area_set_preferred_size (child, MECH_AXIS_X,
                                MECH_UNIT_PX, 700);
  mech_area_set_preferred_size (child, MECH_AXIS_Y,
                                MECH_UNIT_PX, 500);
  mech_area_add (gl_box, child);

  /* Set a 2D container for sliders, to be shown on top */
  sw_box = mech_fixed_box_new ();
  mech_area_set_surface_type (sw_box, MECH_SURFACE_TYPE_SOFTWARE);

  slider = mech_slider_new (MECH_ORIENTATION_HORIZONTAL);
  mech_adjustable_set_bounds (MECH_ADJUSTABLE (slider), - G_PI / 4, G_PI / 4);
  g_signal_connect (slider, "notify::value",
                    G_CALLBACK (rotate_slider_notify), gl_box);
  mech_area_add (sw_box, slider);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 40);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 40);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 20);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_TOP, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 40);

  slider = mech_slider_new (MECH_ORIENTATION_VERTICAL);
  g_signal_connect (slider, "notify::value",
                    G_CALLBACK (rotate_slider_notify), gl_box);
  mech_adjustable_set_bounds (MECH_ADJUSTABLE (slider), - G_PI / 4, G_PI / 4);
  mech_area_add (sw_box, slider);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 40);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 40);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 20);
  mech_fixed_box_attach (MECH_FIXED_BOX (sw_box), slider,
                         MECH_SIDE_LEFT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 40);

  mech_area_add (demo, sw_box);

  return demo;
}
