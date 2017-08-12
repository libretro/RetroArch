// 6 december 2015
#include "uipriv_unix.h"

struct uiMultilineEntry {
	uiUnixControl c;
	GtkWidget *widget;
	GtkContainer *scontainer;
	GtkScrolledWindow *sw;
	GtkWidget *textviewWidget;
	GtkTextView *textview;
	GtkTextBuffer *textbuf;
	void (*onChanged)(uiMultilineEntry *, void *);
	void *onChangedData;
	gulong onChangedSignal;
};

uiUnixControlAllDefaults(uiMultilineEntry)

static void onChanged(GtkTextBuffer *textbuf, gpointer data)
{
	uiMultilineEntry *e = uiMultilineEntry(data);

	(*(e->onChanged))(e, e->onChangedData);
}

static void defaultOnChanged(uiMultilineEntry *e, void *data)
{
	// do nothing
}

char *uiMultilineEntryText(uiMultilineEntry *e)
{
	GtkTextIter start, end;
	char *tret, *out;

	gtk_text_buffer_get_start_iter(e->textbuf, &start);
	gtk_text_buffer_get_end_iter(e->textbuf, &end);
	tret = gtk_text_buffer_get_text(e->textbuf, &start, &end, TRUE);
	// theoretically we could just return tret because uiUnixStrdupText() is just g_strdup(), but if that ever changes we can't, so let's do it this way to be safe
	out = uiUnixStrdupText(tret);
	g_free(tret);
	return out;
}

void uiMultilineEntrySetText(uiMultilineEntry *e, const char *text)
{
	// we need to inhibit sending of ::changed because this WILL send a ::changed otherwise
	g_signal_handler_block(e->textbuf, e->onChangedSignal);
	gtk_text_buffer_set_text(e->textbuf, text, -1);
	g_signal_handler_unblock(e->textbuf, e->onChangedSignal);
}

// TODO scroll to end?
void uiMultilineEntryAppend(uiMultilineEntry *e, const char *text)
{
	GtkTextIter end;

	gtk_text_buffer_get_end_iter(e->textbuf, &end);
	// we need to inhibit sending of ::changed because this WILL send a ::changed otherwise
	g_signal_handler_block(e->textbuf, e->onChangedSignal);
	gtk_text_buffer_insert(e->textbuf, &end, text, -1);
	g_signal_handler_unblock(e->textbuf, e->onChangedSignal);
}

void uiMultilineEntryOnChanged(uiMultilineEntry *e, void (*f)(uiMultilineEntry *e, void *data), void *data)
{
	e->onChanged = f;
	e->onChangedData = data;
}

int uiMultilineEntryReadOnly(uiMultilineEntry *e)
{
	return gtk_text_view_get_editable(e->textview) == FALSE;
}

void uiMultilineEntrySetReadOnly(uiMultilineEntry *e, int readonly)
{
	gboolean editable;

	editable = TRUE;
	if (readonly)
		editable = FALSE;
	gtk_text_view_set_editable(e->textview, editable);
}

static uiMultilineEntry *finishMultilineEntry(GtkPolicyType hpolicy, GtkWrapMode wrapMode)
{
	uiMultilineEntry *e;

	uiUnixNewControl(uiMultilineEntry, e);

	e->widget = gtk_scrolled_window_new(NULL, NULL);
	e->scontainer = GTK_CONTAINER(e->widget);
	e->sw = GTK_SCROLLED_WINDOW(e->widget);
	gtk_scrolled_window_set_policy(e->sw,
		hpolicy,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(e->sw, GTK_SHADOW_IN);

	e->textviewWidget = gtk_text_view_new();
	e->textview = GTK_TEXT_VIEW(e->textviewWidget);
	gtk_text_view_set_wrap_mode(e->textview, wrapMode);

	gtk_container_add(e->scontainer, e->textviewWidget);
	// and make the text view visible; only the scrolled window's visibility is controlled by libui
	gtk_widget_show(e->textviewWidget);

	e->textbuf = gtk_text_view_get_buffer(e->textview);

	e->onChangedSignal = g_signal_connect(e->textbuf, "changed", G_CALLBACK(onChanged), e);
	uiMultilineEntryOnChanged(e, defaultOnChanged, NULL);

	return e;
}

uiMultilineEntry *uiNewMultilineEntry(void)
{
	return finishMultilineEntry(GTK_POLICY_NEVER, GTK_WRAP_WORD);
}

uiMultilineEntry *uiNewNonWrappingMultilineEntry(void)
{
	return finishMultilineEntry(GTK_POLICY_AUTOMATIC, GTK_WRAP_NONE);
}
