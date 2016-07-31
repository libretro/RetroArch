#ifndef BIN_PACKING_2D_H
#define BIN_PACKING_2D_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bp2d_position {
	int x, y;
} bp2d_position;

typedef struct bp2d_size {
	int w, h;
} bp2d_size;

typedef struct bp2d_rectangle {
	int x, y, w, h;
} bp2d_rectangle;

typedef struct bp2d_node {
	struct bp2d_node *left;
	struct bp2d_node *right;
	bp2d_rectangle rect;
	int filled;
} bp2d_node;

bp2d_node *bp2d_create(const bp2d_rectangle *rect);
void bp2d_free(bp2d_node *node);
// 1 success, 0 failure
int bp2d_insert(bp2d_node *node, const bp2d_size *in_size, bp2d_position *out_pos, bp2d_node **out_node);
int bp2d_delete(bp2d_node *root, bp2d_node *node);

#ifdef __cplusplus
}
#endif

#endif
