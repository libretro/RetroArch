// 11 june 2015
#include "uipriv_unix.h"

struct uiEditableCombobox {
	uiUnixControl c;
	GtkWidget *widget;
	GtkBin *bin;
	GtkComboBox *combobox;
	GtkComboBoxText *comboboxText;
	void (*onChanged)(uiEditableCombobox *, void *);
	void *onChangedData;
	gulong onChangedSignal;
};

uiUnixControlAllDefaults(uiEditableCombobox)

static void onChanged(GtkComboBox *cbox, gpointer data)
{
	uiEditableCombobox *c = uiEditableCombobox(data);

	(*(c->onChanged))(c, c->onChangedData);
}

static void defaultOnChanged(uiEditableCombobox *c, void *data)
{
	// do nothing
}

void uiEditableComboboxAppend(uiEditableCombobox *c, const char *text)
{
	gtk_combo_box_text_append(c->comboboxText, NULL, text);
}

char *uiEditableComboboxText(uiEditableCombobox *c)
{
	char *s;
	char *out;

	s = gtk_combo_box_text_get_active_text(c->comboboxText);
	// s will always be non-NULL in the case of a combobox with an entry (according to the source code)
	out = uiUnixStrdupText(s);
	g_free(s);
	return out;
}

void uiEditableComboboxSetText(uiEditableCombobox *c, const char *text)
{
	GtkEntry *e;

	// we need to inhibit sending of ::changed because this WILL send a ::changed otherwise
	g_signal_handler_block(c->combobox, c->onChangedSignal);
	// since there isn't a gtk_combo_box_text_set_active_text()...
	e = GTK_ENTRY(gtk_bin_get_child(c->bin));
	gtk_entry_set_text(e, text);
	g_signal_handler_unblock(c->combobox, c->onChangedSignal);
}

void uiEditableComboboxOnChanged(uiEditableCombobox *c, void (*f)(uiEditableCombobox *c, void *data), void *data)
{
	c->onChanged = f;
	c->onChangedData = data;
}

uiEditableCombobox *uiNewEditableCombobox(void)
{
	uiEditableCombobox *c;

	uiUnixNewControl(uiEditableCombobox, c);

	c->widget = gtk_combo_box_text_new_with_entry();
	c->bin = GTK_BIN(c->widget);
	c->combobox = GTK_COMBO_BOX(c->widget);
	c->comboboxText = GTK_COMBO_BOX_TEXT(c->widget);

	c->onChangedSignal = g_signal_connect(c->widget, "changed", G_CALLBACK(onChanged), c);
	uiEditableComboboxOnChanged(c, defaultOnChanged, NULL);

	return c;
}
