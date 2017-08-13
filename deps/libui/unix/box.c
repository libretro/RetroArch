// 7 april 2015
#include "uipriv_unix.h"

struct boxChild {
	uiControl *c;
	int stretchy;
	gboolean oldhexpand;
	GtkAlign oldhalign;
	gboolean oldvexpand;
	GtkAlign oldvalign;
};

struct uiBox {
	uiUnixControl c;
	GtkWidget *widget;
	GtkContainer *container;
	GtkBox *box;
	GArray *controls;
	int vertical;
	int padded;
	GtkSizeGroup *stretchygroup;		// ensures all stretchy controls have the same size
};

uiUnixControlAllDefaultsExceptDestroy(uiBox)

#define ctrl(b, i) &g_array_index(b->controls, struct boxChild, i)

static void uiBoxDestroy(uiControl *c)
{
	uiBox *b = uiBox(c);
	struct boxChild *bc;
	guint i;

	// kill the size group
	g_object_unref(b->stretchygroup);
	// free all controls
	for (i = 0; i < b->controls->len; i++) {
		bc = ctrl(b, i);
		uiControlSetParent(bc->c, NULL);
		// and make sure the widget itself stays alive
		uiUnixControlSetContainer(uiUnixControl(bc->c), b->container, TRUE);
		uiControlDestroy(bc->c);
	}
	g_array_free(b->controls, TRUE);
	// and then ourselves
	g_object_unref(b->widget);
	uiFreeControl(uiControl(b));
}

void uiBoxAppend(uiBox *b, uiControl *c, int stretchy)
{
	struct boxChild bc;
	GtkWidget *widget;

	bc.c = c;
	bc.stretchy = stretchy;
	widget = GTK_WIDGET(uiControlHandle(bc.c));
	bc.oldhexpand = gtk_widget_get_hexpand(widget);
	bc.oldhalign = gtk_widget_get_halign(widget);
	bc.oldvexpand = gtk_widget_get_vexpand(widget);
	bc.oldvalign = gtk_widget_get_valign(widget);

	if (bc.stretchy) {
		if (b->vertical) {
			gtk_widget_set_vexpand(widget, TRUE);
			gtk_widget_set_valign(widget, GTK_ALIGN_FILL);
		} else {
			gtk_widget_set_hexpand(widget, TRUE);
			gtk_widget_set_halign(widget, GTK_ALIGN_FILL);
		}
		gtk_size_group_add_widget(b->stretchygroup, widget);
	} else
		if (b->vertical)
			gtk_widget_set_vexpand(widget, FALSE);
		else
			gtk_widget_set_hexpand(widget, FALSE);
	// and make them fill the opposite direction
	if (b->vertical) {
		gtk_widget_set_hexpand(widget, TRUE);
		gtk_widget_set_halign(widget, GTK_ALIGN_FILL);
	} else {
		gtk_widget_set_vexpand(widget, TRUE);
		gtk_widget_set_valign(widget, GTK_ALIGN_FILL);
	}

	uiControlSetParent(bc.c, uiControl(b));
	uiUnixControlSetContainer(uiUnixControl(bc.c), b->container, FALSE);
	g_array_append_val(b->controls, bc);
}

void uiBoxDelete(uiBox *b, int index)
{
	struct boxChild *bc;
	GtkWidget *widget;

	bc = ctrl(b, index);
	widget = GTK_WIDGET(uiControlHandle(bc->c));

	uiControlSetParent(bc->c, NULL);
	uiUnixControlSetContainer(uiUnixControl(bc->c), b->container, TRUE);

	if (bc->stretchy)
		gtk_size_group_remove_widget(b->stretchygroup, widget);
	gtk_widget_set_hexpand(widget, bc->oldhexpand);
	gtk_widget_set_halign(widget, bc->oldhalign);
	gtk_widget_set_vexpand(widget, bc->oldvexpand);
	gtk_widget_set_valign(widget, bc->oldvalign);

	g_array_remove_index(b->controls, index);
}

int uiBoxPadded(uiBox *b)
{
	return b->padded;
}

void uiBoxSetPadded(uiBox *b, int padded)
{
	b->padded = padded;
	if (b->padded)
		if (b->vertical)
			gtk_box_set_spacing(b->box, gtkYPadding);
		else
			gtk_box_set_spacing(b->box, gtkXPadding);
	else
		gtk_box_set_spacing(b->box, 0);
}

static uiBox *finishNewBox(GtkOrientation orientation)
{
	uiBox *b;

	uiUnixNewControl(uiBox, b);

	b->widget = gtk_box_new(orientation, 0);
	b->container = GTK_CONTAINER(b->widget);
	b->box = GTK_BOX(b->widget);

	b->vertical = orientation == GTK_ORIENTATION_VERTICAL;

	if (b->vertical)
		b->stretchygroup = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);
	else
		b->stretchygroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	b->controls = g_array_new(FALSE, TRUE, sizeof (struct boxChild));

	return b;
}

uiBox *uiNewHorizontalBox(void)
{
	return finishNewBox(GTK_ORIENTATION_HORIZONTAL);
}

uiBox *uiNewVerticalBox(void)
{
	return finishNewBox(GTK_ORIENTATION_VERTICAL);
}
