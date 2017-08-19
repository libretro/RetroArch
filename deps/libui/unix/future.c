// 29 june 2016
#include "uipriv_unix.h"

// functions FROM THE FUTURE!
// in some cases, because being held back by LTS releases sucks :/
// in others, because parts of GTK+ being unstable until recently also sucks :/

// added in pango 1.38; we need 1.36
static PangoAttribute *(*newFGAlphaAttr)(guint16 alpha) = NULL;

// added in GTK+ 3.20; we need 3.10
static void (*gwpIterSetObjectName)(GtkWidgetPath *path, gint pos, const char *name) = NULL;

// note that we treat any error as "the symbols aren't there" (and don't care if dlclose() failed)
void loadFutures(void)
{
	void *handle;

	// dlsym() walks the dependency chain, so opening the current process should be sufficient
	handle = dlopen(NULL, RTLD_LAZY);
	if (handle == NULL)
		return;
#define GET(var, fn) *((void **) (&var)) = dlsym(handle, #fn)
	GET(newFGAlphaAttr, pango_attr_foreground_alpha_new);
	GET(gwpIterSetObjectName, gtk_widget_path_iter_set_object_name);
	dlclose(handle);
}

PangoAttribute *FUTURE_pango_attr_foreground_alpha_new(guint16 alpha)
{
	if (newFGAlphaAttr == NULL)
		return NULL;
	return (*newFGAlphaAttr)(alpha);
}

gboolean FUTURE_gtk_widget_path_iter_set_object_name(GtkWidgetPath *path, gint pos, const char *name)
{
	if (gwpIterSetObjectName == NULL)
		return FALSE;
	(*gwpIterSetObjectName)(path, pos, name);
	return TRUE;
}
