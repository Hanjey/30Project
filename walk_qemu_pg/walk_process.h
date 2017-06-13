/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 *
 * $Id$
 */

/**
 * @file	walk_process.h
 * @brief	
 */
#define NO_PROCESS 0X1
#define ALLOC_MEM_ERROR 0x2
#define NOMM 0X3

extern unsigned long kallsyms_lookup_name(const char *name);
extern void
interval_tree_insert(struct interval_tree_node *node, struct rb_root *root);

extern void
interval_tree_remove(struct interval_tree_node *node, struct rb_root *root);

extern struct interval_tree_node *
interval_tree_iter_first(struct rb_root *root,
					 unsigned long start, unsigned long last);

extern struct interval_tree_node *
interval_tree_iter_next(struct interval_tree_node *node,
					unsigned long start, unsigned long last);

