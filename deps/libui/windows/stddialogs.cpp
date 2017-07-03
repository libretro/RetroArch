// 22 may 2015
#include "uipriv_windows.hpp"

// TODO document all this is what we want
// TODO do the same for font and color buttons

// notes:
// - FOS_SUPPORTSTREAMABLEITEMS doesn't seem to be supported on windows vista, or at least not with the flags we use
// - even with FOS_NOVALIDATE the dialogs will reject invalid filenames (at least on Vista, anyway)
// - lack of FOS_NOREADONLYRETURN doesn't seem to matter on Windows 7

// TODO
// - http://blogs.msdn.com/b/wpfsdk/archive/2006/10/26/uncommon-dialogs--font-chooser-and-color-picker-dialogs.aspx
// - when a dialog is active, tab navigation in other windows stops working
// - when adding uiOpenFolder(), use IFileDialog as well - https://msdn.microsoft.com/en-us/library/windows/desktop/bb762115%28v=vs.85%29.aspx

#define windowHWND(w) ((HWND) uiControlHandle(uiControl(w)))

char *commonItemDialog(HWND parent, REFCLSID clsid, REFIID iid, FILEOPENDIALOGOPTIONS optsadd)
{
	IFileDialog *d = NULL;
	FILEOPENDIALOGOPTIONS opts;
	IShellItem *result = NULL;
	WCHAR *wname = NULL;
	char *name = NULL;
	HRESULT hr;

	hr = CoCreateInstance(clsid,
		NULL, CLSCTX_INPROC_SERVER,
		iid, (LPVOID *) (&d));
	if (hr != S_OK) {
		logHRESULT(L"error creating common item dialog", hr);
		// always return NULL on error
		goto out;
	}
	hr = d->GetOptions(&opts);
	if (hr != S_OK) {
		logHRESULT(L"error getting current options", hr);
		goto out;
	}
	opts |= optsadd;
	// the other platforms don't check read-only; we won't either
	opts &= ~FOS_NOREADONLYRETURN;
	hr = d->SetOptions(opts);
	if (hr != S_OK) {
		logHRESULT(L"error setting options", hr);
		goto out;
	}
	hr = d->Show(parent);
	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
		// cancelled; return NULL like we have ready
		goto out;
	if (hr != S_OK) {
		logHRESULT(L"error showing dialog", hr);
		goto out;
	}
	hr = d->GetResult(&result);
	if (hr != S_OK) {
		logHRESULT(L"error getting dialog result", hr);
		goto out;
	}
	hr = result->GetDisplayName(SIGDN_FILESYSPATH, &wname);
	if (hr != S_OK) {
		logHRESULT(L"error getting filename", hr);
		goto out;
	}
	name = toUTF8(wname);

out:
	if (wname != NULL)
		CoTaskMemFree(wname);
	if (result != NULL)
		result->Release();
	if (d != NULL)
		d->Release();
	return name;
}

char *uiOpenFile(uiWindow *parent)
{
	char *res;

	disableAllWindowsExcept(parent);
	res = commonItemDialog(windowHWND(parent),
		CLSID_FileOpenDialog, IID_IFileOpenDialog,
		FOS_NOCHANGEDIR | FOS_ALLNONSTORAGEITEMS | FOS_NOVALIDATE | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_SHAREAWARE | FOS_NOTESTFILECREATE | FOS_NODEREFERENCELINKS | FOS_FORCESHOWHIDDEN | FOS_DEFAULTNOMINIMODE);
	enableAllWindowsExcept(parent);
	return res;
}

char *uiSaveFile(uiWindow *parent)
{
	char *res;

	disableAllWindowsExcept(parent);
	res = commonItemDialog(windowHWND(parent),
		CLSID_FileSaveDialog, IID_IFileSaveDialog,
		FOS_OVERWRITEPROMPT | FOS_NOCHANGEDIR | FOS_ALLNONSTORAGEITEMS | FOS_NOVALIDATE | FOS_SHAREAWARE | FOS_NOTESTFILECREATE | FOS_NODEREFERENCELINKS | FOS_FORCESHOWHIDDEN | FOS_DEFAULTNOMINIMODE);
	enableAllWindowsExcept(parent);
	return res;
}

// TODO switch to TaskDialogIndirect()?

static void msgbox(HWND parent, const char *title, const char *description, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR icon)
{
	WCHAR *wtitle, *wdescription;
	HRESULT hr;

	wtitle = toUTF16(title);
	wdescription = toUTF16(description);

	hr = TaskDialog(parent, NULL, NULL, wtitle, wdescription, buttons, icon, NULL);
	if (hr != S_OK)
		logHRESULT(L"error showing task dialog", hr);

	uiFree(wdescription);
	uiFree(wtitle);
}

void uiMsgBox(uiWindow *parent, const char *title, const char *description)
{
	disableAllWindowsExcept(parent);
	msgbox(windowHWND(parent), title, description, TDCBF_OK_BUTTON, NULL);
	enableAllWindowsExcept(parent);
}

void uiMsgBoxError(uiWindow *parent, const char *title, const char *description)
{
	disableAllWindowsExcept(parent);
	msgbox(windowHWND(parent), title, description, TDCBF_OK_BUTTON, TD_ERROR_ICON);
	enableAllWindowsExcept(parent);
}
