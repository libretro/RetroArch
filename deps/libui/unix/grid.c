// 9 june 2016
#include "uipriv_unix.h"

struct gridChild {
	uiControl *c;
	GtkWidget *label;
	gboolean oldhexpand;
	GtkAlign oldhalign;
	gboolean oldvexpand;
	GtkAlign oldvalign;
};

struct uiGrid {
	uiUnixControl c;
	GtkWidget *widget;
	GtkContainer *container;
	GtkGrid *grid;
	GArray *children;
	int padded;
};

uiUnixControlAllDefaultsExceptDestroy(uiGrid)

#define ctrl(g, i) &g_array_index(g->children, struct gridChild, i)

static void uiGridDestroy(uiControl *c)
{
	uiGrid *g = uiGrid(c);
	struct gridChild *gc;
	guint i;

	// free all controls
	for (i = 0; i < g->children->len; i++) {
		gc = ctrl(g, i);
		uiControlSetParent(gc->c, NULL);
		uiUnixControlSetContainer(uiUnixControl(gc->c), g->container, TRUE);
		uiControlDestroy(gc->c);
	}
	g_array_free(g->children, TRUE);
	// and then ourselves
	g_object_unref(g->widget);
	uiFreeControl(uiControl(g));
}

#define TODO_MASSIVE_HACK(c) \
	if (!uiUnixControl(c)->addedBefore) { \
		g_object_ref_sink(GTK_WIDGET(uiControlHandle(uiControl(c)))); \
		gtk_widget_show(GTK_WIDGET(uiControlHandle(uiControl(c)))); \
		uiUnixControl(c)->addedBefore = TRUE; \
	}

static const GtkAlign gtkAligns[] = {
	[uiAlignFill] = GTK_ALIGN_FILL,
	[uiAlignStart] = GTK_ALIGN_START,
	[uiAlignCenter] = GTK_ALIGN_CENTER,
	[uiAlignEnd] = GTK_ALIGN_END,
};

static const GtkPositionType gtkPositions[] = {
	[uiAtLeading] = GTK_POS_LEFT,
	[uiAtTop] = GTK_POS_TOP,
	[uiAtTrailing] = GTK_POS_RIGHT,
	[uiAtBottom] = GTK_POS_BOTTOM,
};

static GtkWidget *prepare(struct gridChild *gc, uiControl *c, int hexpand, uiAlign halign, int vexpand, uiAlign valign)
{
	GtkWidget *widget;

	gc->c = c;
	widget = GTK_WIDGET(uiControlHandle(gc->c));
	gc->oldhexpand = gtk_widget_get_hexpand(widget);
	gc->oldhalign = gtk_widget_get_halign(widget);
	gc->oldvexpand = gtk_widget_get_vexpand(widget);
	gc->oldvalign = gtk_widget_get_valign(widget);
	gtk_widget_set_hexpand(widget, hexpand != 0);
	gtk_widget_set_halign(widget, gtkAligns[halign]);
	gtk_widget_set_vexpand(widget, vexpand != 0);
	gtk_widget_set_valign(widget, gtkAligns[valign]);
	return widget;
}

void uiGridAppend(uiGrid *g, uiControl *c, int left, int top, int xspan, int yspan, int hexpand, uiAlign halign, int vexpand, uiAlign valign)
{
	struct gridChild gc;
	GtkWidget *widget;

	widget = prepare(&gc, c, hexpand, halign, vexpand, valign);
	uiControlSetParent(gc.c, uiControl(g));
	TODO_MASSIVE_HACK(uiUnixControl(gc.c));
	gtk_grid_attach(g->grid, widget,
		left, top,
		xspan, yspan);
	g_array_append_val(g->children, gc);
}

void uiGridInsertAt(uiGrid *g, uiControl *c, uiControl *existing, uiAt at, int xspan, int yspan, int hexpand, uiAlign halign, int vexpand, uiAlign valign)
{
	struct gridChild gc;
	GtkWidget *widget;

	widget = prepare(&gc, c, hexpand, halign, vexpand, valign);
	uiControlSetParent(gc.c, uiControl(g));
	TODO_MASSIVE_HACK(uiUnixControl(gc.c));
	gtk_grid_attach_next_to(g->grid, widget,
		GTK_WIDGET(uiControlHandle(existing)), gtkPositions[at],
		xspan, yspan);
	g_array_append_val(g->children, gc);
}

int uiGridPadded(uiGrid *g)
{
	return g->padded;
}

void uiGridSetPadded(uiGrid *g, int padded)
{
	g->padded = padded;
	if (g->padded) {
		gtk_grid_set_row_spacing(g->grid, gtkYPadding);
		gtk_grid_set_column_spacing(g->grid, gtkXPadding);
	} else {
		gtk_grid_set_row_spacing(g->grid, 0);
		gtk_grid_set_column_spacing(g->grid, 0);
	}
}

uiGrid *uiNewGrid(void)
{
	uiGrid *g;

	uiUnixNewControl(uiGrid, g);

	g->widget = gtk_grid_new();
	g->container = GTK_CONTAINER(g->widget);
	g->grid = GTK_GRID(g->widget);

	g->children = g_array_new(FALSE, TRUE, sizeof (struct gridChild));

	return g;
}
