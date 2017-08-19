// 14 april 2016
#include "uipriv_unix.h"

struct uiFontButton {
	uiUnixControl c;
	GtkWidget *widget;
	GtkButton *button;
	GtkFontButton *fb;
	GtkFontChooser *fc;
	void (*onChanged)(uiFontButton *, void *);
	void *onChangedData;
};

uiUnixControlAllDefaults(uiFontButton)

// TODO NOTE no need to inhibit the signal; font-set is documented as only being sent when the user changes the font
static void onFontSet(GtkFontButton *button, gpointer data)
{
	uiFontButton *b = uiFontButton(data);

	(*(b->onChanged))(b, b->onChangedData);
}

static void defaultOnChanged(uiFontButton *b, void *data)
{
	// do nothing
}

uiDrawTextFont *uiFontButtonFont(uiFontButton *b)
{
	PangoFont *f;
	PangoFontDescription *desc;

	desc = gtk_font_chooser_get_font_desc(b->fc);
	f = pangoDescToPangoFont(desc);
	// desc is transfer-full and thus is a copy
	pango_font_description_free(desc);
	return mkTextFont(f, FALSE);			// we hold the initial reference; no need to ref
}

void uiFontButtonOnChanged(uiFontButton *b, void (*f)(uiFontButton *, void *), void *data)
{
	b->onChanged = f;
	b->onChangedData = data;
}

uiFontButton *uiNewFontButton(void)
{
	uiFontButton *b;

	uiUnixNewControl(uiFontButton, b);

	b->widget = gtk_font_button_new();
	b->button = GTK_BUTTON(b->widget);
	b->fb = GTK_FONT_BUTTON(b->widget);
	b->fc = GTK_FONT_CHOOSER(b->widget);

	// match behavior on other platforms
	gtk_font_button_set_show_style(b->fb, TRUE);
	gtk_font_button_set_show_size(b->fb, TRUE);
	gtk_font_button_set_use_font(b->fb, FALSE);
	gtk_font_button_set_use_size(b->fb, FALSE);
	// other customizations
	gtk_font_chooser_set_show_preview_entry(b->fc, TRUE);

	g_signal_connect(b->widget, "font-set", G_CALLBACK(onFontSet), b);
	uiFontButtonOnChanged(b, defaultOnChanged, NULL);

	return b;
}
