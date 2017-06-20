/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 */
#ifndef lint
static const char rcsid[] = "$Id$";
#endif /* not lint */

/**
 * @file	vm_eraser_detect.c
 * @brief	
 */

#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <string.h>
#include <pthread.h>
#include "../hash/hash.h"
/*shared structure with kernel*/
struct memory_block{
	void * buffer_add;
	int buffer_size;
};
/*1G memory (page num)*/
#define MAX_MEMORY_HASH 262144
/*2M memory*/
#define HASH_MEMORY     2097152

#define NORMAL 0x0
#define NO_PROCESS 0x1
#define ALLOC_MEM_ERROR 0x2
#define NOMM 0X3
/*flags:bit 0 indicate the first or the second time 
 *		bit 1 indicate if need request hash ;1 need 0 no
 * */
typedef struct vm_info{
	int pid;
	struct memory_block mb;
	int flags; 
	unsigned int error_num;
	unsigned int total_memory;
	unsigned int memory_in_ram;
}VM_INFO;
static int ret_value=0;
static int oril_or_sec;
VM_INFO vm_info;
/*talk to walk_processs module*/
#define CHEN_WALK 0xEF
#define CHECK _IOR(CHEN_WALK,0x1,unsigned int)
#define HASH  _IOR(CHEN_WALK,0x3,unsigned int)//begin hash
#define FILE_ORIGAL "/root/temp/first.txt"
#define FILE_SECOND "/root/temp/second.txt"
/*file name can be a random num*/
int write_buffer_to_file(void  *buffer_add,int flags,int size){
	int fd;
	int ret=0;
	if(flags){
	    fd=open(FILE_SECOND,O_RDWR|O_CREAT|O_APPEND);
		if(fd)
		ret=write(fd,buffer_add,size);
		if(ret<1){
			printf("write second error!\n");
		}else
			printf("write second OK!\n");
	}else{
		fd=open(FILE_ORIGAL,O_RDWR|O_CREAT|O_APPEND);
		if(fd==-1){
			printf("error open file\n");
			return -1;
		}
		printf("open file OK!\n");
		ret = write(fd,buffer_add,size);
		if(ret<1){
			printf("write oril error!\n");
		}else
			printf("write oril OK!\n");
	}
	close(fd);
	return 0;
}
/*begin hash*/
int beginHash(){
	int fd,flags,ret=0;
	fd=open("/proc/chen_walk",0);
	if(fd<0){
	   printf("error open proc file\n");
	   return -1;
	}
begin_detect:
	memset(vm_info.mb.buffer_add,0,MAX_MEMORY_HASH*9);	
	ret=ioctl(fd,HASH,&vm_info);                                                                                                         
    printf("hash num:%d\n",vm_info.mb.buffer_size/9);                                                                                    
    flags=vm_info.flags;                                                                                                                 
    write_buffer_to_file(vm_info.mb.buffer_add,flags&0x1,vm_info.mb.buffer_size);                                                
    if(flags&0x2)goto begin_detect;//hash have not finished ,continue!
	if(ret!=0){//normal                                                                                                                  
	    printf("ioctl error\n");                                                                                                         
	    goto error;                                                                                                                      
	}         
error:	
	close(fd);
	return 0;
}
/*just collect memory*/
int beginDetect(){
	int ret;
	int fd;
	int hash_time=0;
	int i;
	int flags;
	fd=open("/proc/chen_walk",0);
	if(fd<0){
		printf("error open proc file\n");
		return -1;
	}
	ret=ioctl(fd,CHECK,&vm_info);
	//if(vm_info.flags==1)goto begin_check;
	switch(vm_info.error_num){
		case NORMAL: 
			break;
		case NO_PROCESS:
			printf("find no process!\n");
			goto error;
			break;
		case ALLOC_MEM_ERROR:
			printf("alloc memory error in kernel\n");
			goto error;
			break;
		case NOMM:
			printf("system error\n");
			goto error;
			break;
	}
	if(ret!=0){
		goto error;
	}
	printf("tatal memory(4KB page num):%d\n",vm_info.total_memory);
	printf("r a m memory(4KB page num):%d\n",vm_info.memory_in_ram);
	printf("----------------------------------------\n");
	close(fd);
	return 0;
error:
	close(fd);
	return -1;
}
/*<devices>
 *<emulator>/usr/libexec/qemu-kvm</emulator>
 *<disk type='file' device='disk'>
 *<driver name='qemu' type='qcow2'/>
 *<source file='/var/lib/libvirt/images/ubuntu14.04-chenmeng.qcow2'/>
 *<backingStore/>
 *<target dev='vda' bus='virtio'/>
 *<alias name='virtio-disk0'/>
 *<address type='pci' domain='0x0000' bus='0x00' slot='0x04' function='0x0'/>
 *</disk>
 *…… 
 *</devices>
 * */
