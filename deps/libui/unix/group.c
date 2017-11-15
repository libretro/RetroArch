// 11 june 2015
#include "uipriv_unix.h"

struct uiGroup {
	uiUnixControl c;
	GtkWidget *widget;
	GtkContainer *container;
	GtkBin *bin;
	GtkFrame *frame;

	// unfortunately, even though a GtkFrame is a GtkBin, calling gtk_container_set_border_width() on it /includes/ the GtkFrame's label; we don't want tht
	struct child *child;

	int margined;
};

uiUnixControlAllDefaultsExceptDestroy(uiGroup)

static void uiGroupDestroy(uiControl *c)
{
	uiGroup *g = uiGroup(c);

	if (g->child != NULL)
		childDestroy(g->child);
	g_object_unref(g->widget);
	uiFreeControl(uiControl(g));
}

char *uiGroupTitle(uiGroup *g)
{
	return uiUnixStrdupText(gtk_frame_get_label(g->frame));
}

void uiGroupSetTitle(uiGroup *g, const char *text)
{
	gtk_frame_set_label(g->frame, text);
}

void uiGroupSetChild(uiGroup *g, uiControl *child)
{
	if (g->child != NULL)
		childRemove(g->child);
	g->child = newChildWithBox(child, uiControl(g), g->container, g->margined);
}

int uiGroupMargined(uiGroup *g)
{
	return g->margined;
}

void uiGroupSetMargined(uiGroup *g, int margined)
{
	g->margined = margined;
	if (g->child != NULL)
		childSetMargined(g->child, g->margined);
}

uiGroup *uiNewGroup(const char *text)
{
	uiGroup *g;
	gfloat yalign;
	GtkLabel *label;
	PangoAttribute *bold;
	PangoAttrList *boldlist;

	uiUnixNewControl(uiGroup, g);

	g->widget = gtk_frame_new(text);
	g->container = GTK_CONTAINER(g->widget);
	g->bin = GTK_BIN(g->widget);
	g->frame = GTK_FRAME(g->widget);

	// with GTK+, groupboxes by default have frames and slightly x-offset regular text
	// they should have no frame and fully left-justified, bold text
	// preserve default y-alignment
	gtk_frame_get_label_align(g->frame, NULL, &yalign);
	gtk_frame_set_label_align(g->frame, 0, yalign);
	gtk_frame_set_shadow_type(g->frame, GTK_SHADOW_NONE);
	label = GTK_LABEL(gtk_frame_get_label_widget(g->frame));
	// this is the boldness level used by GtkPrintUnixDialog
	// (it technically uses "bold" but see pango's pango-enum-types.c for the name conversion; GType is weird)
	bold = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
	boldlist = pango_attr_list_new();
	pango_attr_list_insert(boldlist, bold);
	gtk_label_set_attributes(label, boldlist);
	pango_attr_list_unref(boldlist);		// thanks baedert in irc.gimp.net/#gtk+

	return g;
}
