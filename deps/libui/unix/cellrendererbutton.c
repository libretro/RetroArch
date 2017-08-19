// 28 june 2016
#include "uipriv_unix.h"

// TODOs
// - it's a rather tight fit
// - selected row text color is white
// - resizing a column with a button in it crashes the program
// - accessibility
// - right side too big?

#define cellRendererButtonType (cellRendererButton_get_type())
#define cellRendererButton(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), cellRendererButtonType, cellRendererButton))
#define isCellRendererButton(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), cellRendererButtonType))
#define cellRendererButtonClass(class) (G_TYPE_CHECK_CLASS_CAST((class), cellRendererButtonType, cellRendererButtonClass))
#define isCellRendererButtonClass(class) (G_TYPE_CHECK_CLASS_TYPE((class), cellRendererButton))
#define getCellRendererButtonClass(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), cellRendererButtonType, cellRendererButtonClass))

typedef struct cellRendererButton cellRendererButton;
typedef struct cellRendererButtonClass cellRendererButtonClass;

struct cellRendererButton {
	GtkCellRenderer parent_instance;
	char *text;
};

struct cellRendererButtonClass {
	GtkCellRendererClass parent_class;
};

G_DEFINE_TYPE(cellRendererButton, cellRendererButton, GTK_TYPE_CELL_RENDERER)