char *get_vm_image_path(virConnectPtr conn,virDomainPtr dom){
	const char *filepath="/root/temp/vm_state";
	char *xml_desc=NULL;
	xmlDocPtr pdoc;
	xmlNodePtr cur;
	xmlChar *type;
	xmlChar *device;
	xmlChar *source_path=NULL;
	int size;
	xml_desc=virDomainGetXMLDesc(dom,VIR_DOMAIN_XML_SECURE);
	size=strlen(xml_desc);
	pdoc=xmlParseMemory(xml_desc,size);
	if(pdoc==NULL){
		printf("fail to parse xml\n");
		goto error_code;
	}
	cur=xmlDocGetRootElement(pdoc);
	printf("root name:%s\n",cur->name);
	if(cur==NULL){
		printf("fail to get root node\n");
		goto error_code;
	}
	cur=cur->xmlChildrenNode;
	while(cur!=NULL){
		if(!xmlStrcmp(cur->name,(const xmlChar *)"devices")){
			cur=cur->xmlChildrenNode;
			while(cur!=NULL){
				if(!xmlStrcmp(cur->name,(const xmlChar *)"disk")){
					type=xmlGetProp(cur,(const xmlChar *)"type");
					device=xmlGetProp(cur,(const xmlChar *)"device");
					if(!xmlStrcmp(type,(const xmlChar *)"file")&&!xmlStrcmp(device,(const xmlChar *)"disk")){
						cur=cur->xmlChildrenNode;
						while(cur!=NULL){
							if(!xmlStrcmp(cur->name,(const xmlChar *)"source")){
								source_path=xmlGetProp(cur,(const xmlChar *)"file");
								goto get_it;						
							}
							cur=cur->next;	
						}
					}
				}
				cur=cur->next;
			}
		}
		cur=cur->next;
	}
get_it:
	if(source_path!=NULL){
		printf("source_path:%s\n",source_path);
	}
	//printf("vm-xml-desc:%s\n",xml_desc);
	return source_path;
	/*resolve xml_desc to get image path*/
error_code:
	return NULL;
}
/*fun execute shutdown
 * */
int exec_fun(virConnectPtr conn){
	/*we need vm image path to suspend it */	
	return 0;
}
/*get vm insformation*/
int list_domains(virConnectPtr conn){
	int i;
	int numdomains;
	int *activeDomains;
	virDomainPtr dom;
	numdomains=virConnectNumOfDomains(conn);
	activeDomains=malloc(sizeof(int)*numdomains);
	numdomains=virConnectListDomains(conn,activeDomains,numdomains);
	printf("domain ID ---------domain name-----------\n");
	for(i=0;i<numdomains;i++){
		dom=virDomainLookupByID(conn,activeDomains[i]);
		if(dom!=NULL)
			printf("%4d----------%s\n",activeDomains[i],virDomainGetName(dom));
	}
	return 0;
}


/*thread fun to save/restore VM*/
void  save_vm_state(virDomainPtr dom,char *imagepath){
	virDomainInfoPtr info;
	if(!dom){
		printf("error, can not find guest to be saved!\n");
		return;
	}
	if(virDomainGetInfo(dom,info)<0){
		printf("error,can not check vm state\n");
		return;
	}
	if(info->state==VIR_DOMAIN_SHUTOFF){
		printf("not saving guest that is not running\n");
		return;
	}
	if(virDomainSave(dom,imagepath)<0){
		printf("ubable to save guest to %s\n",imagepath);
		return;
	}
	printf("guest state saved to %s\n",imagepath);
}

/*suspend/resume*/
int suspend_vm_state(virDomainPtr dom){
	if(virDomainSuspend(dom)<0){
		printf("error,can not suspend vm state\n");
		return;
	}
	return 0;
}
int resume_vm_state(virDomainPtr dom){
	if(virDomainResume(dom)<0){
		printf("error,can not resume vm state\n");
		return -1;	
	}
	return 0;
}

int shutdown_vm_state(virDomainPtr dom){
	if(virDomainShutdown(dom)<0){
		printf("error,can not shutdown vm state\n");
		return -1;
	}
	return 0;
}
int init_vm_info(){
	vm_info.flags=0;
	vm_info.mb.buffer_add=malloc(MAX_MEMORY_HASH*9);
	if(vm_info.mb.buffer_add==NULL)
		return -1;
	vm_info.mb.buffer_size=MAX_MEMORY_HASH*9;
	vm_info.total_memory=0;
	vm_info.memory_in_ram=0;
	return 0;
}
/*syn threads*/
void *thread_suspend(virDomainPtr dom){
	if(suspend_vm_state(dom)<0){
		ret_value=-1;
		printf("suspend vm error\n");
		goto error_return;
	}
	printf("suspend vm……\n");
	sleep(1);
error_return:
	return (void *)&ret_value;
}

