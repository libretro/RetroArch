// 24 april 2015
#include "uipriv_windows.hpp"

// LONGTERM migrate to std::vector

static uiMenu **menus = NULL;
static size_t len = 0;
static size_t cap = 0;
static BOOL menusFinalized = FALSE;
static WORD curID = 100;			// start somewhere safe
static BOOL hasQuit = FALSE;
static BOOL hasPreferences = FALSE;
static BOOL hasAbout = FALSE;

struct uiMenu {
	WCHAR *name;
	uiMenuItem **items;
	size_t len;
	size_t cap;
};

struct uiMenuItem {
	WCHAR *name;
	int type;
	WORD id;
	void (*onClicked)(uiMenuItem *, uiWindow *, void *);
	void *onClickedData;
	BOOL disabled;				// template for new instances; kept in sync with everything else
	BOOL checked;
	HMENU *hmenus;
	size_t len;
	size_t cap;
};

enum {
	typeRegular,
	typeCheckbox,
	typeQuit,
	typePreferences,
	typeAbout,
	typeSeparator,
};

#define grow 32

static void sync(uiMenuItem *item)
{
	size_t i;
	MENUITEMINFOW mi;

	ZeroMemory(&mi, sizeof (MENUITEMINFOW));
	mi.cbSize = sizeof (MENUITEMINFOW);
	mi.fMask = MIIM_STATE;
	if (item->disabled)
		mi.fState |= MFS_DISABLED;
	if (item->checked)
		mi.fState |= MFS_CHECKED;

	for (i = 0; i < item->len; i++)
		if (SetMenuItemInfo(item->hmenus[i], item->id, FALSE, &mi) == 0)
			logLastError(L"error synchronizing menu items");
}

static void defaultOnClicked(uiMenuItem *item, uiWindow *w, void *data)
{
	// do nothing
}

static void onQuitClicked(uiMenuItem *item, uiWindow *w, void *data)
{
	if (shouldQuit())
		uiQuit();
}

void uiMenuItemEnable(uiMenuItem *i)
{
	i->disabled = FALSE;
	sync(i);
}

void uiMenuItemDisable(uiMenuItem *i)
{
	i->disabled = TRUE;
	sync(i);
}

void uiMenuItemOnClicked(uiMenuItem *i, void (*f)(uiMenuItem *, uiWindow *, void *), void *data)
{
	if (i->type == typeQuit)
		userbug("You can not call uiMenuItemOnClicked() on a Quit item; use uiOnShouldQuit() instead.");
	i->onClicked = f;
	i->onClickedData = data;
}

int uiMenuItemChecked(uiMenuItem *i)
{
	return i->checked != FALSE;
}

void uiMenuItemSetChecked(uiMenuItem *i, int checked)
{
	// use explicit values
	i->checked = FALSE;
	if (checked)
		i->checked = TRUE;
	sync(i);
}

static uiMenuItem *newItem(uiMenu *m, int type, const char *name)
{
	uiMenuItem *item;

	if (menusFinalized)
		userbug("You can not create a new menu item after menus have been finalized.");

	if (m->len >= m->cap) {
		m->cap += grow;
		m->items = (uiMenuItem **) uiRealloc(m->items, m->cap * sizeof (uiMenuItem *), "uiMenuitem *[]");
	}

	item = uiNew(uiMenuItem);

	m->items[m->len] = item;
	m->len++;

	item->type = type;
	switch (item->type) {
	case typeQuit:
		item->name = toUTF16("Quit");
		break;
	case typePreferences:
		item->name = toUTF16("Preferences...");
		break;
	case typeAbout:
		item->name = toUTF16("About");
		break;
	case typeSeparator:
		break;
	default:
		item->name = toUTF16(name);
		break;
	}

	if (item->type != typeSeparator) {
		item->id = curID;
		curID++;
	}

	if (item->type == typeQuit) {
		// can't call uiMenuItemOnClicked() here
		item->onClicked = onQuitClicked;
		item->onClickedData = NULL;
	} else
		uiMenuItemOnClicked(item, defaultOnClicked, NULL);

	return item;
}

uiMenuItem *uiMenuAppendItem(uiMenu *m, const char *name)
{
	return newItem(m, typeRegular, name);
}

uiMenuItem *uiMenuAppendCheckItem(uiMenu *m, const char *name)
{
	return newItem(m, typeCheckbox, name);
}

uiMenuItem *uiMenuAppendQuitItem(uiMenu *m)
{
	if (hasQuit)
		userbug("You can not have multiple Quit menu items in a program.");
	hasQuit = TRUE;
	newItem(m, typeSeparator, NULL);
	return newItem(m, typeQuit, NULL);
}

uiMenuItem *uiMenuAppendPreferencesItem(uiMenu *m)
{
	if (hasPreferences)
		userbug("You can not have multiple Preferences menu items in a program.");
	hasPreferences = TRUE;
	newItem(m, typeSeparator, NULL);
	return newItem(m, typePreferences, NULL);
}

