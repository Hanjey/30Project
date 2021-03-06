/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 */
#ifndef lint
static const char rcsid[] = "$Id$";
#endif /* not lint */

/**
 * @file	hash.c
 * @brief	
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <error.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "hash.h"


#define first_file  "/root/temp/first.txt"
#define second_file  "/root/temp/second.txt"
/*4M for once hash*/
#define once_hash 4194304


static int total_num=0;
static int eff_num=0;
int hash_thread(HashNode *org_node,HashNode *sec_node,int flags){
	char *start_org,*start_sec,*end_org,*end_sec;
	char *zero_str="cdb22d25";
	float res;
//	int total_num=0,eff_num=0;
	start_org=org_node->start_address;
	end_org=org_node->end_address;
	start_sec=sec_node->start_address;
	end_sec=sec_node->end_address;
	char str_org[9],str_sec[9];
	while(start_org<end_org){
		strncpy(str_org,start_org,8);
		str_org[8]='\0';
		strncpy(str_sec,start_sec,8);
		str_sec[8]='\0';
		if(strcmp(str_org,zero_str)==0)
			goto con;
		if(strcmp(str_org,str_sec)==0){
			eff_num++;
		}
		total_num++;
		con:
		start_org+=9;
		start_sec+=9;
	}
//	printf("total_num:%d  eff_num:%d \n",total_num,eff_num);
	if(flags){
		res=(float)eff_num/total_num*100;
		printf("total_num:%d  eff_num:%d \n",total_num,eff_num);
		printf("match rate:%.2f \%\n",res);
	}
	return 0;
}


int hash_file(char *file_org,char *file_sec){
	int fd_org,fd_sec;
	int hash_index=0;
	struct stat sb_org;
	struct stat sb_sec;
	void *add_org,*add_sec;
	int flags=0;
	int map_size=0;
	HashNode org_node,sec_node;
	fd_org=open(file_org,O_RDONLY);
	fd_sec=open(file_sec,O_RDONLY);
	fstat(fd_org,&sb_org);
	fstat(fd_sec,&sb_sec);
	printf("org.size:%d\n",sb_org.st_size);
	printf("sec.size:%d\n",sb_sec.st_size);
begin_mmap:
	map_size=sb_org.st_size-hash_index*once_hash<once_hash?sb_org.st_size-hash_index*once_hash:once_hash;
	if(map_size<once_hash)flags=1;
	add_org=mmap(NULL,map_size, PROT_READ, MAP_PRIVATE, fd_org,hash_index*once_hash);
	add_sec=mmap(NULL,map_size, PROT_READ, MAP_PRIVATE, fd_sec,hash_index*once_hash);
	if(add_org==MAP_FAILED){
		printf("mmap org file failed!\n");
		goto end_map;
	}
	if(add_sec==MAP_FAILED){
		printf("mmap sec file failed!\n");
		goto end_map;
	}
	printf("org map address:%p map_size:%d\n",add_org,map_size);
	printf("sec map address:%p map_size:%d\n",add_sec,map_size);
	org_node.start_address=add_org;
	org_node.end_address=add_org+map_size;
	sec_node.start_address=add_sec;
	sec_node.end_address=add_sec+map_size;
	hash_thread(&org_node,&sec_node,flags);
//	if(!flags)goto begin_mmap;
	/*split hash file*/
	if(add_org!=MAP_FAILED){
		if ((munmap((void *)add_org,map_size)) == -1) {  
			perror("munmap failed\n");  
		}  	
	}
	if(add_sec!=MAP_FAILED){
		if ((munmap((void *)add_sec,map_size)) == -1) {  
			perror("munmap failed\n");  
		}  
	}
	hash_index++;
	if(!flags)goto begin_mmap;
end_map:
	 close(fd_org);
	 close(fd_sec);
	return 0;
}
/*
int main(){
	hash_file(first_file,second_file);
	return 0;
}
*/

