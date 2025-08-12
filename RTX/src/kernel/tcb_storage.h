#ifndef TCB_STORAGE
#define TCB_STORAGE

#include "k_inc.h"
#include "common_ext.h"

typedef struct rb_tree_node {
	U8 priority;
	struct rb_tree_node *left;
	struct rb_tree_node *right;
	struct rb_tree_node *parent;
	//0 = red, 1 = black
	U8 colour;
} rb_tree_node;

typedef struct rb_tree {
	struct rb_tree_node *root;
	struct rb_tree_node *nil;
} rb_tree;

extern TCB g_tcbs[MAX_TASKS];
extern rb_tree_node 	priorities[PRIO_NULL + 2];
extern rb_tree_node	*root;
extern rb_tree tree;
extern TCB *prio_list[PRIO_NULL + 1];

void rb_initialize_tree(rb_tree_node tree_arr[], rb_tree *tree);

void rb_left_rotate(rb_tree *tree, rb_tree_node* x);

void rb_right_rotate(rb_tree *tree, rb_tree_node* y);

void rb_insert(rb_tree *tree, rb_tree_node* z);

void rb_insert_fixup(rb_tree *tree, rb_tree_node* z);

void rb_transplant(rb_tree *tree, rb_tree_node *u, rb_tree_node *v);

void rb_remove(rb_tree *tree, rb_tree_node* z);

void rb_remove_fixup(rb_tree *tree, rb_tree_node* x);

void rb_traverse(rb_tree_node *root);

rb_tree_node *rb_min(rb_tree_node *root, rb_tree *tree);

void pl_initialize_lists(TCB **list_arr);

void pl_insert(TCB **list_arr, TCB *to_insert, rb_tree *tree, rb_tree_node* z);

TCB *pl_pop_min(TCB **list_arr, U8 prio, rb_tree *tree, rb_tree_node* z);

void pl_remove(TCB **list_arr, TCB *to_remove, rb_tree *tree, rb_tree_node* z);

void print_list(TCB **list_arr, U8 prio);

#endif
