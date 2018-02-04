// 25 may 2016
#include "uipriv_unix.h"

ptrdiff_t *graphemes(const char *text, PangoContext *context)
{
	size_t len, lenchars;
	PangoLogAttr *logattrs;
	ptrdiff_t *out;
	ptrdiff_t *op;
	size_t i;

	len = strlen(text);
	lenchars = g_utf8_strlen(text, -1);
	logattrs = (PangoLogAttr *) uiAlloc((lenchars + 1) * sizeof (PangoLogAttr), "PangoLogAttr[]");
	pango_get_log_attrs(text, len,
		-1, NULL,
		logattrs, lenchars + 1);

	// should be more than enough
	out = (ptrdiff_t *) uiAlloc((lenchars + 2) * sizeof (ptrdiff_t), "ptrdiff_t[]");
	op = out;
	for (i = 0; i < lenchars; i++)
		if (logattrs[i].is_cursor_position != 0)
			// TODO optimize this
			*op++ = g_utf8_offset_to_pointer(text, i) - text;
	// and do the last one
	*op++ = len;

	uiFree(logattrs);
	return out;
}
