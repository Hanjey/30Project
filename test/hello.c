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

#include <stdio.h>
#include<pthread.h>
#include<unistd.h>
#include <fcntl.h>
static int ret_value;
void *threada(void *arg){
	printf("thread a run\n");
	sleep(1);
}
/*wait for a*/
void *threadb(void *arg){
	pthread_t thread_a;                                                                                                                   
	pthread_create(&thread_a,NULL,threada,NULL);                                                                                          
	pthread_join(thread_a,NULL);
	printf("thread b run\n");
	sleep(1);
}
/*wait for b*/
void *threadc(void *arg){
	pthread_t thread_b;                                                                                                                   
	pthread_create(&thread_b,NULL,threadb,NULL);                                                                                          
	pthread_join(thread_b,NULL);
	printf("thread c run\n");
	sleep(1);
}
/*wait for c*/
void *threadd(void *arg){
	pthread_t thread_c;                                                                                                                   
	pthread_create(&thread_c,NULL,threadc,NULL);                                                                                          
	pthread_join(thread_c,NULL);
	printf("thread d run\n");
	sleep(1);
	ret_value =1;
	return (void *)&ret_value;
}
#define BYTES_PER_1M (1<<20)

#define ITEM_PER_2M (BYTES_PER_1M<<1)>>3
typedef struct stu{
	int i;
}stu;
int main(){
	unsigned int temp=0x8cd234f4;
	unsigned int temp2=0x12345678;
	const char *file_path="hello_test.txt";
	char str[20];
	sprintf(str,"%x ",temp);
	sprintf(str+9,"%x ",temp2);
/*	int *ret;
	pthread_t thread_d;
	pthread_create(&thread_d,NULL,threadd,NULL);
	pthread_join(thread_d,(void **)&ret);
	printf("ret:%d\n",*ret);
	printf("main thread run\n");*/
	printf("ITEM_PER_2M %d\n",ITEM_PER_2M);
	printf("str %s",str);
	int fd;
	fd=open(file_path,O_RDWR|O_CREAT);
	write(fd,str,20);
	close(fd);
	char str1[10];
	printf("please input:\n");
	scanf("%s\n",str1);
	printf("%s %d\n",str1,strcmp(str1,"yes"));
	printf("size of void *%d\n",(int)sizeof(void *));
	return 0;
}

