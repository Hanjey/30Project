/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 *
 * $Id$
 */

/**
 * @file	hash.h
 * @brief	
 */
typedef struct hash_node{                                                       
   char *start_address;                                                        
   char *end_address;                                                          
}HashNode;

int hash_file(char *fd_org,char *fd_sec);
