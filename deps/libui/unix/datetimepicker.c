// 4 september 2015
#include "uipriv_unix.h"

// LONGTERM imitate gnome-calendar's day/month/year entries above the calendar
// LONGTERM allow entering a 24-hour hour in the hour spinbutton and adjust accordingly

#define dateTimePickerWidgetType (dateTimePickerWidget_get_type())
#define dateTimePickerWidget(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), dateTimePickerWidgetType, dateTimePickerWidget))
#define isDateTimePickerWidget(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), dateTimePickerWidgetType))
#define dateTimePickerWidgetClass(class) (G_TYPE_CHECK_CLASS_CAST((class), dateTimePickerWidgetType, dateTimePickerWidgetClass))
#define isDateTimePickerWidgetClass(class) (G_TYPE_CHECK_CLASS_TYPE((class), dateTimePickerWidget))
#define getDateTimePickerWidgetClass(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), dateTimePickerWidgetType, dateTimePickerWidgetClass))

typedef struct dateTimePickerWidget dateTimePickerWidget;
typedef struct dateTimePickerWidgetClass dateTimePickerWidgetClass;

struct dateTimePickerWidget {
	GtkToggleButton parent_instance;

	gulong toggledSignal;

	gboolean hasTime;
	gboolean hasDate;

	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *calendar;
	GtkWidget *timebox;
	GtkWidget *hours;
	GtkWidget *minutes;
	GtkWidget *seconds;
	GtkWidget *ampm;

	gulong hoursBlock;
	gulong minutesBlock;
	gulong secondsBlock;
	gulong ampmBlock;

	GdkDevice *keyboard;
	GdkDevice *mouse;
};

struct dateTimePickerWidgetClass {
	GtkToggleButtonClass parent_class;
};

G_DEFINE_TYPE(dateTimePickerWidget, dateTimePickerWidget, GTK_TYPE_TOGGLE_BUTTON)

static int realSpinValue(GtkSpinButton *spinButton)
{
	GtkAdjustment *adj;

	adj = gtk_spin_button_get_adjustment(spinButton);
	return (int) gtk_adjustment_get_value(adj);
}

static void setRealSpinValue(GtkSpinButton *spinButton, int value, gulong block)
{
	GtkAdjustment *adj;

	g_signal_handler_block(spinButton, block);
	adj = gtk_spin_button_get_adjustment(spinButton);
	gtk_adjustment_set_value(adj, value);
	g_signal_handler_unblock(spinButton, block);
}

static GDateTime *selected(dateTimePickerWidget *d)
{
	// choose a day for which all times are likely to be valid for the default date in case we're only dealing with time
	guint year = 1970, month = 1, day = 1;
	guint hour = 0, minute = 0, second = 0;

	if (d->hasDate) {
		gtk_calendar_get_date(GTK_CALENDAR(d->calendar), &year, &month, &day);
		month++;		// GtkCalendar/GDateTime differences
	}
	if (d->hasTime) {
		hour = realSpinValue(GTK_SPIN_BUTTON(d->hours));
		if (realSpinValue(GTK_SPIN_BUTTON(d->ampm)) != 0)
			hour += 12;
		minute = realSpinValue(GTK_SPIN_BUTTON(d->minutes));
		second = realSpinValue(GTK_SPIN_BUTTON(d->seconds));
	}
	return g_date_time_new_local(year, month, day, hour, minute, second);
}

static void setLabel(dateTimePickerWidget *d)
{
	GDateTime *dt;
	char *fmt;
	char *msg;
	gboolean free;

	dt = selected(d);
	free = FALSE;
	if (d->hasDate && d->hasTime) {
		// don't use D_T_FMT; that's too verbose
		fmt = g_strdup_printf("%s %s", nl_langinfo(D_FMT), nl_langinfo(T_FMT));
		free = TRUE;
	} else if (d->hasDate)
		fmt = nl_langinfo(D_FMT);
	else
		fmt = nl_langinfo(T_FMT);
	msg = g_date_time_format(dt, fmt);
	gtk_button_set_label(GTK_BUTTON(d), msg);
	g_free(msg);
	if (free)
		g_free(fmt);
	g_date_time_unref(dt);
}

static void dateTimeChanged(dateTimePickerWidget *d)
{
	setLabel(d);
	// TODO fire event here
}

