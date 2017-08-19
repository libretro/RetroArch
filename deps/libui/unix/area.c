// 4 september 2015
#include "uipriv_unix.h"

// notes:
// - G_DECLARE_DERIVABLE/FINAL_INTERFACE() requires glib 2.44 and that's starting with debian stretch (testing) (GTK+ 3.18) and ubuntu 15.04 (GTK+ 3.14) - debian jessie has 2.42 (GTK+ 3.14)
#define areaWidgetType (areaWidget_get_type())
#define areaWidget(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), areaWidgetType, areaWidget))
#define isAreaWidget(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), areaWidgetType))
#define areaWidgetClass(class) (G_TYPE_CHECK_CLASS_CAST((class), areaWidgetType, areaWidgetClass))
#define isAreaWidgetClass(class) (G_TYPE_CHECK_CLASS_TYPE((class), areaWidget))
#define getAreaWidgetClass(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), areaWidgetType, areaWidgetClass))

typedef struct areaWidget areaWidget;
typedef struct areaWidgetClass areaWidgetClass;

struct areaWidget {
	GtkDrawingArea parent_instance;
	uiArea *a;
	// construct-only parameters aare not set until after the init() function has returned
	// we need this particular object available during init(), so put it here instead of in uiArea
	// keep a pointer in uiArea for convenience, though
	clickCounter cc;
};

struct areaWidgetClass {
	GtkDrawingAreaClass parent_class;
};

struct uiArea {
	uiUnixControl c;
	GtkWidget *widget;		// either swidget or areaWidget depending on whether it is scrolling

	GtkWidget *swidget;
	GtkContainer *scontainer;
	GtkScrolledWindow *sw;

	GtkWidget *areaWidget;
	GtkDrawingArea *drawingArea;
	areaWidget *area;

	uiAreaHandler *ah;

	gboolean scrolling;
	int scrollWidth;
	int scrollHeight;

	// note that this is a pointer; see above
	clickCounter *cc;

	// for user window drags
	GdkEventButton *dragevent;
};

G_DEFINE_TYPE(areaWidget, areaWidget, GTK_TYPE_DRAWING_AREA)

static void areaWidget_init(areaWidget *aw)
{
	// for events
	gtk_widget_add_events(GTK_WIDGET(aw),
		GDK_POINTER_MOTION_MASK |
		GDK_BUTTON_MOTION_MASK |
		GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK |
		GDK_KEY_PRESS_MASK |
		GDK_KEY_RELEASE_MASK |
		GDK_ENTER_NOTIFY_MASK |
		GDK_LEAVE_NOTIFY_MASK);

	gtk_widget_set_can_focus(GTK_WIDGET(aw), TRUE);

	clickCounterReset(&(aw->cc));
}

static void areaWidget_dispose(GObject *obj)
{
	G_OBJECT_CLASS(areaWidget_parent_class)->dispose(obj);
}

static void areaWidget_finalize(GObject *obj)
{
	G_OBJECT_CLASS(areaWidget_parent_class)->finalize(obj);
}

static void areaWidget_size_allocate(GtkWidget *w, GtkAllocation *allocation)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	// GtkDrawingArea has a size_allocate() implementation; we need to call it
	// this will call gtk_widget_set_allocation() for us
	GTK_WIDGET_CLASS(areaWidget_parent_class)->size_allocate(w, allocation);

	if (!a->scrolling)
		// we must redraw everything on resize because Windows requires it
		gtk_widget_queue_resize(w);
}

static void loadAreaSize(uiArea *a, double *width, double *height)
{
	GtkAllocation allocation;

	*width = 0;
	*height = 0;
	// don't provide size information for scrolling areas
	if (!a->scrolling) {
		gtk_widget_get_allocation(a->areaWidget, &allocation);
		// these are already in drawing space coordinates
		// for drawing, the size of drawing space has the same value as the widget allocation
		// thanks to tristan in irc.gimp.net/#gtk+
		*width = allocation.width;
		*height = allocation.height;
	}
}

