// 8 june 2016
#include "uipriv_unix.h"

struct formChild {
	uiControl *c;
	int stretchy;
	GtkWidget *label;
	gboolean oldhexpand;
	GtkAlign oldhalign;
	gboolean oldvexpand;
	GtkAlign oldvalign;
	GBinding *labelBinding;
};

struct uiForm {
	uiUnixControl c;
	GtkWidget *widget;
	GtkContainer *container;
	GtkGrid *grid;
	GArray *children;
	int padded;
	GtkSizeGroup *stretchygroup;		// ensures all stretchy controls have the same size
};

uiUnixControlAllDefaultsExceptDestroy(uiForm)

#define ctrl(f, i) &g_array_index(f->children, struct formChild, i)

static void uiFormDestroy(uiControl *c)
{
	uiForm *f = uiForm(c);
	struct formChild *fc;
	guint i;

	// kill the size group
	g_object_unref(f->stretchygroup);
	// free all controls
	for (i = 0; i < f->children->len; i++) {
		fc = ctrl(f, i);
		uiControlSetParent(fc->c, NULL);
		uiUnixControlSetContainer(uiUnixControl(fc->c), f->container, TRUE);
		uiControlDestroy(fc->c);
		gtk_widget_destroy(fc->label);
	}
	g_array_free(f->children, TRUE);
	// and then ourselves
	g_object_unref(f->widget);
	uiFreeControl(uiControl(f));
}

void uiFormAppend(uiForm *f, const char *label, uiControl *c, int stretchy)
{
	struct formChild fc;
	GtkWidget *widget;
	guint row;

	fc.c = c;
	widget = GTK_WIDGET(uiControlHandle(fc.c));
	fc.stretchy = stretchy;
	fc.oldhexpand = gtk_widget_get_hexpand(widget);
	fc.oldhalign = gtk_widget_get_halign(widget);
	fc.oldvexpand = gtk_widget_get_vexpand(widget);
	fc.oldvalign = gtk_widget_get_valign(widget);

	if (stretchy) {
		gtk_widget_set_vexpand(widget, TRUE);
		gtk_widget_set_valign(widget, GTK_ALIGN_FILL);
		gtk_size_group_add_widget(f->stretchygroup, widget);
	} else
		gtk_widget_set_vexpand(widget, FALSE);
	// and make them fill horizontally
	gtk_widget_set_hexpand(widget, TRUE);
	gtk_widget_set_halign(widget, GTK_ALIGN_FILL);

	fc.label = gtk_label_new(label);
	gtk_widget_set_hexpand(fc.label, FALSE);
	gtk_widget_set_halign(fc.label, GTK_ALIGN_END);
	gtk_widget_set_vexpand(fc.label, FALSE);
	if (GTK_IS_SCROLLED_WINDOW(widget))
		gtk_widget_set_valign(fc.label, GTK_ALIGN_START);
	else
		gtk_widget_set_valign(fc.label, GTK_ALIGN_CENTER);
	gtk_style_context_add_class(gtk_widget_get_style_context(fc.label), "dim-label");
	row = f->children->len;
	gtk_grid_attach(f->grid, fc.label,
		0, row,
		1, 1);
	// and make them share visibility so if the control is hidden, so is its label
	fc.labelBinding = g_object_bind_property(GTK_WIDGET(uiControlHandle(fc.c)), "visible",
		fc.label, "visible",
		G_BINDING_SYNC_CREATE);

	uiControlSetParent(fc.c, uiControl(f));
	uiUnixControlSetContainer(uiUnixControl(fc.c), f->container, FALSE);
	g_array_append_val(f->children, fc);

	// move the widget to the correct place
	gtk_container_child_set(f->container, widget,
		"left-attach", 1,
		"top-attach", row,
		NULL);
}

void uiFormDelete(uiForm *f, int index)
{
	struct formChild *fc;
	GtkWidget *widget;

	fc = ctrl(f, index);
	widget = GTK_WIDGET(uiControlHandle(fc->c));

	gtk_widget_destroy(fc->label);

	uiControlSetParent(fc->c, NULL);
	uiUnixControlSetContainer(uiUnixControl(fc->c), f->container, TRUE);

	if (fc->stretchy)
		gtk_size_group_remove_widget(f->stretchygroup, widget);
	gtk_widget_set_hexpand(widget, fc->oldhexpand);
	gtk_widget_set_halign(widget, fc->oldhalign);
	gtk_widget_set_vexpand(widget, fc->oldvexpand);
	gtk_widget_set_valign(widget, fc->oldvalign);

	g_array_remove_index(f->children, index);
}

int uiFormPadded(uiForm *f)
{
	return f->padded;
}

void uiFormSetPadded(uiForm *f, int padded)
{
	f->padded = padded;
	if (f->padded) {
		gtk_grid_set_row_spacing(f->grid, gtkYPadding);
		gtk_grid_set_column_spacing(f->grid, gtkXPadding);
	} else {
		gtk_grid_set_row_spacing(f->grid, 0);
		gtk_grid_set_column_spacing(f->grid, 0);
	}
}

uiForm *uiNewForm(void)
{
	uiForm *f;

	uiUnixNewControl(uiForm, f);

	f->widget = gtk_grid_new();
	f->container = GTK_CONTAINER(f->widget);
	f->grid = GTK_GRID(f->widget);

	f->stretchygroup = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);

	f->children = g_array_new(FALSE, TRUE, sizeof (struct formChild));

	return f;
}
