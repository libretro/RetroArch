/* ui_qt_float_resync() against model float widgets.
 *
 * The models round the way the Qt widgets do: QDoubleSpinBox keeps
 * the number formatted at its decimals and read back, QSlider keeps
 * the stored value scaled by the precision and truncated to int.
 * Each model runs the paint-time resync the shipping widgets run and
 * counts the setValue() pushes it makes; a widget showing a setting
 * finer than its precision must stop pushing after one resync, or
 * every hover repaint does widget work again. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../ui/drivers/ui_qt_float_sync.h"

#define PAINTS 1000

typedef struct
{
   double value;   /* what the widget holds and reports */
   double min;
   double max;
   float *stored;  /* the setting the widget edits */
   float synced;
   unsigned pushes;
   int decimals;
} model_spinbox_t;

typedef struct
{
   float *stored;
   float synced;
   unsigned pushes;
   int value;
   int min;
   int max;
   unsigned precision;
} model_slider_t;

static int failures = 0;

static void check(int ok, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

static void spinbox_set(model_spinbox_t *b, double v)
{
   char buf[64];
   if (v < b->min)
      v = b->min;
   if (v > b->max)
      v = b->max;
   sprintf(buf, "%.*f", b->decimals, v);
   b->value = strtod(buf, NULL);
}

static void spinbox_init(model_spinbox_t *b, float *stored,
      int decimals, double min, double max)
{
   b->stored   = stored;
   b->decimals = decimals;
   b->min      = min;
   b->max      = max;
   b->pushes   = 0;
   spinbox_set(b, *stored);
   b->synced   = *stored;
}

static void spinbox_paint(model_spinbox_t *b)
{
   if (ui_qt_float_resync(&b->synced, *b->stored))
   {
      spinbox_set(b, *b->stored);
      b->pushes++;
   }
}

/* The user edits the box: Qt rounds, then valueChanged stores. */
static void spinbox_edit(model_spinbox_t *b, double v)
{
   spinbox_set(b, v);
   *b->stored = (float)b->value;
   b->synced  = *b->stored;
}

static void slider_set(model_slider_t *s, double v)
{
   int i = (int)v;
   if (i < s->min)
      i = s->min;
   if (i > s->max)
      i = s->max;
   s->value = i;
}

static void slider_init(model_slider_t *s, float *stored,
      unsigned precision, double min, double max)
{
   s->stored    = stored;
   s->precision = precision;
   s->min       = (int)(min * precision);
   s->max       = (int)(max * precision);
   s->pushes    = 0;
   slider_set(s, *stored * precision);
   s->synced    = *stored;
}

static void slider_paint(model_slider_t *s)
{
   if (ui_qt_float_resync(&s->synced, *s->stored))
   {
      slider_set(s, *s->stored * s->precision);
      s->pushes++;
   }
}

static void slider_edit(model_slider_t *s, int v)
{
   slider_set(s, v);
   *s->stored = (float)s->value / s->precision;
   s->synced  = *s->stored;
}

static void paint_spinbox(model_spinbox_t *b)
{
   int i;
   for (i = 0; i < PAINTS; i++)
      spinbox_paint(b);
}

static void paint_slider(model_slider_t *s)
{
   int i;
   for (i = 0; i < PAINTS; i++)
      slider_paint(s);
}

static void test_finer_than_decimals(void)
{
   float stored = 1.01f;
   model_spinbox_t b;
   model_slider_t  s;

   spinbox_init(&b, &stored, 1, 1.0, 10.0);
   slider_init(&s, &stored, 10, 1.0, 10.0);
   paint_spinbox(&b);
   paint_slider(&s);

   check(b.value == 1.0, "spinbox shows 1.01 at one decimal as 1.0");
   check(b.pushes == 0, "spinbox finer than its decimals stops pushing");
   check(s.pushes == 0, "slider finer than its precision stops pushing");
   check(stored == 1.01f, "resync leaves the setting as stored");
}

static void test_exact_values(void)
{
   static const float values[] = { 3.0f, 0.3f, 1.0f, 9.9f, 0.1f };
   unsigned i;

   for (i = 0; i < sizeof(values) / sizeof(values[0]); i++)
   {
      float stored = values[i];
      model_spinbox_t b;
      model_slider_t  s;

      spinbox_init(&b, &stored, 1, 0.0, 10.0);
      slider_init(&s, &stored, 10, 0.0, 10.0);
      paint_spinbox(&b);
      paint_slider(&s);
      check(b.pushes == 0, "spinbox at a representable value never pushes");
      check(s.pushes == 0, "slider at a representable value never pushes");
   }
}

static void test_external_change(void)
{
   float stored = 1.01f;
   model_spinbox_t b;
   model_slider_t  s;

   spinbox_init(&b, &stored, 1, 1.0, 10.0);
   slider_init(&s, &stored, 10, 1.0, 10.0);

   /* The menu or a config load changes the setting under the widgets. */
   stored = 2.55f;
   paint_spinbox(&b);
   paint_slider(&s);
   check(b.pushes == 1, "spinbox pushes a changed setting once");
   check(s.pushes == 1, "slider pushes a changed setting once");
   check(b.value == 2.5 || b.value == 2.6, "spinbox shows the new setting");
   check(s.value == 25, "slider shows the new setting");
}

static void test_paired_edit(void)
{
   /* FloatSliderAndSpinBox: both widgets edit one setting. */
   float stored = 1.01f;
   model_spinbox_t b;
   model_slider_t  s;

   spinbox_init(&b, &stored, 1, 1.0, 10.0);
   slider_init(&s, &stored, 10, 1.0, 10.0);

   slider_edit(&s, 42);
   paint_slider(&s);
   paint_spinbox(&b);
   check(s.pushes == 0, "slider does not echo its own edit");
   check(b.pushes == 1, "spinbox follows a slider edit once");
   check(b.value == 4.2, "spinbox shows the slider's value");

   spinbox_edit(&b, 7.3);
   paint_spinbox(&b);
   paint_slider(&s);
   check(b.pushes == 1, "spinbox does not echo its own edit");
   check(s.pushes == 1, "slider follows a spinbox edit once");
   check(s.value == 73, "slider shows the spinbox's value");
}

static void test_nan(void)
{
   float stored;
   float synced = 0.0f;
   unsigned pushes = 0;
   int i;

   stored = (float)strtod("nan", NULL);
   for (i = 0; i < PAINTS; i++)
      if (ui_qt_float_resync(&synced, stored))
         pushes++;
   check(pushes == 1, "a NaN setting settles after one push");
}

int main(void)
{
   test_finer_than_decimals();
   test_exact_values();
   test_external_change();
   test_paired_edit();
   test_nan();

   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   printf("qt_float_resync: all checks passed\n");
   return 0;
}
