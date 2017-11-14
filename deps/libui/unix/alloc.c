// 7 april 2015
#include <string.h>
#include "uipriv_unix.h"

static GPtrArray *allocations;

#define UINT8(p) ((uint8_t *) (p))
#define PVOID(p) ((void *) (p))
#define EXTRA (sizeof (size_t) + sizeof (const char **))
#define DATA(p) PVOID(UINT8(p) + EXTRA)
#define BASE(p) PVOID(UINT8(p) - EXTRA)
#define SIZE(p) ((size_t *) (p))
#define CCHAR(p) ((const char **) (p))
#define TYPE(p) CCHAR(UINT8(p) + sizeof (size_t))

void initAlloc(void)
{
	allocations = g_ptr_array_new();
}

static void uninitComplain(gpointer ptr, gpointer data)
{
	char **str = (char **) data;
	char *str2;

	if (*str == NULL)
		*str = g_strdup_printf("");
	str2 = g_strdup_printf("%s%p %s\n", *str, ptr, *TYPE(ptr));
	g_free(*str);
	*str = str2;
}

void uninitAlloc(void)
{
	char *str = NULL;

	if (allocations->len == 0) {
		g_ptr_array_free(allocations, TRUE);
		return;
	}
	g_ptr_array_foreach(allocations, uninitComplain, &str);
	userbug("Some data was leaked; either you left a uiControl lying around or there's a bug in libui itself. Leaked data:\n%s", str);
	g_free(str);
}

void *uiAlloc(size_t size, const char *type)
{
	void *out;

	out = g_malloc0(EXTRA + size);
	*SIZE(out) = size;
	*TYPE(out) = type;
	g_ptr_array_add(allocations, out);
	return DATA(out);
}

void *uiRealloc(void *p, size_t new, const char *type)
{
	void *out;
	size_t *s;

	if (p == NULL)
		return uiAlloc(new, type);
	p = BASE(p);
	out = g_realloc(p, EXTRA + new);
	s = SIZE(out);
	if (new <= *s)
		memset(((uint8_t *) DATA(out)) + *s, 0, new - *s);
	*s = new;
	if (g_ptr_array_remove(allocations, p) == FALSE)
		implbug("%p not found in allocations array in uiRealloc()", p);
	g_ptr_array_add(allocations, out);
	return DATA(out);
}

void uiFree(void *p)
{
	if (p == NULL)
		implbug("attempt to uiFree(NULL)");
	p = BASE(p);
	g_free(p);
	if (g_ptr_array_remove(allocations, p) == FALSE)
		implbug("%p not found in allocations array in uiFree()", p);
}