// we don't want ::toggled to be sent again
static void setActive(dateTimePickerWidget *d, gboolean active)
{
	g_signal_handler_block(d, d->toggledSignal);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d), active);
	g_signal_handler_unblock(d, d->toggledSignal);
}

// like startGrab() below, a lot of this is in the order that GtkComboBox does it
static void endGrab(dateTimePickerWidget *d)
{
	if (d->keyboard != NULL)
		gdk_device_ungrab(d->keyboard, GDK_CURRENT_TIME);
	gdk_device_ungrab(d->mouse, GDK_CURRENT_TIME);
	gtk_device_grab_remove(d->window, d->mouse);
	d->keyboard = NULL;
	d->mouse = NULL;
}

static void hidePopup(dateTimePickerWidget *d)
{
	endGrab(d);
	gtk_widget_hide(d->window);
	setActive(d, FALSE);
}

// this consolidates a good chunk of what GtkComboBox does
static gboolean startGrab(dateTimePickerWidget *d)
{
	GdkDevice *dev;
	guint32 time;
	GdkWindow *window;
	GdkDevice *keyboard, *mouse;

	dev = gtk_get_current_event_device();
	if (dev == NULL) {
		// this is what GtkComboBox does
		// since no device was set, just use the first available "master device"
		GdkDisplay *disp;
		GdkDeviceManager *dm;
		GList *list;

		disp = gtk_widget_get_display(GTK_WIDGET(d));
		dm = gdk_display_get_device_manager(disp);
		list = gdk_device_manager_list_devices(dm, GDK_DEVICE_TYPE_MASTER);
		dev = (GdkDevice *) (list->data);
		g_list_free(list);
	}

	time = gtk_get_current_event_time();
	keyboard = dev;
	mouse = gdk_device_get_associated_device(dev);
	if (gdk_device_get_source(dev) != GDK_SOURCE_KEYBOARD) {
		dev = mouse;
		mouse = keyboard;
		keyboard = dev;
	}

	window = gtk_widget_get_window(d->window);
	if (keyboard != NULL)
		if (gdk_device_grab(keyboard, window,
			GDK_OWNERSHIP_WINDOW, TRUE,
			GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
			NULL, time) != GDK_GRAB_SUCCESS)
			return FALSE;
	if (mouse != NULL)
		if (gdk_device_grab(mouse, window,
			GDK_OWNERSHIP_WINDOW, TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
			NULL, time) != GDK_GRAB_SUCCESS) {
			if (keyboard != NULL)
				gdk_device_ungrab(keyboard, time);
			return FALSE;
		}

	gtk_device_grab_add(d->window, mouse, TRUE);
	d->keyboard = keyboard;
	d->mouse = mouse;
	return TRUE;
}

// based on gtk_combo_box_list_position() in the GTK+ source code
static void allocationToScreen(dateTimePickerWidget *d, gint *x, gint *y)
{
	GdkWindow *window;
	GtkAllocation a;
	GtkRequisition aWin;
	GdkScreen *screen;
	GdkRectangle workarea;
	int otherY;

	gtk_widget_get_allocation(GTK_WIDGET(d), &a);
	gtk_widget_get_preferred_size(d->window, &aWin, NULL);
	*x = 0;
	*y = 0;
	if (!gtk_widget_get_has_window(GTK_WIDGET(d))) {
		*x = a.x;
		*y = a.y;
	}
	window = gtk_widget_get_window(GTK_WIDGET(d));
	gdk_window_get_root_coords(window, *x, *y, x, y);
	if (gtk_widget_get_direction(GTK_WIDGET(d)) == GTK_TEXT_DIR_RTL)
		*x += a.width - aWin.width;

	// now adjust to prevent the box from going offscreen
	screen = gtk_widget_get_screen(GTK_WIDGET(d));
	gdk_screen_get_monitor_workarea(screen,
		gdk_screen_get_monitor_at_window(screen, window),
		&workarea);
	if (*x < workarea.x)					// too far to the left?
		*x = workarea.x;
	else if (*x + aWin.width > (workarea.x + workarea.width))	// too far to the right?
		*x = (workarea.x + workarea.width) - aWin.width;
	// this isn't the same algorithm used by GtkComboBox
	// first, get our two choices; *y for down and otherY for up
	otherY = *y - aWin.height;
	*y += a.height;
	// and use otherY if we're too low
	if (*y + aWin.height >= workarea.y + workarea.height)
		*y = otherY;
}

