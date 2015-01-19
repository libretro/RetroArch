#ifndef __RARCHDB_QUERY_H__
#define __RARCHDB_QUERY_H__

#include "rarchdb.h"

void rarchdb_query_inc_ref(rarchdb_query * q);
void rarchdb_query_dec_ref(rarchdb_query * q);
int rarchdb_query_filter(
        rarchdb_query * q,
        struct rmsgpack_dom_value * v
);

#endif
