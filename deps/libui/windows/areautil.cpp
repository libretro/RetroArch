// 18 december 2015
#include "uipriv_windows.hpp"
#include "area.hpp"

void loadAreaSize(uiArea *a, ID2D1RenderTarget *rt, double *width, double *height)
{
	D2D1_SIZE_F size;

	*width = 0;
	*height = 0;
	if (!a->scrolling) {
		if (rt == NULL)
			rt = a->rt;
		size = realGetSize(rt);
		*width = size.width;
		*height = size.height;
	}
}

void pixelsToDIP(uiArea *a, double *x, double *y)
{
	FLOAT dpix, dpiy;

	a->rt->GetDpi(&dpix, &dpiy);
	// see https://msdn.microsoft.com/en-us/library/windows/desktop/dd756649%28v=vs.85%29.aspx (and others; search "direct2d mouse")
	*x = (*x * 96) / dpix;
	*y = (*y * 96) / dpiy;
}

void dipToPixels(uiArea *a, double *x, double *y)
{
	FLOAT dpix, dpiy;

	a->rt->GetDpi(&dpix, &dpiy);
	*x = (*x * dpix) / 96;
	*y = (*y * dpiy) / 96;
}
