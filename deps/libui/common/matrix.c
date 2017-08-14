// 11 october 2015
#include <math.h>
#include "../ui.h"
#include "uipriv.h"

void uiDrawMatrixSetIdentity(uiDrawMatrix *m)
{
	m->M11 = 1;
	m->M12 = 0;
	m->M21 = 0;
	m->M22 = 1;
	m->M31 = 0;
	m->M32 = 0;
}

// The rest of this file provides basic utilities in case the platform doesn't provide any of its own for these tasks.
// Keep these as minimal as possible. They should generally not call other fallbacks.

// see https://msdn.microsoft.com/en-us/library/windows/desktop/ff684171%28v=vs.85%29.aspx#skew_transform
// TODO see if there's a way we can avoid the multiplication
void fallbackSkew(uiDrawMatrix *m, double x, double y, double xamount, double yamount)
{
	uiDrawMatrix n;

	uiDrawMatrixSetIdentity(&n);
	// TODO explain this
	n.M12 = tan(yamount);
	n.M21 = tan(xamount);
	n.M31 = -y * tan(xamount);
	n.M32 = -x * tan(yamount);
	uiDrawMatrixMultiply(m, &n);
}

void scaleCenter(double xCenter, double yCenter, double *x, double *y)
{
	*x = xCenter - (*x * xCenter);
	*y = yCenter - (*y * yCenter);
}

// the basic algorithm is from cairo
// but it's the same algorithm as the transform point, just without M31 and M32 taken into account, so let's just do that instead
void fallbackTransformSize(uiDrawMatrix *m, double *x, double *y)
{
	uiDrawMatrix m2;

	m2 = *m;
	m2.M31 = 0;
	m2.M32 = 0;
	uiDrawMatrixTransformPoint(&m2, x, y);
}
