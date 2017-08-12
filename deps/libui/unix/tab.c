// 11 june 2015
#include "uipriv_unix.h"

struct uiTab {
	uiUnixControl c;

	GtkWidget *widget;
	GtkContainer *container;
	GtkNotebook *notebook;

	GArray *pages;				// []*struct child
};

uiUnixControlAllDefaultsExceptDestroy(uiTab)

static void uiTabDestroy(uiControl *c)
{
	uiTab *t = uiTab(c);
	guint i;
	struct child *page;

	for (i = 0; i < t->pages->len; i++) {
		page = g_array_index(t->pages, struct child *, i);
		childDestroy(page);
	}
	g_array_free(t->pages, TRUE);
	// and free ourselves
	g_object_unref(t->widget);
	uiFreeControl(uiControl(t));
}

void uiTabAppend(uiTab *t, const char *name, uiControl *child)
{
	uiTabInsertAt(t, name, t->pages->len, child);
}

void uiTabInsertAt(uiTab *t, const char *name, int n, uiControl *child)
{
	struct child *page;

	// this will create a tab, because of gtk_container_add()
	page = newChildWithBox(child, uiControl(t), t->container, 0);

	gtk_notebook_set_tab_label_text(t->notebook, childBox(page), name);
	gtk_notebook_reorder_child(t->notebook, childBox(page), n);

	g_array_insert_val(t->pages, n, page);
}

void uiTabDelete(uiTab *t, int n)
{
	struct child *page;

	page = g_array_index(t->pages, struct child *, n);
	// this will remove the tab, because gtk_widget_destroy() calls gtk_container_remove()
	childRemove(page);
	g_array_remove_index(t->pages, n);
}

int uiTabNumPages(uiTab *t)
{
	return t->pages->len;
}

int uiTabMargined(uiTab *t, int n)
{
	struct child *page;

	page = g_array_index(t->pages, struct child *, n);
	return childFlag(page);
}

void uiTabSetMargined(uiTab *t, int n, int margined)
{
	struct child *page;

	page = g_array_index(t->pages, struct child *, n);
	childSetFlag(page, margined);
	childSetMargined(page, childFlag(page));
}

uiTab *uiNewTab(void)
{
	uiTab *t;

	uiUnixNewControl(uiTab, t);

	t->widget = gtk_notebook_new();
	t->container = GTK_CONTAINER(t->widget);
	t->notebook = GTK_NOTEBOOK(t->widget);

	gtk_notebook_set_scrollable(t->notebook, TRUE);

	t->pages = g_array_new(FALSE, TRUE, sizeof (struct child *));

	return t;
}
