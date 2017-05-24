/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 *
 * $Id$
 */

/**
 * @file	Hash.h
 * @brief	
 */
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/mm.h>
#include<linux/highmem.h>

#define u32 unsigned int
u32 calc_check_num(struct page *page);
