#include <mechane/mechane.h>

typedef struct {
  gdouble angle;
  gdouble scale;
} MatrixData;

static MatrixData *
area_get_matrix_data (MechArea *area)
{
  MatrixData *data;

  data = g_object_get_data (G_OBJECT (area), "matrix-data");

  if (data)
    return data;

  data = g_new0 (MatrixData, 1);
  data->angle = 0;
  data->scale = 1;
  g_object_set_data_full (G_OBJECT (area), "matrix-data",
                          data, (GDestroyNotify) g_free);
  return data;
}

static void
update_area_matrix (MechArea   *area,
                    MatrixData *data)
{
  cairo_matrix_t matrix;
  cairo_rectangle_t alloc;

  mech_area_get_allocated_size (area, &alloc);

  cairo_matrix_init_translate (&matrix, alloc.width / 2, alloc.height / 2);
  cairo_matrix_rotate (&matrix, data->angle);
  cairo_matrix_scale (&matrix, data->scale, data->scale);
  cairo_matrix_translate (&matrix, -alloc.width / 2, -alloc.height / 2);

  mech_area_set_matrix (area, &matrix);
}

static void
angle_slider_notify (GObject    *object,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  MatrixData *data;

  data = area_get_matrix_data (user_data);
  data->angle = mech_adjustable_get_value (MECH_ADJUSTABLE (object));
  update_area_matrix (user_data, data);
}

static void
scale_slider_notify (GObject    *object,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  MatrixData *data;

  data = area_get_matrix_data (user_data);
  data->scale = mech_adjustable_get_value (MECH_ADJUSTABLE (object));
  update_area_matrix (user_data, data);
}

static MechArea *
create_transform_area (void)
{
  MechArea *box, *button, *text, *check;

  box = mech_linear_box_new (MECH_ORIENTATION_VERTICAL);

  button = mech_button_new ();
  mech_area_add (box, button);

  text = mech_text_view_new ();
  mech_text_set_string (MECH_TEXT (text), "Button");
  mech_area_add (button, text);

  check = mech_check_mark_new ();
  mech_text_set_string (MECH_TEXT (check), "Check!");
  mech_area_add (box, check);

  return box;
}

MechArea *
demo_matrices (void)
{
  MechArea *slider, *transform, *box;

  box = mech_fixed_box_new ();

  transform = create_transform_area ();

  mech_area_add (box, transform);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), transform,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT,
                         MECH_UNIT_PERCENTAGE, .45);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), transform,
                         MECH_SIDE_TOP, MECH_SIDE_TOP,
                         MECH_UNIT_PX, 100);

  /* Angle slider */
  slider = mech_slider_new (MECH_ORIENTATION_HORIZONTAL);
  mech_adjustable_set_bounds (MECH_ADJUSTABLE (slider), 0, 2 * G_PI);
  g_signal_connect (slider, "notify::value",
                    G_CALLBACK (angle_slider_notify), transform);

  mech_area_add (box, slider);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), slider,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 20);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), slider,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 20);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), slider,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 20);

  /* Scale slider */
  slider = mech_slider_new (MECH_ORIENTATION_HORIZONTAL);
  mech_adjustable_set_bounds (MECH_ADJUSTABLE (slider), 0.5, 4);
  mech_adjustable_set_value (MECH_ADJUSTABLE (slider), 1);
  g_signal_connect (slider, "notify::value",
                    G_CALLBACK (scale_slider_notify), transform);

  mech_area_add (box, slider);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), slider,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 20);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), slider,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 20);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), slider,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 40);

  return box;
}
