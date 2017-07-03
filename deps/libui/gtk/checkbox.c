// 10 june 2015
#include "uipriv_unix.h"

struct uiCheckbox {
	uiUnixControl c;
	GtkWidget *widget;
	GtkButton *button;
	GtkToggleButton *toggleButton;
	GtkCheckButton *checkButton;
	void (*onToggled)(uiCheckbox *, void *);
	void *onToggledData;
	gulong onToggledSignal;
};

uiUnixControlAllDefaults(uiCheckbox)

static void onToggled(GtkToggleButton *b, gpointer data)
{
	uiCheckbox *c = uiCheckbox(data);

	(*(c->onToggled))(c, c->onToggledData);
}

static void defaultOnToggled(uiCheckbox *c, void *data)
{
	// do nothing
}

char *uiCheckboxText(uiCheckbox *c)
{
	return uiUnixStrdupText(gtk_button_get_label(c->button));
}

void uiCheckboxSetText(uiCheckbox *c, const char *text)
{
	gtk_button_set_label(GTK_BUTTON(c->button), text);
}

void uiCheckboxOnToggled(uiCheckbox *c, void (*f)(uiCheckbox *, void *), void *data)
{
	c->onToggled = f;
	c->onToggledData = data;
}

int uiCheckboxChecked(uiCheckbox *c)
{
	return gtk_toggle_button_get_active(c->toggleButton) != FALSE;
}

void uiCheckboxSetChecked(uiCheckbox *c, int checked)
{
	gboolean active;

	active = FALSE;
	if (checked)
		active = TRUE;
	// we need to inhibit sending of ::toggled because this WILL send a ::toggled otherwise
	g_signal_handler_block(c->toggleButton, c->onToggledSignal);
	gtk_toggle_button_set_active(c->toggleButton, active);
	g_signal_handler_unblock(c->toggleButton, c->onToggledSignal);
}

uiCheckbox *uiNewCheckbox(const char *text)
{
	uiCheckbox *c;

	uiUnixNewControl(uiCheckbox, c);

	c->widget = gtk_check_button_new_with_label(text);
	c->button = GTK_BUTTON(c->widget);
	c->toggleButton = GTK_TOGGLE_BUTTON(c->widget);
	c->checkButton = GTK_CHECK_BUTTON(c->widget);

	c->onToggledSignal = g_signal_connect(c->widget, "toggled", G_CALLBACK(onToggled), c);
	uiCheckboxOnToggled(c, defaultOnToggled, NULL);

	return c;
}
