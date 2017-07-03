// 22 may 2016
#include "../ui.h"
#include "uipriv.h"

void uiUserBugCannotSetParentOnToplevel(const char *type)
{
	userbug("You cannot make a %s a child of another uiControl,", type);
}
