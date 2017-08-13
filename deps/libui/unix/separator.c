// 11 june 2015
#include "uipriv_unix.h"

struct uiSeparator {
	uiUnixControl c;
	GtkWidget *widget;
	GtkSeparator *separator;
};

uiUnixControlAllDefaults(uiSeparator)

uiSeparator *uiNewHorizontalSeparator(void)
{
	uiSeparator *s;

	uiUnixNewControl(uiSeparator, s);

	s->widget = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	s->separator = GTK_SEPARATOR(s->widget);

	return s;
}

uiSeparator *uiNewVerticalSeparator(void)
{
	uiSeparator *s;

	uiUnixNewControl(uiSeparator, s);

	s->widget = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	s->separator = GTK_SEPARATOR(s->widget);

	return s;
}