static void showPopup(dateTimePickerWidget *d)
{
	GtkWidget *toplevel;
	gint x, y;

	// GtkComboBox does it
	toplevel = gtk_widget_get_toplevel(GTK_WIDGET(d));
	if (GTK_IS_WINDOW(toplevel))
		gtk_window_group_add_window(gtk_window_get_group(GTK_WINDOW(toplevel)), GTK_WINDOW(d->window));

	allocationToScreen(d, &x, &y);
	gtk_window_move(GTK_WINDOW(d->window), x, y);

	gtk_widget_show(d->window);
	setActive(d, TRUE);

	if (!startGrab(d))
		hidePopup(d);
}

static void onToggled(GtkToggleButton *b, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(b);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d)))
		showPopup(d);
	else
		hidePopup(d);
}

static gboolean grabBroken(GtkWidget *w, GdkEventGrabBroken *e, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);

	hidePopup(d);
	return TRUE;		// this is what GtkComboBox does
}

static gboolean buttonReleased(GtkWidget *w, GdkEventButton *e, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);
	int winx, winy;
	GtkAllocation wina;
	gboolean in;

	gtk_widget_get_allocation(d->window, &wina);
	winx = 0;
	winy = 0;
	if (!gtk_widget_get_has_window(d->window)) {
		winx = wina.x;
		winy = wina.y;
	}
	gdk_window_get_root_coords(gtk_widget_get_window(d->window), winx, winy, &winx, &winy);
	in = TRUE;
	if (e->x_root < winx)
		in = FALSE;
	if (e->x_root >= (winx + wina.width))
		in = FALSE;
	if (e->y_root < winy)
		in = FALSE;
	if (e->y_root >= (winy + wina.height))
		in = FALSE;
	if (!in)
		hidePopup(d);
	return TRUE;		// this is what GtkComboBox does
}

static gint hoursSpinboxInput(GtkSpinButton *sb, gpointer ptr, gpointer data)
{
	double *out = (double *) ptr;
	const gchar *text;
	int value;

	text = gtk_entry_get_text(GTK_ENTRY(sb));
	value = (int) g_strtod(text, NULL);
	if (value < 0 || value > 12)
		return GTK_INPUT_ERROR;
	if (value == 12)		// 12 to the user is 0 internally
		value = 0;
	*out = (double) value;
	return TRUE;
}

static gboolean hoursSpinboxOutput(GtkSpinButton *sb, gpointer data)
{
	gchar *text;
	int value;

	value = realSpinValue(sb);
	if (value == 0)		// 0 internally is 12 to the user
		value = 12;
	text = g_strdup_printf("%d", value);
	gtk_entry_set_text(GTK_ENTRY(sb), text);
	g_free(text);
	return TRUE;
}

static gboolean zeroPadSpinbox(GtkSpinButton *sb, gpointer data)
{
	gchar *text;
	int value;

	value = realSpinValue(sb);
	text = g_strdup_printf("%02d", value);
	gtk_entry_set_text(GTK_ENTRY(sb), text);
	g_free(text);
	return TRUE;
}

// this is really hacky but we can't use GtkCombobox here :(
static gint ampmSpinboxInput(GtkSpinButton *sb, gpointer ptr, gpointer data)
{
	double *out = (double *) ptr;
	const gchar *text;
	char firstAM, firstPM;

	text = gtk_entry_get_text(GTK_ENTRY(sb));
	// LONGTERM don't use ASCII here for case insensitivity
	firstAM = g_ascii_tolower(nl_langinfo(AM_STR)[0]);
	firstPM = g_ascii_tolower(nl_langinfo(PM_STR)[0]);
	for (; *text != '\0'; text++)
		if (g_ascii_tolower(*text) == firstAM) {
			*out = 0;
			return TRUE;
		} else if (g_ascii_tolower(*text) == firstPM) {
			*out = 1;
			return TRUE;
		}
	return GTK_INPUT_ERROR;
}