void *thread_fir(virDomainPtr dom){
	pthread_t sus_thread;
	int retPid;
	int *ret;
	retPid = pthread_create(&sus_thread,NULL,thread_suspend,dom);
	if(retPid!=0){                          
		printf("create suspend thread error!\n");
		ret_value=-1;
		goto error_return;
	}                                       
	pthread_join(sus_thread,(void **)&ret);
	if(*ret<0){
		ret_value=-1;
		goto error_return;
	}
	printf("first detect------\n");
	beginHash();
error_return:
	return (void *)&ret_value;
}

void *thread_shutdown(virDomainPtr dom){
	pthread_t fir_thread;
	int retPid;
	int *ret;
	retPid = pthread_create(&fir_thread,NULL,thread_fir,dom);
	if(retPid!=0){
		ret_value=-1;		
	    printf("create first thread error!\n");
		goto error_return;
	}                                       
	pthread_join(fir_thread,(void **)&ret);
	if(*ret<0){
		ret_value=-1; 
		goto error_return;
	}
	if(resume_vm_state(dom)<0){  
		ret_value=-1;       
		printf("resume vm state error\n");		
	    goto error_return;                                                                                                               
	}
	printf("shutdown vm……\n");
	if(shutdown_vm_state(dom)<0){
		ret_value=-1;
		printf("shutdown vm state error\n");
		goto error_return;
	}
	sleep(5);
error_return:
	return (void *)&ret_value;
}

void *thread_sec(virDomainPtr dom){
	pthread_t sd_thread;
	int retPid;
	int *ret;
	retPid = pthread_create(&sd_thread,NULL,thread_shutdown,dom);
	if(retPid!=0){                          
	        printf("create shutdown thread error!\n");
			ret_value =-1;
			goto error_return;
	}                                       
	pthread_join(sd_thread,(void **)&ret);
	if(*ret<0){
		ret_value=-1;
		printf("join secthread error\n");
		goto error_return;
	}
	vm_info.flags |= 0x1;
	printf("begin second detect……\n");
	beginHash();
error_return:
	return (void *)&ret_value;
}

/* first start a thread to call a fun while function includes:
 * 1.call libvirt API to suspend vitual mechine,then thread sleep waitting for a signal
 * then begin first  detect
 * 2.when thread wake up by a signal,it resume VM and call shutdown API 
 * then begin second detect
 * */
int main_menu(virConnectPtr conn){
	int vmid;
	pthread_t sec_thread;
	virDomainPtr dom=NULL;
	int retPid;
	char *dom_source;
	int *ret;
	char str[10];
	char reve[10]; 
	list_domains(conn);
	printf("1.chose one virtual mechine,input vm id\n");
	scanf("%d",&vmid);
	/*transfer vmid to qemu process Id*/
	dom=virDomainLookupByID(conn,vmid);
	if(dom==NULL){
		printf("can not find %d vm\n",vmid);
		goto error_code;
	}
	/*dom_source=get_vm_image_path(conn,dom);
	if(dom_source==NULL){
		printf("can not get vm xml desc\n");
		goto error_code;
	}*/
	/*first step:collect memory*/
	if(beginDetect()==-1)goto error_code;
	printf("if begin detect memory?\n");
	scanf("%s",str);
	if(!strcmp(str,"yes")){
		printf("begin detect-----------------\n");
		if(init_vm_info()==-1)goto error_code;
		goto begin_hash;
	}
	else
		goto error_code;
begin_hash:
	retPid = pthread_create(&sec_thread,NULL,thread_sec,dom);
	if(retPid!=0){
		printf("create sec thread error!\n");
		goto error_code;
	}
	pthread_join(sec_thread,(void **)&ret);
	if(*ret<0){
		printf("join sec thread error\n");
		goto error_code;
	}
	printf("OK,Hash finished\n");
	printf("if begin caculate match rate?(yes/no)\n");
	
begin_check:
	scanf("%s",reve);
	if(!strcmp(reve,"yes")){ 
		printf("begin cacluate match rate------\n");
		hash_file(FILE_ORIGAL,FILE_SECOND);
	 }
error_code:
	return 0;
}


int main(int argc,char *argv[]){
	if(argc!=3){
		printf("error argument\n");
		return -1;
	}
	int pid=0;
	if(strcmp(argv[1],"-p")==0){
		pid=atoi(argv[2]);
	}else{
		printf("error pid!\n");
		return -1;
	}	
	virConnectPtr conn;
	conn=virConnectOpen("qemu:///system");
	if(conn==NULL){
		printf("failed to open connection to qemu:///system!\n");
		return 1;
	}
	vm_info.pid=pid;
	main_menu(conn);
	virConnectClose(conn);
	if(vm_info.mb.buffer_add)
		free(vm_info.mb.buffer_add);
	return 0;
}

