/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 */
#ifndef lint
static const char rcsid[] = "$Id$";
#endif /* not lint */

/**
 * @file	resolve_xml.c
 * @brief	
 */

#include <stdio.h>
#include <resolve_xml.h>
#include <string.h>
int list_sub_node(char *xml,char *nodename){
 	int sub_count;
	char *pos;
	char *start_pos;
	char *end_pos;
	char temp_str[20];
	int size=strlen(nodename);
	pos=xml;
	while(pos!=NULL){
		if(*pos=='<'){
			if(!strncmp(pos++,nodename,size)){
				break;
			}	
		}
		continue;
	}
	start_pos=pos+size+1;
	while(pos!=NULL&&)
	return 0;	
}

