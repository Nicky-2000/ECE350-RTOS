#include "tcb_storage.h"
#include "k_mem.h"
#include "printf.h"

#define DEBUG_0
extern TCB g_tcbs[MAX_TASKS];
extern rb_tree_node 	priorities[PRIO_NULL + 2];
extern rb_tree_node	*root;
extern TCB *prio_list[PRIO_NULL + 1];

// RB Tree design from "Introduction to Algorithms, 3rd ed.", CLRS

void rb_initialize_tree(rb_tree_node tree_arr[], rb_tree *tree){
	rb_tree_node* nil_rb = &tree_arr[PRIO_NULL + 1];
	nil_rb->priority = 0;
	nil_rb->colour = 1;
	nil_rb->left = NULL;
	nil_rb->right = NULL;
	nil_rb->parent = NULL;

	// array not passed in properly (doesn't get access to any element other than 0)
	for(U32 i = 0; i < (PRIO_NULL + 1); ++i){
		tree_arr[i].priority = (U8)i;
	}
	tree_arr[PRIO_NULL].colour = 1;
	tree_arr[PRIO_NULL].left = nil_rb;
	tree_arr[PRIO_NULL].right = nil_rb;
	tree_arr[PRIO_NULL].parent = nil_rb;

	tree->nil = nil_rb;
	tree->root = tree->nil;
}


// TODO - Make inline
void rb_left_rotate(rb_tree *tree, rb_tree_node* x){
	rb_tree_node *y = x->right;
	x->right = y->left;
	if (y->left != tree->nil){
		y->left->parent = x;
	}
	y->parent = x->parent;
	if(x->parent == tree->nil){
		tree->root = y;
	} else if (x == x->parent->left){
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}
	y->left = x;
	x->parent = y;
}

// TODO - Make inline
void rb_right_rotate(rb_tree *tree, rb_tree_node* y){
	rb_tree_node *x = y->left;
	y->left = x->right;
	if(x->right != tree->nil){
		x->right->parent = y;
	}
	x->parent = y->parent;
	if(y->parent == tree->nil){
		tree->root = x;
	} else if(y == y->parent->right){
		y->parent->right = x;
	} else {
		y->parent->left = x;
	}
	x->right = y;
	y->parent = x;
}

//allow insert of head
void rb_insert(rb_tree *tree, rb_tree_node* z){
	rb_tree_node *y = tree->nil;
	rb_tree_node *x = tree->root;

	while(x != tree->nil) {
		y = x;
		if(z->priority < x->priority){
			x = x->left;
		} else {
			x = x->right;
		}
	}
	z->parent = y;
	if(y == tree->nil){
		tree->root = z;
	} else if(z->priority < y->priority){
		y->left = z;
	} else {
		y->right = z;
	}
	z->left = tree->nil;
	z->right = tree->nil;
	//0 = red, 1 = black
	z->colour = 0;
	rb_insert_fixup(tree, z);
}

// TODO - Make inline
void rb_insert_fixup(rb_tree *tree, rb_tree_node* z){
	//0 = red, 1 = black
	while(z->parent->colour == 0){
		if(z->parent == z->parent->parent->left){
			rb_tree_node *y = z->parent->parent->right;
			if(y->colour == 0){
				z->parent->colour = 1;
				y->colour = 1;
				z->parent->parent->colour = 0;
				z = z->parent->parent;
			} else if(z == z->parent->right){
				z = z->parent;
				rb_left_rotate(tree, z);
			} else {
				z->parent->colour = 1;
				z->parent->parent->colour = 0;
				rb_right_rotate(tree, z->parent->parent);
			}
		} else {
			rb_tree_node *y = z->parent->parent->left;
			if(y->colour == 0){
				z->parent->colour = 1;
				y->colour = 1;
				z->parent->parent->colour = 0;
				z = z->parent->parent;
			} else if(z == z->parent->left){
				z = z->parent;
				rb_right_rotate(tree, z);
			} else {
				z->parent->colour = 1;
				z->parent->parent->colour = 0;
				rb_left_rotate(tree, z->parent->parent);
			}
		}
	}
	tree->root->colour = 1;
}

// TODO - Make inline
void rb_transplant(rb_tree *tree, rb_tree_node *u, rb_tree_node *v){
	if(u->parent == tree->nil){
		tree->root = v;
	} else if(u == u->parent->left){
		u->parent->left = v;
	} else{
		u->parent->right = v;
	}
	v->parent = u->parent;
}

