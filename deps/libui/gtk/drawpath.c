// 6 september 2015
#include "uipriv_unix.h"
#include "draw.h"

struct uiDrawPath {
	GArray *pieces;
	uiDrawFillMode fillMode;
	gboolean ended;
};

struct piece {
	int type;
	double d[8];
	int b;
};

enum {
	newFigure,
	newFigureArc,
	lineTo,
	arcTo,
	bezierTo,
	closeFigure,
	addRect,
};

uiDrawPath *uiDrawNewPath(uiDrawFillMode mode)
{
	uiDrawPath *p;

	p = uiNew(uiDrawPath);
	p->pieces = g_array_new(FALSE, TRUE, sizeof (struct piece));
	p->fillMode = mode;
	return p;
}

void uiDrawFreePath(uiDrawPath *p)
{
	g_array_free(p->pieces, TRUE);
	uiFree(p);
}

static void add(uiDrawPath *p, struct piece *piece)
{
	if (p->ended)
		userbug("You cannot modify a uiDrawPath that has been ended. (path: %p)", p);
	g_array_append_vals(p->pieces, piece, 1);
}

void uiDrawPathNewFigure(uiDrawPath *p, double x, double y)
{
	struct piece piece;

	piece.type = newFigure;
	piece.d[0] = x;
	piece.d[1] = y;
	add(p, &piece);
}

void uiDrawPathNewFigureWithArc(uiDrawPath *p, double xCenter, double yCenter, double radius, double startAngle, double sweep, int negative)
{
	struct piece piece;

	if (sweep > 2 * uiPi)
		sweep = 2 * uiPi;
	piece.type = newFigureArc;
	piece.d[0] = xCenter;
	piece.d[1] = yCenter;
	piece.d[2] = radius;
	piece.d[3] = startAngle;
	piece.d[4] = sweep;
	piece.b = negative;
	add(p, &piece);
}

void uiDrawPathLineTo(uiDrawPath *p, double x, double y)
{
	struct piece piece;

	piece.type = lineTo;
	piece.d[0] = x;
	piece.d[1] = y;
	add(p, &piece);
}

void uiDrawPathArcTo(uiDrawPath *p, double xCenter, double yCenter, double radius, double startAngle, double sweep, int negative)
{
	struct piece piece;

	if (sweep > 2 * uiPi)
		sweep = 2 * uiPi;
	piece.type = arcTo;
	piece.d[0] = xCenter;
	piece.d[1] = yCenter;
	piece.d[2] = radius;
	piece.d[3] = startAngle;
	piece.d[4] = sweep;
	piece.b = negative;
	add(p, &piece);
}

void uiDrawPathBezierTo(uiDrawPath *p, double c1x, double c1y, double c2x, double c2y, double endX, double endY)
{
	struct piece piece;

	piece.type = bezierTo;
	piece.d[0] = c1x;
	piece.d[1] = c1y;
	piece.d[2] = c2x;
	piece.d[3] = c2y;
	piece.d[4] = endX;
	piece.d[5] = endY;
	add(p, &piece);
}

void uiDrawPathCloseFigure(uiDrawPath *p)
{
	struct piece piece;

	piece.type = closeFigure;
	add(p, &piece);
}

void uiDrawPathAddRectangle(uiDrawPath *p, double x, double y, double width, double height)
{
	struct piece piece;

	piece.type = addRect;
	piece.d[0] = x;
	piece.d[1] = y;
	piece.d[2] = width;
	piece.d[3] = height;
	add(p, &piece);
}

void uiDrawPathEnd(uiDrawPath *p)
{
	p->ended = TRUE;
}

void runPath(uiDrawPath *p, cairo_t *cr)
{
	guint i;
	struct piece *piece;
	void (*arc)(cairo_t *, double, double, double, double, double);

	if (!p->ended)
		userbug("You cannot draw with a uiDrawPath that has not been ended. (path: %p)", p);
	cairo_new_path(cr);
	for (i = 0; i < p->pieces->len; i++) {
		piece = &g_array_index(p->pieces, struct piece, i);
		switch (piece->type) {
		case newFigure:
			cairo_move_to(cr, piece->d[0], piece->d[1]);
			break;
		case newFigureArc:
			cairo_new_sub_path(cr);
			// fall through
		case arcTo:
			arc = cairo_arc;
			if (piece->b)
				arc = cairo_arc_negative;
			(*arc)(cr,
				piece->d[0],
				piece->d[1],
				piece->d[2],
				piece->d[3],
				piece->d[3] + piece->d[4]);
			break;
		case lineTo:
			cairo_line_to(cr, piece->d[0], piece->d[1]);
			break;
		case bezierTo:
			cairo_curve_to(cr,
				piece->d[0],
				piece->d[1],
				piece->d[2],
				piece->d[3],
				piece->d[4],
				piece->d[5]);
			break;
		case closeFigure:
			cairo_close_path(cr);
			break;
		case addRect:
			cairo_rectangle(cr,
				piece->d[0],
				piece->d[1],
				piece->d[2],
				piece->d[3]);
			break;
		}
	}
}

uiDrawFillMode pathFillMode(uiDrawPath *path)
{
	return path->fillMode;
}