uiMenuItem *uiMenuAppendAboutItem(uiMenu *m)
{
	if (hasAbout)
		// TODO place these userbug strings in a header
		userbug("You can not have multiple About menu items in a program.");
	hasAbout = TRUE;
	newItem(m, typeSeparator, NULL);
	return newItem(m, typeAbout, NULL);
}

void uiMenuAppendSeparator(uiMenu *m)
{
	newItem(m, typeSeparator, NULL);
}

uiMenu *uiNewMenu(const char *name)
{
	uiMenu *m;

	if (menusFinalized)
		userbug("You can not create a new menu after menus have been finalized.");
	if (len >= cap) {
		cap += grow;
		menus = (uiMenu **) uiRealloc(menus, cap * sizeof (uiMenu *), "uiMenu *[]");
	}

	m = uiNew(uiMenu);

	menus[len] = m;
	len++;

	m->name = toUTF16(name);

	return m;
}

static void appendMenuItem(HMENU menu, uiMenuItem *item)
{
	UINT uFlags;

	uFlags = MF_SEPARATOR;
	if (item->type != typeSeparator) {
		uFlags = MF_STRING;
		if (item->disabled)
			uFlags |= MF_DISABLED | MF_GRAYED;
		if (item->checked)
			uFlags |= MF_CHECKED;
	}
	if (AppendMenuW(menu, uFlags, item->id, item->name) == 0)
		logLastError(L"error appending menu item");

	if (item->len >= item->cap) {
		item->cap += grow;
		item->hmenus = (HMENU *) uiRealloc(item->hmenus, item->cap * sizeof (HMENU), "HMENU[]");
	}
	item->hmenus[item->len] = menu;
	item->len++;
}

static HMENU makeMenu(uiMenu *m)
{
	HMENU menu;
	size_t i;

	menu = CreatePopupMenu();
	if (menu == NULL)
		logLastError(L"error creating menu");
	for (i = 0; i < m->len; i++)
		appendMenuItem(menu, m->items[i]);
	return menu;
}

HMENU makeMenubar(void)
{
	HMENU menubar;
	HMENU menu;
	size_t i;

	menusFinalized = TRUE;

	menubar = CreateMenu();
	if (menubar == NULL)
		logLastError(L"error creating menubar");

	for (i = 0; i < len; i++) {
		menu = makeMenu(menus[i]);
		if (AppendMenuW(menubar, MF_POPUP | MF_STRING, (UINT_PTR) menu, menus[i]->name) == 0)
			logLastError(L"error appending menu to menubar");
	}

	return menubar;
}

void runMenuEvent(WORD id, uiWindow *w)
{
	uiMenu *m;
	uiMenuItem *item;
	size_t i, j;

	// this isn't optimal, but it works, and it should be just fine for most cases
	for (i = 0; i < len; i++) {
		m = menus[i];
		for (j = 0; j < m->len; j++) {
			item = m->items[j];
			if (item->id == id)
				goto found;
		}
	}
	// no match
	implbug("unknown menu ID %hu in runMenuEvent()", id);

found:
	// first toggle checkboxes, if any
	if (item->type == typeCheckbox)
		uiMenuItemSetChecked(item, !uiMenuItemChecked(item));

	// then run the event
	(*(item->onClicked))(item, w, item->onClickedData);
}

static void freeMenu(uiMenu *m, HMENU submenu)
{
	size_t i;
	uiMenuItem *item;
	size_t j;

	for (i = 0; i < m->len; i++) {
		item = m->items[i];
		for (j = 0; j < item->len; j++)
			if (item->hmenus[j] == submenu)
				break;
		if (j >= item->len)
			implbug("submenu handle %p not found in freeMenu()", submenu);
		for (; j < item->len - 1; j++)
			item->hmenus[j] = item->hmenus[j + 1];
		item->hmenus[j] = NULL;
		item->len--;
	}
}

void freeMenubar(HMENU menubar)
{
	size_t i;
	MENUITEMINFOW mi;

	for (i = 0; i < len; i++) {
		ZeroMemory(&mi, sizeof (MENUITEMINFOW));
		mi.cbSize = sizeof (MENUITEMINFOW);
		mi.fMask = MIIM_SUBMENU;
		if (GetMenuItemInfoW(menubar, i, TRUE, &mi) == 0)
			logLastError(L"error getting menu to delete item references from");
		freeMenu(menus[i], mi.hSubMenu);
	}
	// no need to worry about destroying any menus; destruction of the window they're in will do it for us
}

void uninitMenus(void)
{
	uiMenu *m;
	uiMenuItem *item;
	size_t i, j;

	for (i = 0; i < len; i++) {
		m = menus[i];
		uiFree(m->name);
		for (j = 0; j < m->len; j++) {
			item = m->items[j];
			if (item->len != 0)
				// LONGTERM userbug()?
				implbug("menu item %p (%ws) still has uiWindows attached; did you forget to destroy some windows?", item, item->name);
			if (item->name != NULL)
				uiFree(item->name);
			if (item->hmenus != NULL)
				uiFree(item->hmenus);
			uiFree(item);
		}
		if (m->items != NULL)
			uiFree(m->items);
		uiFree(m);
	}
	if (menus != NULL)
		uiFree(menus);
}
