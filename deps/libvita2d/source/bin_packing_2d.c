#include <stdlib.h>
#include "bin_packing_2d.h"

bp2d_node *bp2d_create(const bp2d_rectangle *rect)
{
	bp2d_node *node = malloc(sizeof(*node));
	if (!node)
		return NULL;

	node->left = NULL;
	node->right = NULL;
	node->rect.x = rect->x;
	node->rect.y = rect->y;
	node->rect.w = rect->w;
	node->rect.h = rect->h;
	node->filled = 0;

	return node;
}

void bp2d_free(bp2d_node *node)
{
	if (node) {
		if (node->left) {
			bp2d_free(node->left);
		}
		if (node->right) {
			bp2d_free(node->right);
		}
		free(node);
	}
}

int bp2d_insert(bp2d_node *node, const bp2d_size *in_size, bp2d_position *out_pos, bp2d_node **out_node)
{
	if (node->left != NULL || node->right != NULL) {
		int ret = bp2d_insert(node->left, in_size, out_pos, out_node);
		if (ret == 0) {
			return bp2d_insert(node->right, in_size, out_pos, out_node);
		}
		return ret;
	} else {
		if (node->filled)
			return 0;

		if (in_size->w > node->rect.w || in_size->h > node->rect.h)
			return 0;

		if (in_size->w == node->rect.w && in_size->h == node->rect.h) {
			out_pos->x = node->rect.x;
			out_pos->y = node->rect.y;
			node->filled = 1;
			if (out_node)
				*out_node = node;
			return 1;
		}

		int dw = node->rect.w - in_size->w;
		int dh = node->rect.h - in_size->h;

		bp2d_rectangle left_rect, right_rect;

		if (dw > dh) {
			left_rect.x = node->rect.x;
			left_rect.y = node->rect.y;
			left_rect.w = in_size->w;
			left_rect.h = node->rect.h;

			right_rect.x = node->rect.x + in_size->w;
			right_rect.y = node->rect.y;
			right_rect.w = node->rect.w - in_size->w;
			right_rect.h = node->rect.h;
		} else {
			left_rect.x = node->rect.x;
			left_rect.y = node->rect.y;
			left_rect.w = node->rect.w;
			left_rect.h = in_size->h;

			right_rect.x = node->rect.x;
			right_rect.y = node->rect.y + in_size->h;
			right_rect.w = node->rect.w;
			right_rect.h = node->rect.h - in_size->h;
		}

		node->left = bp2d_create(&left_rect);
		node->right = bp2d_create(&right_rect);

		return bp2d_insert(node->left, in_size, out_pos, out_node);
	}
}

int bp2d_delete(bp2d_node *root, bp2d_node *node)
{
	if (root == NULL || node == NULL)
		return 0;
	else if (root == node) {
		bp2d_free(root->left);
		bp2d_free(root->right);
		root->left = NULL;
		root->right = NULL;
		root->filled = 0;
		return 1;
	}

	return bp2d_delete(root->left, node) || bp2d_delete(root->right, node);
}
