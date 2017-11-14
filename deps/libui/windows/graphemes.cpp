// 25 may 2016
#include "uipriv_windows.hpp"

// We could use CharNext() to generate grapheme cluster boundaries, but it doesn't handle surrogate pairs properly (see http://archives.miloush.net/michkap/archive/2008/12/16/9223301.html).
// So let's use Uniscribe (see http://archives.miloush.net/michkap/archive/2005/01/14/352802.html)
// See also http://www.catch22.net/tuts/uniscribe-mysteries and http://www.catch22.net/tuts/keyboard-navigation for more details.

static HRESULT itemize(WCHAR *msg, size_t len, SCRIPT_ITEM **out, int *outn)
{
	SCRIPT_CONTROL sc;
	SCRIPT_STATE ss;
	SCRIPT_ITEM *items;
	size_t maxItems;
	int n;
	HRESULT hr;

	// make sure these are zero-initialized to avoid mangling the text
	ZeroMemory(&sc, sizeof (SCRIPT_CONTROL));
	ZeroMemory(&ss, sizeof (SCRIPT_STATE));

	maxItems = len + 2;
	for (;;) {
		items = new SCRIPT_ITEM[maxItems];
		hr = ScriptItemize(msg, len,
			maxItems,
			&sc, &ss,
			items, &n);
		if (hr == S_OK)
			break;
		// otherwise either an error or not enough room
		delete[] items;
		if (hr != E_OUTOFMEMORY)
			return hr;
		maxItems *= 2;		// add some more and try again
	}

	*out = items;
	*outn = n;
	return S_OK;
}

size_t *graphemes(WCHAR *msg)
{
	size_t len;
	SCRIPT_ITEM *items;
	int i, n;
	size_t *out;
	size_t *op;
	SCRIPT_LOGATTR *logattr;
	int j, nn;
	HRESULT hr;

	len = wcslen(msg);
	hr = itemize(msg, len, &items, &n);
	if (hr != S_OK)
		logHRESULT(L"error itemizing string for finding grapheme cluster boundaries", hr);

	// should be enough; 2 more just to be safe
	out = (size_t *) uiAlloc((len + 2) * sizeof (size_t), "size_t[]");
	op = out;

	// note that there are actually n + 1 elements in items
	for (i = 0; i < n; i++) {
		nn = items[i + 1].iCharPos - items[i].iCharPos;
		logattr = new SCRIPT_LOGATTR[nn];
		hr = ScriptBreak(msg + items[i].iCharPos, nn,
			&(items[i].a), logattr);
		if (hr != S_OK)
			logHRESULT(L"error breaking string for finding grapheme cluster boundaries", hr);
		for (j = 0; j < nn; j++)
			if (logattr[j].fCharStop != 0)
				*op++ = items[i].iCharPos + j;
		delete[] logattr;
	}
	// and handle the last item for the end of the string
	*op++ = items[i].iCharPos;

	delete[] items;
	return out;
}