static gboolean ampmSpinboxOutput(GtkSpinButton *sb, gpointer data)
{
	int value;

	value = gtk_spin_button_get_value_as_int(sb);
	if (value == 0)
		gtk_entry_set_text(GTK_ENTRY(sb), nl_langinfo(AM_STR));
	else
		gtk_entry_set_text(GTK_ENTRY(sb), nl_langinfo(PM_STR));
	return TRUE;
}

static void spinboxChanged(GtkSpinButton *sb, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);

	dateTimeChanged(d);
}

static GtkWidget *newSpinbox(dateTimePickerWidget *d, int min, int max, gint (*input)(GtkSpinButton *, gpointer, gpointer), gboolean (*output)(GtkSpinButton *, gpointer), gulong *block)
{
	GtkWidget *sb;

	sb = gtk_spin_button_new_with_range(min, max, 1);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(sb), 0);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_orientable_set_orientation(GTK_ORIENTABLE(sb), GTK_ORIENTATION_VERTICAL);
	*block = g_signal_connect(sb, "value-changed", G_CALLBACK(spinboxChanged), d);
	if (input != NULL)
		g_signal_connect(sb, "input", G_CALLBACK(input), NULL);
	if (output != NULL)
		g_signal_connect(sb, "output", G_CALLBACK(output), NULL);
	return sb;
}

static void dateChanged(GtkCalendar *c, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);

	dateTimeChanged(d);
}

static void setDateOnly(dateTimePickerWidget *d)
{
	d->hasTime = FALSE;
	gtk_container_remove(GTK_CONTAINER(d->box), d->timebox);
}

static void setTimeOnly(dateTimePickerWidget *d)
{
	d->hasDate = FALSE;
	gtk_container_remove(GTK_CONTAINER(d->box), d->calendar);
}

static void dateTimePickerWidget_init(dateTimePickerWidget *d)
{
	GDateTime *dt;
	gint year, month, day;
	gint hour;
	gulong calendarBlock;

	d->window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_resizable(GTK_WINDOW(d->window), FALSE);
	gtk_window_set_attached_to(GTK_WINDOW(d->window), GTK_WIDGET(d));
	gtk_window_set_decorated(GTK_WINDOW(d->window), FALSE);
	gtk_window_set_deletable(GTK_WINDOW(d->window), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(d->window), GDK_WINDOW_TYPE_HINT_COMBO);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(d->window), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(d->window), TRUE);
	gtk_window_set_has_resize_grip(GTK_WINDOW(d->window), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(d->window), 12);
	// and make it stand out a bit
	gtk_style_context_add_class(gtk_widget_get_style_context(d->window), "frame");

	d->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add(GTK_CONTAINER(d->window), d->box);

	d->calendar = gtk_calendar_new();
	calendarBlock = g_signal_connect(d->calendar, "day-selected", G_CALLBACK(dateChanged), d);
	gtk_container_add(GTK_CONTAINER(d->box), d->calendar);

	d->timebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_valign(d->timebox, GTK_ALIGN_CENTER);
	gtk_container_add(GTK_CONTAINER(d->box), d->timebox);

	d->hours = newSpinbox(d, 0, 11, hoursSpinboxInput, hoursSpinboxOutput, &(d->hoursBlock));
	gtk_container_add(GTK_CONTAINER(d->timebox), d->hours);

	gtk_container_add(GTK_CONTAINER(d->timebox),
		gtk_label_new(":"));

	d->minutes = newSpinbox(d, 0, 59, NULL, zeroPadSpinbox, &(d->minutesBlock));
	gtk_container_add(GTK_CONTAINER(d->timebox), d->minutes);

	gtk_container_add(GTK_CONTAINER(d->timebox),
		gtk_label_new(":"));

	d->seconds = newSpinbox(d, 0, 59, NULL, zeroPadSpinbox, &(d->secondsBlock));
	gtk_container_add(GTK_CONTAINER(d->timebox), d->seconds);

	// LONGTERM this should be the case, but that interferes with grabs
	// switch to it when we can drop GTK+ 3.10 and use popovers
#if 0
	d->ampm = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(d->ampm), NULL, "AM");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(d->ampm), NULL, "PM");