static void cellRendererButton_init(cellRendererButton *c)
{
	g_object_set(c, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
	// the standard cell renderers all do this
	gtk_cell_renderer_set_padding(GTK_CELL_RENDERER(c), 2, 2);
}

static void cellRendererButton_dispose(GObject *obj)
{
	G_OBJECT_CLASS(cellRendererButton_parent_class)->dispose(obj);
}

static void cellRendererButton_finalize(GObject *obj)
{
	cellRendererButton *c = cellRendererButton(obj);

	if (c->text != NULL) {
		g_free(c->text);
		c->text = NULL;
	}
	G_OBJECT_CLASS(cellRendererButton_parent_class)->finalize(obj);
}

static GtkSizeRequestMode cellRendererButton_get_request_mode(GtkCellRenderer *r)
{
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

// this is basically what GtkCellRendererToggle did in 3.10 and does in 3.20, as well as what the Foreign Drawing gtk3-demo demo does
static GtkStyleContext *setButtonStyle(GtkWidget *widget)
{
	GtkStyleContext *base, *context;
	GtkWidgetPath *path;

	base = gtk_widget_get_style_context(widget);
	context = gtk_style_context_new();

	path = gtk_widget_path_copy(gtk_style_context_get_path(base));
	gtk_widget_path_append_type(path, G_TYPE_NONE);
	if (!FUTURE_gtk_widget_path_iter_set_object_name(path, -1, "button"))
		// not on 3.20; try the type
		gtk_widget_path_iter_set_object_type(path, -1, GTK_TYPE_BUTTON);

	gtk_style_context_set_path(context, path);
	gtk_style_context_set_parent(context, base);
	// the gtk3-demo example (which says we need to do this) uses gtk_widget_path_iter_get_state(path, -1) but that's not available until 3.14
	// TODO make a future for that too
	gtk_style_context_set_state(context, gtk_style_context_get_state(base));
	gtk_widget_path_unref(path);

	// and if the above widget path screwery stil doesn't work, this will
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);

	return context;
}

void unsetButtonStyle(GtkStyleContext *context)
{
	g_object_unref(context);
}

// this is based on what GtkCellRendererText does
static void cellRendererButton_get_preferred_width(GtkCellRenderer *r, GtkWidget *widget, gint *minimum, gint *natural)
{
	cellRendererButton *c = cellRendererButton(r);
	gint xpad;
	PangoLayout *layout;
	PangoRectangle rect;
	gint out;

	gtk_cell_renderer_get_padding(GTK_CELL_RENDERER(c), &xpad, NULL);

	layout = gtk_widget_create_pango_layout(widget, c->text);
	pango_layout_set_width(layout, -1);
	pango_layout_get_extents(layout, NULL, &rect);
	g_object_unref(layout);

	out = 2 * xpad + PANGO_PIXELS_CEIL(rect.width);
	if (minimum != NULL)
		*minimum = out;
	if (natural != NULL)
		*natural = out;
}

// this is based on what GtkCellRendererText does
static void cellRendererButton_get_preferred_height_for_width(GtkCellRenderer *r, GtkWidget *widget, gint width, gint *minimum, gint *natural)
{
	cellRendererButton *c = cellRendererButton(r);
	gint xpad, ypad;
	PangoLayout *layout;
	gint height;
	gint out;

	gtk_cell_renderer_get_padding(GTK_CELL_RENDERER(c), &xpad, &ypad);

	layout = gtk_widget_create_pango_layout(widget, c->text);
	pango_layout_set_width(layout, ((2 * xpad + width) * PANGO_SCALE));
	pango_layout_get_pixel_size(layout, NULL, &height);
	g_object_unref(layout);

	out = 2 * ypad + height;
	if (minimum != NULL)
		*minimum = out;
	if (natural != NULL)
		*natural = out;
}

// this is basically what GtkCellRendererText does
static void cellRendererButton_get_preferred_height(GtkCellRenderer *r, GtkWidget *widget, gint *minimum, gint *natural)
{
	gint width;

	gtk_cell_renderer_get_preferred_width(r, widget, &width, NULL);
	gtk_cell_renderer_get_preferred_height_for_width(r, widget, width, minimum, natural);
}

// this is based on what GtkCellRendererText does
static void cellRendererButton_get_aligned_area(GtkCellRenderer *r, GtkWidget *widget, GtkCellRendererState flags, const GdkRectangle *cell_area, GdkRectangle *aligned_area)
{
	cellRendererButton *c = cellRendererButton(r);
	gint xpad, ypad;
	PangoLayout *layout;
	PangoRectangle rect;
	gfloat xalign, yalign;
	gint xoffset, yoffset;

	gtk_cell_renderer_get_padding(GTK_CELL_RENDERER(c), &xpad, &ypad);

	layout = gtk_widget_create_pango_layout(widget, c->text);
	pango_layout_set_width(layout, -1);
	pango_layout_get_pixel_extents(layout, NULL, &rect);

	xoffset = 0;
	yoffset = 0;
	if (cell_area != NULL) {
		gtk_cell_renderer_get_alignment(GTK_CELL_RENDERER(c), &xalign, &yalign);
		xoffset = cell_area->width - (2 * xpad + rect.width);
		// use explicit casts just to be safe
		if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL)
			xoffset = ((gdouble) xoffset) * (1.0 - xalign);
		else
			xoffset *= ((gdouble) xoffset) * xalign;
		yoffset = yalign * (cell_area->height - (2 * ypad + rect.height));
		yoffset = MAX(yoffset, 0);
	}

	aligned_area->x = cell_area->x + xoffset;
	aligned_area->y = cell_area->y + yoffset;
	aligned_area->width = 2 * xpad + rect.width;
	aligned_area->height = 2 * ypad + rect.height;

	g_object_unref(layout);
}