static gboolean areaWidget_draw(GtkWidget *w, cairo_t *cr)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	uiAreaDrawParams dp;
	double clipX0, clipY0, clipX1, clipY1;

	dp.Context = newContext(cr);

	loadAreaSize(a, &(dp.AreaWidth), &(dp.AreaHeight));

	cairo_clip_extents(cr, &clipX0, &clipY0, &clipX1, &clipY1);
	dp.ClipX = clipX0;
	dp.ClipY = clipY0;
	dp.ClipWidth = clipX1 - clipX0;
	dp.ClipHeight = clipY1 - clipY0;

	// no need to save or restore the graphics state to reset transformations; GTK+ does that for us
	(*(a->ah->Draw))(a->ah, a, &dp);

	freeContext(dp.Context);
	return FALSE;
}

// to do this properly for scrolling areas, we need to
// - return the same value for min and nat
// - call gtk_widget_queue_resize() when the size changes
// thanks to Company in irc.gimp.net/#gtk+
static void areaWidget_get_preferred_height(GtkWidget *w, gint *min, gint *nat)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	// always chain up just in case
	GTK_WIDGET_CLASS(areaWidget_parent_class)->get_preferred_height(w, min, nat);
	if (a->scrolling) {
		*min = a->scrollHeight;
		*nat = a->scrollHeight;
	}
}

static void areaWidget_get_preferred_width(GtkWidget *w, gint *min, gint *nat)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	// always chain up just in case
	GTK_WIDGET_CLASS(areaWidget_parent_class)->get_preferred_width(w, min, nat);
	if (a->scrolling) {
		*min = a->scrollWidth;
		*nat = a->scrollWidth;
	}
}

static guint translateModifiers(guint state, GdkWindow *window)
{
	GdkModifierType statetype;

	// GDK doesn't initialize the modifier flags fully; we have to explicitly tell it to (thanks to Daniel_S and daniels (two different people) in irc.gimp.net/#gtk+)
	statetype = state;
	gdk_keymap_add_virtual_modifiers(
		gdk_keymap_get_for_display(gdk_window_get_display(window)),
		&statetype);
	return statetype;
}

static uiModifiers toModifiers(guint state)
{
	uiModifiers m;

	m = 0;
	if ((state & GDK_CONTROL_MASK) != 0)
		m |= uiModifierCtrl;
	if ((state & GDK_META_MASK) != 0)
		m |= uiModifierAlt;
	if ((state & GDK_MOD1_MASK) != 0)		// GTK+ itself requires this to be Alt (just read through gtkaccelgroup.c)
		m |= uiModifierAlt;
	if ((state & GDK_SHIFT_MASK) != 0)
		m |= uiModifierShift;
	if ((state & GDK_SUPER_MASK) != 0)
		m |= uiModifierSuper;
	return m;
}

// capture on drag is done automatically on GTK+
static void finishMouseEvent(uiArea *a, uiAreaMouseEvent *me, guint mb, gdouble x, gdouble y, guint state, GdkWindow *window)
{
	// on GTK+, mouse buttons 4-7 are for scrolling; if we got here, that's a mistake
	if (mb >= 4 && mb <= 7)
		return;
	// if the button ID >= 8, continue counting from 4, as in the MouseEvent spec
	if (me->Down >= 8)
		me->Down -= 4;
	if (me->Up >= 8)
		me->Up -= 4;

	state = translateModifiers(state, window);
	me->Modifiers = toModifiers(state);

	// the mb != # checks exclude the Up/Down button from Held
	me->Held1To64 = 0;
	if (mb != 1 && (state & GDK_BUTTON1_MASK) != 0)
		me->Held1To64 |= 1 << 0;
	if (mb != 2 && (state & GDK_BUTTON2_MASK) != 0)
		me->Held1To64 |= 1 << 1;
	if (mb != 3 && (state & GDK_BUTTON3_MASK) != 0)
		me->Held1To64 |= 1 << 2;
	// don't check GDK_BUTTON4_MASK or GDK_BUTTON5_MASK because those are for the scrolling buttons mentioned above
	// GDK expressly does not support any more buttons in the GdkModifierType; see https://git.gnome.org/browse/gtk+/tree/gdk/x11/gdkdevice-xi2.c#n763 (thanks mclasen in irc.gimp.net/#gtk+)

	// these are already in drawing space coordinates
	// the size of drawing space has the same value as the widget allocation
	// thanks to tristan in irc.gimp.net/#gtk+
	me->X = x;
	me->Y = y;

	loadAreaSize(a, &(me->AreaWidth), &(me->AreaHeight));

	(*(a->ah->MouseEvent))(a->ah, a, me);
}

