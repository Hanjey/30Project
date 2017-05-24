/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 */
#ifndef lint
static const char rcsid[] = "$Id$";
#endif /* not lint */

/**
 * @file	hello.c
 * @brief	
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(){
	unsigned int a=0x23edf033;
	unsigned int b=0x23edf034;
	char temp[30];
	memset(temp,0,20);
	//itoa(a,temp,16);
	sprintf(temp,"%x\n",a);
	printf("%s\n",temp);
	return 0;
}

