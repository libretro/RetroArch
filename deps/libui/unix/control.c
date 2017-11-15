// 16 august 2015
#include "uipriv_unix.h"

void uiUnixControlSetContainer(uiUnixControl *c, GtkContainer *container, gboolean remove)
{
	(*(c->SetContainer))(c, container, remove);
}

#define uiUnixControlSignature 0x556E6978

uiUnixControl *uiUnixAllocControl(size_t n, uint32_t typesig, const char *typenamestr)
{
	return uiUnixControl(uiAllocControl(n, uiUnixControlSignature, typesig, typenamestr));
}
