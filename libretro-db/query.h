#ifndef __LIBRETRODB_QUERY_H__
#define __LIBRETRODB_QUERY_H__

#include "libretrodb.h"
#include "rmsgpack_dom.h"

typedef struct libretrodb_query libretrodb_query_t;

void libretrodb_query_inc_ref(libretrodb_query_t *q);

void libretrodb_query_dec_ref(libretrodb_query_t *q);

int libretrodb_query_filter(libretrodb_query_t *q, struct rmsgpack_dom_value *v);

#endif
