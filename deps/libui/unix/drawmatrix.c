// 6 september 2015
#include "uipriv_unix.h"
#include "draw.h"

void m2c(uiDrawMatrix *m, cairo_matrix_t *c)
{
	c->xx = m->M11;
	c->yx = m->M12;
	c->xy = m->M21;
	c->yy = m->M22;
	c->x0 = m->M31;
	c->y0 = m->M32;
}

static void c2m(cairo_matrix_t *c, uiDrawMatrix *m)
{
	m->M11 = c->xx;
	m->M12 = c->yx;
	m->M21 = c->xy;
	m->M22 = c->yy;
	m->M31 = c->x0;
	m->M32 = c->y0;
}

void uiDrawMatrixTranslate(uiDrawMatrix *m, double x, double y)
{
	cairo_matrix_t c;

	m2c(m, &c);
	cairo_matrix_translate(&c, x, y);
	c2m(&c, m);
}

void uiDrawMatrixScale(uiDrawMatrix *m, double xCenter, double yCenter, double x, double y)
{
	cairo_matrix_t c;
	double xt, yt;

	m2c(m, &c);
	xt = x;
	yt = y;
	scaleCenter(xCenter, yCenter, &xt, &yt);
	cairo_matrix_translate(&c, xt, yt);
	cairo_matrix_scale(&c, x, y);
	cairo_matrix_translate(&c, -xt, -yt);
	c2m(&c, m);
}

void uiDrawMatrixRotate(uiDrawMatrix *m, double x, double y, double amount)
{
	cairo_matrix_t c;

	m2c(m, &c);
	cairo_matrix_translate(&c, x, y);
	cairo_matrix_rotate(&c, amount);
	cairo_matrix_translate(&c, -x, -y);
	c2m(&c, m);
}

void uiDrawMatrixSkew(uiDrawMatrix *m, double x, double y, double xamount, double yamount)
{
	fallbackSkew(m, x, y, xamount, yamount);
}

void uiDrawMatrixMultiply(uiDrawMatrix *dest, uiDrawMatrix *src)
{
	cairo_matrix_t c;
	cairo_matrix_t d;

	m2c(dest, &c);
	m2c(src, &d);
	cairo_matrix_multiply(&c, &c, &d);
	c2m(&c, dest);
}

int uiDrawMatrixInvertible(uiDrawMatrix *m)
{
	cairo_matrix_t c;

	m2c(m, &c);
	return cairo_matrix_invert(&c) == CAIRO_STATUS_SUCCESS;
}

int uiDrawMatrixInvert(uiDrawMatrix *m)
{
	cairo_matrix_t c;

	m2c(m, &c);
	if (cairo_matrix_invert(&c) != CAIRO_STATUS_SUCCESS)
		return 0;
	c2m(&c, m);
	return 1;
}

void uiDrawMatrixTransformPoint(uiDrawMatrix *m, double *x, double *y)
{
	cairo_matrix_t c;

	m2c(m, &c);
	cairo_matrix_transform_point(&c, x, y);
}

void uiDrawMatrixTransformSize(uiDrawMatrix *m, double *x, double *y)
{
	cairo_matrix_t c;

	m2c(m, &c);
	cairo_matrix_transform_distance(&c, x, y);
}
