// 11 june 2015
#include "uipriv_unix.h"

struct uiProgressBar {
	uiUnixControl c;
	GtkWidget *widget;
	GtkProgressBar *pbar;
	gboolean indeterminate;
	guint pulser;
};

uiUnixControlAllDefaultsExceptDestroy(uiProgressBar)

static void uiProgressBarDestroy(uiControl *c)
{
	uiProgressBar *p = uiProgressBar(c);

	// be sure to stop the timeout now
	if (p->indeterminate)
		g_source_remove(p->pulser);
	g_object_unref(p->widget);
	uiFreeControl(uiControl(p));
}

int uiProgressBarValue(uiProgressBar *p)
{
	if (p->indeterminate)
		return -1;
	return (int) (gtk_progress_bar_get_fraction(p->pbar) * 100);
}

static gboolean pulse(void* data)
{
	uiProgressBar *p = uiProgressBar(data);

	gtk_progress_bar_pulse(p->pbar);
	return TRUE;
}

void uiProgressBarSetValue(uiProgressBar *p, int value)
{
	if (value == -1) {
		if (!p->indeterminate) {
			p->indeterminate = TRUE;
			// TODO verify the timeout
			p->pulser = g_timeout_add(100, pulse, p);
		}
		return;
	}
	if (p->indeterminate) {
		p->indeterminate = FALSE;
		g_source_remove(p->pulser);
	}

	if (value < 0 || value > 100)
		userbug("Value %d is out of range for a uiProgressBar.", value);

	gtk_progress_bar_set_fraction(p->pbar, ((gdouble) value) / 100);
}

uiProgressBar *uiNewProgressBar(void)
{
	uiProgressBar *p;

	uiUnixNewControl(uiProgressBar, p);

	p->widget = gtk_progress_bar_new();
	p->pbar = GTK_PROGRESS_BAR(p->widget);

	return p;
}
