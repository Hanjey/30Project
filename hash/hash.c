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

#define file_org "/root/temp/first.txt"
#define file_sec "/root/temp/second.txt"
/*512k for one thread*/
#define max_hash 65536
typedef struct hash_node{
	char *start_address;
	char *end_address;
}HashNode;

int hash_thread(HashNode *org_node,HashNode *sec_node){
	char *start_org,*start_sec,*end_org,*end_sec;
	int total_num=0,eff_num=0;
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
		if(strcmp(str_org,str_sec)==0){
			eff_num++;
		}
		total_num++;
		start_org+=9;
		start_sec+=9;
	}
	printf("total_num:%d  eff_num:%d \n",total_num,eff_num);
	return 0;
}


int hash_file(){
	int fd_org,fd_sec;
	int thread_index;
	struct stat sb_org;
	struct stat sb_sec;
	void *add_org,*add_sec;
	fd_org=open(file_org,O_RDONLY);
	fd_sec=open(file_sec,O_RDONLY);
	fstat(fd_org,&sb_org);
	fstat(fd_sec,&sb_sec);
	add_org=mmap(NULL,sb_org.st_size, PROT_READ, MAP_PRIVATE, fd_org, 0);
	add_sec=mmap(NULL,sb_sec.st_size, PROT_READ, MAP_PRIVATE, fd_sec, 0);
	if(add_org==MAP_FAILED){
		printf("mmap org file failed!\n");
		goto end_map;
	}
	if(add_sec==MAP_FAILED){
		printf("mmap sec file failed!\n");
		goto end_map;
	}
	printf("org map address:%p end_address:%p org file size:%d\n",add_org,add_org+sb_org.st_size,sb_org.st_size);
	printf("sec map address:%p  sec file size:%d\n",add_sec,sb_sec.st_size);
	HashNode org_node,sec_node;
	org_node.start_address=add_org;
	org_node.end_address=add_org+sb_org.st_size;
	sec_node.start_address=add_sec;
	sec_node.end_address=add_sec+sb_sec.st_size;
	hash_thread(&org_node,&sec_node);
    int hash_lenth=max_hash*9;
	/*split hash file*/

	char *temp_ptr=add_org;
	char *temp_ptr2=add_sec;
	unsigned int *temp;
	unsigned int *end_address;
	/*create handle threads*/
//	while(temp_ptr<add_org+sb_org.st_size){
/*		end_address=temp_ptr+hash_lenth>add_org+sb_org.st_size?add_org+sb_org.st_size:temp_ptr+hash_lenth;
		printf("start address:%p end address:%p\n",temp_ptr,end_address);
		temp_ptr+=hash_lenth;*/
	//}
end_map:
	close(fd_org);
	close(fd_sec);
	if(add_org!=MAP_FAILED){
		if ((munmap((void *)add_org, sb_org.st_size)) == -1) {  
			perror("munmap failed\n");  
		}  	
	}
	if(add_sec!=MAP_FAILED){
		if ((munmap((void *)add_sec, sb_sec.st_size)) == -1) {  
			perror("munmap failed\n");  
		}  
	}
	return 0;
}
int main(){
	hash_file();
	return 0;
}


