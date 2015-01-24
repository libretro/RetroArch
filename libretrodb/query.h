#ifndef __LIBRETRODB_QUERY_H__
#define __LIBRETRODB_QUERY_H__

#include "libretrodb.h"

void libretrodb_query_inc_ref(libretrodb_query_t *q);

void libretrodb_query_dec_ref(libretrodb_query_t *q);

int libretrodb_query_filter(libretrodb_query_t *q,
      struct rmsgpack_dom_value * v);

#endif