static gboolean areaWidget_button_press_event(GtkWidget *w, GdkEventButton *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	gint maxTime, maxDistance;
	GtkSettings *settings;
	uiAreaMouseEvent me;

	// clicking doesn't automatically transfer keyboard focus; we must do so manually (thanks tristan in irc.gimp.net/#gtk+)
	gtk_widget_grab_focus(w);

	// we handle multiple clicks ourselves here, in the same way as we do on Windows
	if (e->type != GDK_BUTTON_PRESS)
		// ignore GDK's generated double-clicks and beyond
		return GDK_EVENT_PROPAGATE;
	settings = gtk_widget_get_settings(w);
	g_object_get(settings,
		"gtk-double-click-time", &maxTime,
		"gtk-double-click-distance", &maxDistance,
		NULL);
	// don't unref settings; it's transfer-none (thanks gregier in irc.gimp.net/#gtk+)
	// e->time is guint32
	// e->x and e->y are floating-point; just make them 32-bit integers
	// maxTime and maxDistance... are gint, which *should* fit, hopefully...
	me.Count = clickCounterClick(a->cc, me.Down,
		e->x, e->y,
		e->time, maxTime,
		maxDistance, maxDistance);

	me.Down = e->button;
	me.Up = 0;

	// and set things up for window drags
	a->dragevent = e;
	finishMouseEvent(a, &me, e->button, e->x, e->y, e->state, e->window);
	a->dragevent = NULL;
	return GDK_EVENT_PROPAGATE;
}

static gboolean areaWidget_button_release_event(GtkWidget *w, GdkEventButton *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	uiAreaMouseEvent me;

	me.Down = 0;
	me.Up = e->button;
	me.Count = 0;
	finishMouseEvent(a, &me, e->button, e->x, e->y, e->state, e->window);
	return GDK_EVENT_PROPAGATE;
}

static gboolean areaWidget_motion_notify_event(GtkWidget *w, GdkEventMotion *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	uiAreaMouseEvent me;

	me.Down = 0;
	me.Up = 0;
	me.Count = 0;
	finishMouseEvent(a, &me, 0, e->x, e->y, e->state, e->window);
	return GDK_EVENT_PROPAGATE;
}

// we want switching away from the control to reset the double-click counter, like with WM_ACTIVATE on Windows
// according to tristan in irc.gimp.net/#gtk+, doing this on both enter-notify-event and leave-notify-event is correct (and it seems to be true in my own tests; plus the events DO get sent when switching programs with the keyboard (just pointing that out))
static gboolean onCrossing(areaWidget *aw, int left)
{
	uiArea *a = aw->a;

	(*(a->ah->MouseCrossed))(a->ah, a, left);
	clickCounterReset(a->cc);
	return GDK_EVENT_PROPAGATE;
}

static gboolean areaWidget_enter_notify_event(GtkWidget *w, GdkEventCrossing *e)
{
	return onCrossing(areaWidget(w), 0);
}

static gboolean areaWidget_leave_notify_event(GtkWidget *w, GdkEventCrossing *e)
{
	return onCrossing(areaWidget(w), 1);
}

