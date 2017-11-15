// 15 may 2016
#include "uipriv_unix.h"

struct uiColorButton {
	uiUnixControl c;
	GtkWidget *widget;
	GtkButton *button;
	GtkColorButton *cb;
	GtkColorChooser *cc;
	void (*onChanged)(uiColorButton *, void *);
	void *onChangedData;
};

uiUnixControlAllDefaults(uiColorButton)

static void onColorSet(GtkColorButton *button, gpointer data)
{
	uiColorButton *b = uiColorButton(data);

	(*(b->onChanged))(b, b->onChangedData);
}

static void defaultOnChanged(uiColorButton *b, void *data)
{
	// do nothing
}

void uiColorButtonColor(uiColorButton *b, double *r, double *g, double *bl, double *a)
{
	GdkRGBA rgba;

	gtk_color_chooser_get_rgba(b->cc, &rgba);
	*r = rgba.red;
	*g = rgba.green;
	*bl = rgba.blue;
	*a = rgba.alpha;
}

void uiColorButtonSetColor(uiColorButton *b, double r, double g, double bl, double a)
{
	GdkRGBA rgba;

	rgba.red = r;
	rgba.green = g;
	rgba.blue = bl;
	rgba.alpha = a;
	// no need to inhibit the signal; color-set is documented as only being sent when the user changes the color
	gtk_color_chooser_set_rgba(b->cc, &rgba);
}

void uiColorButtonOnChanged(uiColorButton *b, void (*f)(uiColorButton *, void *), void *data)
{
	b->onChanged = f;
	b->onChangedData = data;
}

uiColorButton *uiNewColorButton(void)
{
	uiColorButton *b;
	GdkRGBA black;

	uiUnixNewControl(uiColorButton, b);

	// I'm not sure what the initial color is; set up a real one
	black.red = 0.0;
	black.green = 0.0;
	black.blue = 0.0;
	black.alpha = 1.0;
	b->widget = gtk_color_button_new_with_rgba(&black);
	b->button = GTK_BUTTON(b->widget);
	b->cb = GTK_COLOR_BUTTON(b->widget);
	b->cc = GTK_COLOR_CHOOSER(b->widget);

	gtk_color_chooser_set_use_alpha(b->cc, TRUE);

	g_signal_connect(b->widget, "color-set", G_CALLBACK(onColorSet), b);
	uiColorButtonOnChanged(b, defaultOnChanged, NULL);

	return b;
}