//implement removeal of head
void rb_remove(rb_tree *tree, rb_tree_node* z){
	rb_tree_node *y = z;
	rb_tree_node *x = tree->nil;
	U8 y_original_colour = y->colour;
	if(z->left == tree->nil){
		x = z->right;
		rb_transplant(tree, z, z->right);
	} else if(z->right == tree->nil){
		x = z->left;
		rb_transplant(tree, z, z->left);
	} else {
		y = rb_min(z->right, tree);
		y_original_colour = y->colour;
		x = y->right;
		if(y->parent == z){
			x->parent = y;
		} else {
			rb_transplant(tree, y, y->right);
			y->right = z->right;
			y->right->parent = y;
		}
		rb_transplant(tree, z, y);
		y->left = z->left;
		y->left->parent = y;
		y->colour = z->colour;
	}
	if(y_original_colour == 1){
		rb_remove_fixup(tree, x);
	}
}

// TODO - Make inline
void rb_remove_fixup(rb_tree *tree, rb_tree_node* x){
	rb_tree_node *w = tree->nil;

	while(x != tree->root && x->colour == 1){
		if(x == x->parent->left){
			w = x->parent->right;
			if(w->colour == 0){
				w->colour = 1;
				x->parent->colour = 0;
				rb_left_rotate(tree, x->parent);
				w = x->parent->right;
			}
			if(w->left->colour == 1 && w->right->colour == 1){
				w->colour = 0;
				x = x->parent;
			} else if(w->right->colour == 1){
				w->left->colour = 1;
				w->colour = 0;
				rb_right_rotate(tree, w);
				w = x->parent->right;
			} else {
				w->colour = x->parent->colour;
				x->parent->colour = 1;
				w->right->colour = 1;
				rb_left_rotate(tree, x->parent);
				x = tree->root;
			}
		} else {
			w = x->parent->left;
			if(w->colour == 0){
				w->colour = 1;
				x->parent->colour = 0;
				rb_right_rotate(tree, x->parent);
				w = x->parent->left;
			}
			if(w->right->colour == 1 && w->left->colour == 1){
				w->colour = 0;
				x = x->parent;
			} else if(w->left->colour == 1){
				w->right->colour = 1;
				w->colour = 0;
				rb_left_rotate(tree, w);
				w = x->parent->left;
			} else {
				w->colour = x->parent->colour;
				x->parent->colour = 1;
				w->left->colour = 1;
				rb_right_rotate(tree, x->parent);
				x = tree->root;
			}
		}
	}
	x->colour = 1;
}

void rb_traverse(rb_tree_node *root){
	if(root != NULL){
		rb_traverse(root->left);
#ifdef DEBUG_0
		printf("%d, ", root->priority);
#endif
		rb_traverse(root->right);
	}
}

// TODO - Make inline
rb_tree_node *rb_min(rb_tree_node *root, rb_tree *tree){
	rb_tree_node *traverse = root;
	if (root == tree->nil){
		return tree->nil;
	}
	while(traverse->left != tree->nil){
		traverse = traverse->left;
	}
	return traverse;
}

void pl_initialize_lists(TCB **list_arr){
	for(U32 i = 0; i < (PRIO_NULL + 1); ++i){
		list_arr[i] = NULL;
	}
}

void pl_insert(TCB **list_arr, TCB *to_insert, rb_tree *tree, rb_tree_node* z){
	U8 prio = to_insert->prio;
	TCB *traverse = list_arr[prio];

	if(traverse == NULL){
		list_arr[prio] = to_insert;
		rb_insert(tree, z);
	} else {
		while(traverse->next != NULL){
			traverse = traverse->next;
		}
		traverse->next = to_insert;
	}
}

TCB *pl_pop_min(TCB **list_arr, U8 prio, rb_tree *tree, rb_tree_node* z){
	TCB* ret_val = list_arr[prio];
	list_arr[prio] = ret_val->next;
	ret_val->next = NULL;
	if(list_arr[prio] == NULL){
		rb_remove(tree, z);
	}
	return ret_val;
}

void pl_remove(TCB **list_arr, TCB *to_remove, rb_tree *tree, rb_tree_node* z){
	U8 prio = to_remove->prio;

	if(list_arr[prio] == to_remove){
		pl_pop_min(list_arr, prio, tree, z);
		return;
	}
	TCB *traverse = list_arr[prio];

	while(traverse->next != to_remove && traverse->next != NULL){
		traverse = traverse->next;
	}
	if(traverse->next == NULL){
		return;
	}

	traverse->next = to_remove->next;
	to_remove->next = NULL;
}

void print_list(TCB **list_arr, U8 prio){
	TCB *traverse = list_arr[prio];
	printf("%d: ", prio);
	while(traverse != NULL){
#ifdef DEBUG_0
		printf("%d, ", traverse->tid);
#endif
		traverse = traverse->next;
	}
#ifdef DEBUG_0
		printf("\r\n");
#endif
}
