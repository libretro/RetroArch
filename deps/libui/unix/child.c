// 28 august 2015
#include "uipriv_unix.h"

// This file contains helpers for managing child controls.

struct child {
	uiControl *c;
	GtkWidget *widget;

	gboolean oldhexpand;
	GtkAlign oldhalign;
	gboolean oldvexpand;
	GtkAlign oldvalign;

	// Some children can be boxed; that is, they can have an optionally-margined box around them.
	// uiGroup, uiTab, and uiWindow all do this.
	GtkWidget *box;

	// If the child is not boxed, this is its parent.
	// If the child is boxed, this is the box.
	GtkContainer *parent;

	// This flag is for users of these functions.
	// For uiBox, this is "spaced".
	// For uiTab, this is "margined". (uiGroup and uiWindow have to maintain their margined state themselves, since the margined state is independent of whether there is a child for those two.)
	int flag;
};

struct child *newChild(uiControl *child, uiControl *parent, GtkContainer *parentContainer)
{
	struct child *c;

	if (child == NULL)
		return NULL;

	c = uiNew(struct child);
	c->c = child;
	c->widget = GTK_WIDGET(uiControlHandle(c->c));

	c->oldhexpand = gtk_widget_get_hexpand(c->widget);
	c->oldhalign = gtk_widget_get_halign(c->widget);
	c->oldvexpand = gtk_widget_get_vexpand(c->widget);
	c->oldvalign = gtk_widget_get_valign(c->widget);

	uiControlSetParent(c->c, parent);
	uiUnixControlSetContainer(uiUnixControl(c->c), parentContainer, FALSE);
	c->parent = parentContainer;

	return c;
}

struct child *newChildWithBox(uiControl *child, uiControl *parent, GtkContainer *parentContainer, int margined)
{
	struct child *c;
	GtkWidget *box;

	if (child == NULL)
		return NULL;
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(box);
	c = newChild(child, parent, GTK_CONTAINER(box));
	gtk_widget_set_hexpand(c->widget, TRUE);
	gtk_widget_set_halign(c->widget, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand(c->widget, TRUE);
	gtk_widget_set_valign(c->widget, GTK_ALIGN_FILL);
	c->box = box;
	gtk_container_add(parentContainer, c->box);
	childSetMargined(c, margined);
	return c;
}

void childRemove(struct child *c)
{
	uiControlSetParent(c->c, NULL);
	uiUnixControlSetContainer(uiUnixControl(c->c), c->parent, TRUE);

	gtk_widget_set_hexpand(c->widget, c->oldhexpand);
	gtk_widget_set_halign(c->widget, c->oldhalign);
	gtk_widget_set_vexpand(c->widget, c->oldvexpand);
	gtk_widget_set_valign(c->widget, c->oldvalign);

	if (c->box != NULL)
		gtk_widget_destroy(c->box);

	uiFree(c);
}

void childDestroy(struct child *c)
{
	uiControl *child;

	child = c->c;
	childRemove(c);
	uiControlDestroy(child);
}

GtkWidget *childWidget(struct child *c)
{
	return c->widget;
}

int childFlag(struct child *c)
{
	return c->flag;
}

void childSetFlag(struct child *c, int flag)
{
	c->flag = flag;
}

GtkWidget *childBox(struct child *c)
{
	return c->box;
}

void childSetMargined(struct child *c, int margined)
{
	setMargined(GTK_CONTAINER(c->box), margined);
}