// note: there is no equivalent to WM_CAPTURECHANGED on GTK+; there literally is no way to break a grab like that (at least not on X11 and Wayland)
// even if I invoke the task switcher and switch processes, the mouse grab will still be held until I let go of all buttons
// therefore, no DragBroken()

// we use GDK_KEY_Print as a sentinel because libui will never support the print screen key; that key belongs to the user

static const struct {
	guint keyval;
	uiExtKey extkey;
} extKeys[] = {
	{ GDK_KEY_Escape, uiExtKeyEscape },
	{ GDK_KEY_Insert, uiExtKeyInsert },
	{ GDK_KEY_Delete, uiExtKeyDelete },
	{ GDK_KEY_Home, uiExtKeyHome },
	{ GDK_KEY_End, uiExtKeyEnd },
	{ GDK_KEY_Page_Up, uiExtKeyPageUp },
	{ GDK_KEY_Page_Down, uiExtKeyPageDown },
	{ GDK_KEY_Up, uiExtKeyUp },
	{ GDK_KEY_Down, uiExtKeyDown },
	{ GDK_KEY_Left, uiExtKeyLeft },
	{ GDK_KEY_Right, uiExtKeyRight },
	{ GDK_KEY_F1, uiExtKeyF1 },
	{ GDK_KEY_F2, uiExtKeyF2 },
	{ GDK_KEY_F3, uiExtKeyF3 },
	{ GDK_KEY_F4, uiExtKeyF4 },
	{ GDK_KEY_F5, uiExtKeyF5 },
	{ GDK_KEY_F6, uiExtKeyF6 },
	{ GDK_KEY_F7, uiExtKeyF7 },
	{ GDK_KEY_F8, uiExtKeyF8 },
	{ GDK_KEY_F9, uiExtKeyF9 },
	{ GDK_KEY_F10, uiExtKeyF10 },
	{ GDK_KEY_F11, uiExtKeyF11 },
	{ GDK_KEY_F12, uiExtKeyF12 },
	// numpad numeric keys and . are handled in events.c
	{ GDK_KEY_KP_Enter, uiExtKeyNEnter },
	{ GDK_KEY_KP_Add, uiExtKeyNAdd },
	{ GDK_KEY_KP_Subtract, uiExtKeyNSubtract },
	{ GDK_KEY_KP_Multiply, uiExtKeyNMultiply },
	{ GDK_KEY_KP_Divide, uiExtKeyNDivide },
	{ GDK_KEY_Print, 0 },
};

static const struct {
	guint keyval;
	uiModifiers mod;
} modKeys[] = {
	{ GDK_KEY_Control_L, uiModifierCtrl },
	{ GDK_KEY_Control_R, uiModifierCtrl },
	{ GDK_KEY_Alt_L, uiModifierAlt },
	{ GDK_KEY_Alt_R, uiModifierAlt },
	{ GDK_KEY_Meta_L, uiModifierAlt },
	{ GDK_KEY_Meta_R, uiModifierAlt },
	{ GDK_KEY_Shift_L, uiModifierShift },
	{ GDK_KEY_Shift_R, uiModifierShift },
	{ GDK_KEY_Super_L, uiModifierSuper },
	{ GDK_KEY_Super_R, uiModifierSuper },
	{ GDK_KEY_Print, 0 },
};

static int areaKeyEvent(uiArea *a, int up, GdkEventKey *e)
{
	uiAreaKeyEvent ke;
	guint state;
	int i;

	ke.Key = 0;
	ke.ExtKey = 0;
	ke.Modifier = 0;

	state = translateModifiers(e->state, e->window);
	ke.Modifiers = toModifiers(state);

	ke.Up = up;

	for (i = 0; extKeys[i].keyval != GDK_KEY_Print; i++)
		if (extKeys[i].keyval == e->keyval) {
			ke.ExtKey = extKeys[i].extkey;
			goto keyFound;
		}

	for (i = 0; modKeys[i].keyval != GDK_KEY_Print; i++)
		if (modKeys[i].keyval == e->keyval) {
			ke.Modifier = modKeys[i].mod;
			// don't include the modifier in ke.Modifiers
			ke.Modifiers &= ~ke.Modifier;
			goto keyFound;
		}

	if (fromScancode(e->hardware_keycode - 8, &ke))
		goto keyFound;

	// no supported key found; treat as unhandled
	return 0;

keyFound:
	return (*(a->ah->KeyEvent))(a->ah, a, &ke);
}

