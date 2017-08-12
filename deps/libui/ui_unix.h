// 7 april 2015

/*
This file assumes that you have included <gtk/gtk.h> and "ui.h" beforehand. It provides API-specific functions for interfacing with foreign controls on Unix systems that use GTK+ to provide their UI (currently all except Mac OS X).
*/

#ifndef __LIBUI_UI_UNIX_H__
#define __LIBUI_UI_UNIX_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uiUnixControl uiUnixControl;
struct uiUnixControl {
	uiControl c;
	uiControl *parent;
	gboolean addedBefore;
	void (*SetContainer)(uiUnixControl *, GtkContainer *, gboolean);
};
#define uiUnixControl(this) ((uiUnixControl *) (this))
// TODO document
_UI_EXTERN void uiUnixControlSetContainer(uiUnixControl *, GtkContainer *, gboolean);

#define uiUnixControlDefaultDestroy(type) \
	static void type ## Destroy(uiControl *c) \
	{ \
		/* TODO is this safe on floating refs? */ \
		g_object_unref(type(c)->widget); \
		uiFreeControl(c); \
	}
#define uiUnixControlDefaultHandle(type) \
	static uintptr_t type ## Handle(uiControl *c) \
	{ \
		return (uintptr_t) (type(c)->widget); \
	}
#define uiUnixControlDefaultParent(type) \
	static uiControl *type ## Parent(uiControl *c) \
	{ \
		return uiUnixControl(c)->parent; \
	}
#define uiUnixControlDefaultSetParent(type) \
	static void type ## SetParent(uiControl *c, uiControl *parent) \
	{ \
		uiControlVerifySetParent(c, parent); \
		uiUnixControl(c)->parent = parent; \
	}
#define uiUnixControlDefaultToplevel(type) \
	static int type ## Toplevel(uiControl *c) \
	{ \
		return 0; \
	}
#define uiUnixControlDefaultVisible(type) \
	static int type ## Visible(uiControl *c) \
	{ \
		return gtk_widget_get_visible(type(c)->widget); \
	}
#define uiUnixControlDefaultShow(type) \
	static void type ## Show(uiControl *c) \
	{ \
		gtk_widget_show(type(c)->widget); \
	}
#define uiUnixControlDefaultHide(type) \
	static void type ## Hide(uiControl *c) \
	{ \
		gtk_widget_hide(type(c)->widget); \
	}
#define uiUnixControlDefaultEnabled(type) \
	static int type ## Enabled(uiControl *c) \
	{ \
		return gtk_widget_get_sensitive(type(c)->widget); \
	}
#define uiUnixControlDefaultEnable(type) \
	static void type ## Enable(uiControl *c) \
	{ \
		gtk_widget_set_sensitive(type(c)->widget, TRUE); \
	}
#define uiUnixControlDefaultDisable(type) \
	static void type ## Disable(uiControl *c) \
	{ \
		gtk_widget_set_sensitive(type(c)->widget, FALSE); \
	}
// TODO this whole addedBefore stuff is a MASSIVE HACK.
#define uiUnixControlDefaultSetContainer(type) \
	static void type ## SetContainer(uiUnixControl *c, GtkContainer *container, gboolean remove) \
	{ \
		if (!uiUnixControl(c)->addedBefore) { \
			g_object_ref_sink(type(c)->widget); /* our own reference, which we release in Destroy() */ \
			gtk_widget_show(type(c)->widget); \
			uiUnixControl(c)->addedBefore = TRUE; \
		} \
		if (remove) \
			gtk_container_remove(container, type(c)->widget); \
		else \
			gtk_container_add(container, type(c)->widget); \
	}

#define uiUnixControlAllDefaultsExceptDestroy(type) \
	uiUnixControlDefaultHandle(type) \
	uiUnixControlDefaultParent(type) \
	uiUnixControlDefaultSetParent(type) \
	uiUnixControlDefaultToplevel(type) \
	uiUnixControlDefaultVisible(type) \
	uiUnixControlDefaultShow(type) \
	uiUnixControlDefaultHide(type) \
	uiUnixControlDefaultEnabled(type) \
	uiUnixControlDefaultEnable(type) \
	uiUnixControlDefaultDisable(type) \
	uiUnixControlDefaultSetContainer(type)

#define uiUnixControlAllDefaults(type) \
	uiUnixControlDefaultDestroy(type) \
	uiUnixControlAllDefaultsExceptDestroy(type)

// TODO document
#define uiUnixNewControl(type, var) \
	var = type(uiUnixAllocControl(sizeof (type), type ## Signature, #type)); \
	uiControl(var)->Destroy = type ## Destroy; \
	uiControl(var)->Handle = type ## Handle; \
	uiControl(var)->Parent = type ## Parent; \
	uiControl(var)->SetParent = type ## SetParent; \
	uiControl(var)->Toplevel = type ## Toplevel; \
	uiControl(var)->Visible = type ## Visible; \
	uiControl(var)->Show = type ## Show; \
	uiControl(var)->Hide = type ## Hide; \
	uiControl(var)->Enabled = type ## Enabled; \
	uiControl(var)->Enable = type ## Enable; \
	uiControl(var)->Disable = type ## Disable; \
	uiUnixControl(var)->SetContainer = type ## SetContainer;
// TODO document
_UI_EXTERN uiUnixControl *uiUnixAllocControl(size_t n, uint32_t typesig, const char *typenamestr);

// uiUnixStrdupText() takes the given string and produces a copy of it suitable for being freed by uiFreeText().
_UI_EXTERN char *uiUnixStrdupText(const char *);

#ifdef __cplusplus
}
#endif

#endif
