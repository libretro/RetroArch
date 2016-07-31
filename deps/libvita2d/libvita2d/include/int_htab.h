#ifndef INT_HTAB_H
#define INT_HTAB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INT_HTAB_MAX_LOAD (70) // over 100

typedef struct int_htab_entry {
	unsigned int key;
	void *value;
} int_htab_entry;

typedef struct int_htab {
	size_t size;
	size_t used;
	int_htab_entry *entries;
} int_htab;

int_htab *int_htab_create(size_t size);
void int_htab_free(int_htab *htab);
int int_htab_insert(int_htab *htab, unsigned int key, void *value);
void *int_htab_find(const int_htab *htab, unsigned int key);
int int_htab_erase(const int_htab *htab, unsigned int key);


#ifdef __cplusplus
}
#endif

#endif
