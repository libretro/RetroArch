// 27 june 2016
#include "uipriv_unix.h"

struct uiImage {
	double width;
	double height;
	GPtrArray *images;
};

static void freeImageRep(gpointer item)
{
	cairo_surface_t *cs = (cairo_surface_t *) item;
	unsigned char *buf;

	buf = cairo_image_surface_get_data(cs);
	cairo_surface_destroy(cs);
	uiFree(buf);
}

uiImage *uiNewImage(double width, double height)
{
	uiImage *i;

	i = uiNew(uiImage);
	i->width = width;
	i->height = height;
	i->images = g_ptr_array_new_with_free_func(freeImageRep);
	return i;
}

void uiFreeImage(uiImage *i)
{
	g_ptr_array_free(i->images, TRUE);
	uiFree(i);
}

void uiImageAppend(uiImage *i, void *pixels, int pixelWidth, int pixelHeight, int pixelStride)
{
	cairo_surface_t *cs;
	unsigned char *buf, *p;
	uint8_t *src = (uint8_t *) pixels;
	int cstride;
	int y;

	cstride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, pixelWidth);
	buf = (unsigned char *) uiAlloc((cstride * pixelHeight * 4) * sizeof (unsigned char), "unsigned char[]");
	p = buf;
	for (y = 0; y < pixelStride * pixelHeight; y += pixelStride) {
		memmove(p, src + y, cstride);
		p += cstride;
	}
	cs = cairo_image_surface_create_for_data(buf, CAIRO_FORMAT_ARGB32,
		pixelWidth, pixelHeight,
		cstride);
	if (cairo_surface_status(cs) != CAIRO_STATUS_SUCCESS)
		/* TODO */;
	cairo_surface_flush(cs);
	g_ptr_array_add(i->images, cs);
}

struct matcher {
	cairo_surface_t *best;
	int distX;
	int distY;
	int targetX;
	int targetY;
	gboolean foundLarger;
};

// TODO is this the right algorithm?
static void match(gpointer surface, gpointer data)
{
	cairo_surface_t *cs = (cairo_surface_t *) surface;
	struct matcher *m = (struct matcher *) data;
	int x, y;
	int x2, y2;

	x = cairo_image_surface_get_width(cs);
	y = cairo_image_surface_get_height(cs);
	if (m->best == NULL)
		goto writeMatch;

	if (x < m->targetX && y < m->targetY)
		if (m->foundLarger)
			// always prefer larger ones
			return;
	if (x >= m->targetX && y >= m->targetY && !m->foundLarger)
		// we set foundLarger below
		goto writeMatch;

	x2 = abs(m->targetX - x);
	y2 = abs(m->targetY - y);
	if (x2 < m->distX && y2 < m->distY)
		goto writeMatch;

	// TODO weight one dimension? threshhold?
	return;

writeMatch:
	// must set this here too; otherwise the first image will never have ths set
	if (x >= m->targetX && y >= m->targetY && !m->foundLarger)
		m->foundLarger = TRUE;
	m->best = cs;
	m->distX = abs(m->targetX - x);
	m->distY = abs(m->targetY - y);
}

cairo_surface_t *imageAppropriateSurface(uiImage *i, GtkWidget *w)
{
	struct matcher m;

	m.best = NULL;
	m.distX = G_MAXINT;
	m.distY = G_MAXINT;
	m.targetX = i->width * gtk_widget_get_scale_factor(w);
	m.targetY = i->height * gtk_widget_get_scale_factor(w);
	m.foundLarger = FALSE;
	g_ptr_array_foreach(i->images, match, &m);
	return m.best;
}
