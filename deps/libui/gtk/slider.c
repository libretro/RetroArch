// 11 june 2015
#include "uipriv_unix.h"

struct uiSlider {
	uiUnixControl c;
	GtkWidget *widget;
	GtkRange *range;
	GtkScale *scale;
	void (*onChanged)(uiSlider *, void *);
	void *onChangedData;
	gulong onChangedSignal;
};

uiUnixControlAllDefaults(uiSlider)

static void onChanged(GtkRange *range, gpointer data)
{
	uiSlider *s = uiSlider(data);

	(*(s->onChanged))(s, s->onChangedData);
}

static void defaultOnChanged(uiSlider *s, void *data)
{
	// do nothing
}

int uiSliderValue(uiSlider *s)
{
	return gtk_range_get_value(s->range);
}

void uiSliderSetValue(uiSlider *s, int value)
{
	// we need to inhibit sending of ::value-changed because this WILL send a ::value-changed otherwise
	g_signal_handler_block(s->range, s->onChangedSignal);
	gtk_range_set_value(s->range, value);
	g_signal_handler_unblock(s->range, s->onChangedSignal);
}

void uiSliderOnChanged(uiSlider *s, void (*f)(uiSlider *, void *), void *data)
{
	s->onChanged = f;
	s->onChangedData = data;
}

uiSlider *uiNewSlider(int min, int max)
{
	uiSlider *s;
	int temp;

	if (min >= max) {
		temp = min;
		min = max;
		max = temp;
	}

	uiUnixNewControl(uiSlider, s);

	s->widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, 1);
	s->range = GTK_RANGE(s->widget);
	s->scale = GTK_SCALE(s->widget);

	// ensure integers, just to be safe
	gtk_scale_set_digits(s->scale, 0);

	s->onChangedSignal = g_signal_connect(s->scale, "value-changed", G_CALLBACK(onChanged), s);
	uiSliderOnChanged(s, defaultOnChanged, NULL);

	return s;
}
