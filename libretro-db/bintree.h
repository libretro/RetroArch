#ifndef __RARCHDB_BINTREE_H__
#define __RARCHDB_BINTREE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bintree bintree_t;

typedef int (*bintree_cmp_func)(const void *a, const void *b, void *ctx);
typedef int (*bintree_iter_cb) (void *value, void *ctx);

bintree_t *bintree_new(bintree_cmp_func cmp, void *ctx);

int bintree_insert(bintree_t *t, void *value);

int bintree_iterate(const bintree_t *t, bintree_iter_cb cb, void *ctx);

void bintree_free(bintree_t *t);

#ifdef __cplusplus
}
#endif

#endif