#endif
	d->ampm = newSpinbox(d, 0, 1, ampmSpinboxInput, ampmSpinboxOutput, &(d->ampmBlock));
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(d->ampm), FALSE);
	gtk_widget_set_valign(d->ampm, GTK_ALIGN_CENTER);
	gtk_container_add(GTK_CONTAINER(d->timebox), d->ampm);

	gtk_widget_show_all(d->box);

	g_signal_connect(d->window, "grab-broken-event", G_CALLBACK(grabBroken), d);
	g_signal_connect(d->window, "button-release-event", G_CALLBACK(buttonReleased), d);

	d->toggledSignal = g_signal_connect(d, "toggled", G_CALLBACK(onToggled), NULL);
	d->keyboard = NULL;
	d->mouse = NULL;

	d->hasTime = TRUE;
	d->hasDate = TRUE;

	// set the current date/time
	// notice how we block signals from firing
	dt = g_date_time_new_now_local();
	g_date_time_get_ymd(dt, &year, &month, &day);
	month--;			// GDateTime/GtkCalendar differences
	g_signal_handler_block(d->calendar, calendarBlock);
	gtk_calendar_select_month(GTK_CALENDAR(d->calendar), month, year);
	gtk_calendar_select_day(GTK_CALENDAR(d->calendar), day);
	g_signal_handler_unblock(d->calendar, calendarBlock);
	hour = g_date_time_get_hour(dt);
	if (hour >= 12) {
		hour -= 12;
		setRealSpinValue(GTK_SPIN_BUTTON(d->ampm), 1, d->ampmBlock);
	}
	setRealSpinValue(GTK_SPIN_BUTTON(d->hours), hour, d->hoursBlock);
	setRealSpinValue(GTK_SPIN_BUTTON(d->minutes), g_date_time_get_minute(dt), d->minutesBlock);
	setRealSpinValue(GTK_SPIN_BUTTON(d->seconds), g_date_time_get_seconds(dt), d->secondsBlock);
	g_date_time_unref(dt);
}

static void dateTimePickerWidget_dispose(GObject *obj)
{
	dateTimePickerWidget *d = dateTimePickerWidget(obj);

	if (d->window != NULL) {
		gtk_widget_destroy(d->window);
		d->window = NULL;
	}
	G_OBJECT_CLASS(dateTimePickerWidget_parent_class)->dispose(obj);
}

static void dateTimePickerWidget_finalize(GObject *obj)
{
	G_OBJECT_CLASS(dateTimePickerWidget_parent_class)->finalize(obj);
}

static void dateTimePickerWidget_class_init(dateTimePickerWidgetClass *class)
{
	G_OBJECT_CLASS(class)->dispose = dateTimePickerWidget_dispose;
	G_OBJECT_CLASS(class)->finalize = dateTimePickerWidget_finalize;
}

static GtkWidget *newDTP(void)
{
	GtkWidget *w;

	w = GTK_WIDGET(g_object_new(dateTimePickerWidgetType, "label", "", NULL));
	setLabel(dateTimePickerWidget(w));
	return w;
}

static GtkWidget *newDP(void)
{
	GtkWidget *w;

	w = GTK_WIDGET(g_object_new(dateTimePickerWidgetType, "label", "", NULL));
	setDateOnly(dateTimePickerWidget(w));
	setLabel(dateTimePickerWidget(w));
	return w;
}

static GtkWidget *newTP(void)
{
	GtkWidget *w;

	w = GTK_WIDGET(g_object_new(dateTimePickerWidgetType, "label", "", NULL));
	setTimeOnly(dateTimePickerWidget(w));
	setLabel(dateTimePickerWidget(w));
	return w;
}

struct uiDateTimePicker {
	uiUnixControl c;
	GtkWidget *widget;
	dateTimePickerWidget *d;
};

uiUnixControlAllDefaults(uiDateTimePicker)

uiDateTimePicker *finishNewDateTimePicker(GtkWidget *(*fn)(void))
{
	uiDateTimePicker *d;

	uiUnixNewControl(uiDateTimePicker, d);

	d->widget = (*fn)();
	d->d = dateTimePickerWidget(d->widget);

	return d;
}

uiDateTimePicker *uiNewDateTimePicker(void)
{
	return finishNewDateTimePicker(newDTP);
}

uiDateTimePicker *uiNewDatePicker(void)
{
	return finishNewDateTimePicker(newDP);
}

uiDateTimePicker *uiNewTimePicker(void)
{
	return finishNewDateTimePicker(newTP);
}
