#ifndef __RARCHDB_BINTREE_H__
#define __RARCHDB_BINTREE_H__

typedef int (* bintree_cmp_func)(
        const void * a,
        const void * b,
        void * ctx
);

typedef int (* bintree_iter_cb)(
        void * value,
        void * ctx
);


struct bintree_node {
	void * value;
	struct bintree_node * parent;
	struct bintree_node * left;
	struct bintree_node * right;
};

struct bintree {
	struct bintree_node * root;
	bintree_cmp_func cmp;
	void * ctx;
};

void bintree_new(
        struct bintree * t,
        bintree_cmp_func cmp,
        void * ctx
);
int bintree_insert(
        struct bintree * t,
        void * value
);
int bintree_iterate(
        const struct bintree * t,
        bintree_iter_cb cb,
        void * ctx
);
void bintree_free(struct bintree * t);

#endif