static gboolean areaWidget_key_press_event(GtkWidget *w, GdkEventKey *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	if (areaKeyEvent(a, 0, e))
		return GDK_EVENT_STOP;
	return GDK_EVENT_PROPAGATE;
}

static gboolean areaWidget_key_release_event(GtkWidget *w, GdkEventKey *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	if (areaKeyEvent(a, 1, e))
		return GDK_EVENT_STOP;
	return GDK_EVENT_PROPAGATE;
}

enum {
	pArea = 1,
	nProps,
};

static GParamSpec *pspecArea;

static void areaWidget_set_property(GObject *obj, guint prop, const GValue *value, GParamSpec *pspec)
{
	areaWidget *aw = areaWidget(obj);

	switch (prop) {
	case pArea:
		aw->a = (uiArea *) g_value_get_pointer(value);
		aw->a->cc = &(aw->cc);
		return;
	}
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
}

static void areaWidget_get_property(GObject *obj, guint prop, GValue *value, GParamSpec *pspec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
}

static void areaWidget_class_init(areaWidgetClass *class)
{
	G_OBJECT_CLASS(class)->dispose = areaWidget_dispose;
	G_OBJECT_CLASS(class)->finalize = areaWidget_finalize;
	G_OBJECT_CLASS(class)->set_property = areaWidget_set_property;
	G_OBJECT_CLASS(class)->get_property = areaWidget_get_property;

	GTK_WIDGET_CLASS(class)->size_allocate = areaWidget_size_allocate;
	GTK_WIDGET_CLASS(class)->draw = areaWidget_draw;
	GTK_WIDGET_CLASS(class)->get_preferred_height = areaWidget_get_preferred_height;
	GTK_WIDGET_CLASS(class)->get_preferred_width = areaWidget_get_preferred_width;
	GTK_WIDGET_CLASS(class)->button_press_event = areaWidget_button_press_event;
	GTK_WIDGET_CLASS(class)->button_release_event = areaWidget_button_release_event;
	GTK_WIDGET_CLASS(class)->motion_notify_event = areaWidget_motion_notify_event;
	GTK_WIDGET_CLASS(class)->enter_notify_event = areaWidget_enter_notify_event;
	GTK_WIDGET_CLASS(class)->leave_notify_event = areaWidget_leave_notify_event;
	GTK_WIDGET_CLASS(class)->key_press_event = areaWidget_key_press_event;
	GTK_WIDGET_CLASS(class)->key_release_event = areaWidget_key_release_event;

	pspecArea = g_param_spec_pointer("libui-area",
		"libui-area",
		"uiArea.",
		G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property(G_OBJECT_CLASS(class), pArea, pspecArea);
}

// control implementation

uiUnixControlAllDefaults(uiArea)

void uiAreaSetSize(uiArea *a, int width, int height)
{
	if (!a->scrolling)
		userbug("You cannot call uiAreaSetSize() on a non-scrolling uiArea. (area: %p)", a);
	a->scrollWidth = width;
	a->scrollHeight = height;
	gtk_widget_queue_resize(a->areaWidget);
}

void uiAreaQueueRedrawAll(uiArea *a)
{
	gtk_widget_queue_draw(a->areaWidget);
}

void uiAreaScrollTo(uiArea *a, double x, double y, double width, double height)
{
	// TODO
	// TODO adjust adjustments and find source for that
}

void uiAreaBeginUserWindowMove(uiArea *a)
{
	GtkWidget *toplevel;

	if (a->dragevent == NULL)
		userbug("cannot call uiAreaBeginUserWindowMove() outside of a Mouse() with Down != 0");
	// TODO don't we have a libui function for this? did I scrap it?
	// TODO widget or areaWidget?
	toplevel = gtk_widget_get_toplevel(a->widget);
	if (toplevel == NULL) {
		// TODO
		return;
	}
	// the docs say to do this
	if (!gtk_widget_is_toplevel(toplevel)) {
		// TODO
		return;
	}
	if (!GTK_IS_WINDOW(toplevel)) {
		// TODO
		return;
	}
	gtk_window_begin_move_drag(GTK_WINDOW(toplevel),
		a->dragevent->button,
		a->dragevent->x_root,		// TODO are these correct?
		a->dragevent->y_root,
		a->dragevent->time);
}

static const GdkWindowEdge edges[] = {
	[uiWindowResizeEdgeLeft] = GDK_WINDOW_EDGE_WEST,
	[uiWindowResizeEdgeTop] = GDK_WINDOW_EDGE_NORTH,
	[uiWindowResizeEdgeRight] = GDK_WINDOW_EDGE_EAST,
	[uiWindowResizeEdgeBottom] = GDK_WINDOW_EDGE_SOUTH,
	[uiWindowResizeEdgeTopLeft] = GDK_WINDOW_EDGE_NORTH_WEST,
	[uiWindowResizeEdgeTopRight] = GDK_WINDOW_EDGE_NORTH_EAST,
	[uiWindowResizeEdgeBottomLeft] = GDK_WINDOW_EDGE_SOUTH_WEST,
	[uiWindowResizeEdgeBottomRight] = GDK_WINDOW_EDGE_SOUTH_EAST,
};

void uiAreaBeginUserWindowResize(uiArea *a, uiWindowResizeEdge edge)
{
	GtkWidget *toplevel;

	if (a->dragevent == NULL)
		userbug("cannot call uiAreaBeginUserWindowResize() outside of a Mouse() with Down != 0");
	// TODO don't we have a libui function for this? did I scrap it?
	// TODO widget or areaWidget?
	toplevel = gtk_widget_get_toplevel(a->widget);
	if (toplevel == NULL) {
		// TODO
		return;
	}
	// the docs say to do this
	if (!gtk_widget_is_toplevel(toplevel)) {
		// TODO
		return;
	}
	if (!GTK_IS_WINDOW(toplevel)) {
		// TODO
		return;
	}
	gtk_window_begin_resize_drag(GTK_WINDOW(toplevel),
		edges[edge],
		a->dragevent->button,
		a->dragevent->x_root,		// TODO are these correct?
		a->dragevent->y_root,
		a->dragevent->time);
}

uiArea *uiNewArea(uiAreaHandler *ah)
{
	uiArea *a;

	uiUnixNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = FALSE;

	a->areaWidget = GTK_WIDGET(g_object_new(areaWidgetType,
		"libui-area", a,
		NULL));
	a->drawingArea = GTK_DRAWING_AREA(a->areaWidget);
	a->area = areaWidget(a->areaWidget);

	a->widget = a->areaWidget;

	return a;
}

uiArea *uiNewScrollingArea(uiAreaHandler *ah, int width, int height)
{
	uiArea *a;

	uiUnixNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = TRUE;
	a->scrollWidth = width;
	a->scrollHeight = height;

	a->swidget = gtk_scrolled_window_new(NULL, NULL);
	a->scontainer = GTK_CONTAINER(a->swidget);
	a->sw = GTK_SCROLLED_WINDOW(a->swidget);

	a->areaWidget = GTK_WIDGET(g_object_new(areaWidgetType,
		"libui-area", a,
		NULL));
	a->drawingArea = GTK_DRAWING_AREA(a->areaWidget);
	a->area = areaWidget(a->areaWidget);

	a->widget = a->swidget;

	gtk_container_add(a->scontainer, a->areaWidget);
	// and make the area visible; only the scrolled window's visibility is controlled by libui
	gtk_widget_show(a->areaWidget);

	return a;
}