// this is based on both what GtkCellRendererText does and what GtkCellRendererToggle does
static void cellRendererButton_render(GtkCellRenderer *r, cairo_t *cr, GtkWidget *widget, const GdkRectangle *background_area, const GdkRectangle *cell_area, GtkCellRendererState flags)
{
	cellRendererButton *c = cellRendererButton(r);
	gint xpad, ypad;
	GdkRectangle alignedArea;
	gint xoffset, yoffset;
	GtkStyleContext *context;
	PangoLayout *layout;
	PangoRectangle rect;

	gtk_cell_renderer_get_padding(GTK_CELL_RENDERER(c), &xpad, &ypad);
	gtk_cell_renderer_get_aligned_area(GTK_CELL_RENDERER(c), widget, flags, cell_area, &alignedArea);
	xoffset = alignedArea.x - cell_area->x;
	yoffset = alignedArea.y - cell_area->y;

	context = setButtonStyle(widget);
	layout = gtk_widget_create_pango_layout(widget, c->text);

	gtk_render_background(context, cr,
		background_area->x + xoffset + xpad,
		background_area->y + yoffset + ypad,
		background_area->width - 2 * xpad,
		background_area->height - 2 * ypad);
	gtk_render_frame(context, cr,
		background_area->x + xoffset + xpad,
		background_area->y + yoffset + ypad,
		background_area->width - 2 * xpad,
		background_area->height - 2 * ypad);

	pango_layout_set_width(layout, -1);
	pango_layout_get_pixel_extents(layout, NULL, &rect);
	xoffset -= rect.x;
	gtk_render_layout(context, cr,
		cell_area->x + xoffset + xpad,
		cell_area->y + yoffset + ypad,
		layout);

	g_object_unref(layout);
	unsetButtonStyle(context);
}

static guint clickedSignal;

static gboolean cellRendererButton_activate(GtkCellRenderer *r, GdkEvent *e, GtkWidget *widget, const gchar *path, const GdkRectangle *background_area, const GdkRectangle *cell_area, GtkCellRendererState flags)
{
	g_signal_emit(r, clickedSignal, 0, path);
	return TRUE;
}

static GParamSpec *props[2] = { NULL, NULL };

static void cellRendererButton_set_property(GObject *object, guint prop, const GValue *value, GParamSpec *pspec)
{
	cellRendererButton *c = cellRendererButton(object);

	if (prop != 1) {
		G_OBJECT_WARN_INVALID_PROPERTY_ID(c, prop, pspec);
		return;
	}
	if (c->text != NULL)
		g_free(c->text);
	c->text = g_value_dup_string(value);
	// GtkCellRendererText doesn't queue a redraw; we won't either
}

static void cellRendererButton_get_property(GObject *object, guint prop, GValue *value, GParamSpec *pspec)
{
	cellRendererButton *c = cellRendererButton(object);

	if (prop != 1) {
		G_OBJECT_WARN_INVALID_PROPERTY_ID(c, prop, pspec);
		return;
	}
	g_value_set_string(value, c->text);
}

static void cellRendererButton_class_init(cellRendererButtonClass *class)
{
	G_OBJECT_CLASS(class)->dispose = cellRendererButton_dispose;
	G_OBJECT_CLASS(class)->finalize = cellRendererButton_finalize;
	G_OBJECT_CLASS(class)->set_property = cellRendererButton_set_property;
	G_OBJECT_CLASS(class)->get_property = cellRendererButton_get_property;
	GTK_CELL_RENDERER_CLASS(class)->get_request_mode = cellRendererButton_get_request_mode;
	GTK_CELL_RENDERER_CLASS(class)->get_preferred_width = cellRendererButton_get_preferred_width;
	GTK_CELL_RENDERER_CLASS(class)->get_preferred_height_for_width = cellRendererButton_get_preferred_height_for_width;
	GTK_CELL_RENDERER_CLASS(class)->get_preferred_height = cellRendererButton_get_preferred_height;
	// don't provide a get_preferred_width_for_height()
	GTK_CELL_RENDERER_CLASS(class)->get_aligned_area = cellRendererButton_get_aligned_area;
	// don't provide a get_size()
	GTK_CELL_RENDERER_CLASS(class)->render = cellRendererButton_render;
	GTK_CELL_RENDERER_CLASS(class)->activate = cellRendererButton_activate;
	// don't provide a start_editing()

	props[1] = g_param_spec_string("text",
		"Text",
		"Button text",
		"",
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
	g_object_class_install_properties(G_OBJECT_CLASS(class), 2, props);

	clickedSignal = g_signal_new("clicked",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE,
		1, G_TYPE_STRING);
}

GtkCellRenderer *newCellRendererButton(void)
{
	return GTK_CELL_RENDERER(g_object_new(cellRendererButtonType, NULL));
}
