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
/*shared structure with kernel*/
struct memory_block{
	void * buffer_add;
	unsigned long buffer_size;
};
typedef struct vm_info{
	int pid;
	struct memory_block mb;
	int flags;
}VM_INFO;

VM_INFO vm_info;
/*talk to walk_processs module*/
#define CHEN_WALK 0xEF
#define CHECK _IOR(CHEN_WALK,0x1,unsigned int)
#define FILE_ORIGAL "/root/temp/first.txt"
#define FILE_SECOND "/root/temp/second.txt"
/*file name can be a random num*/
int write_buffer_to_file(void  *buffer_add,int flags,int size){
	int fd;
	int ret=0;
	if(flags){
	    fd=open(FILE_SECOND,O_RDWR|O_CREAT);
		if(fd)
		ret=write(fd,buffer_add,size);
		if(ret<1){
			printf("write second error!\n");
		}
		printf("write second OK!\n");
	}else{
		fd=open(FILE_ORIGAL,O_RDWR|O_CREAT);
		ret = write(fd,buffer_add,size);
		if(ret<1){
			printf("write oril error!\n");
		}
		printf("write oril OK!\n");
	}
	close(fd);
	return 0;
}
/*send instruction to kernel*/
int beginDetect(){
	int ret;
	int fd;
	printf("vm_info.size:%d\n",vm_info.mb.buffer_size);
	fd=open("/proc/chen_walk",0);
	if(fd<0){
		printf("error open proc file\n");
		return -1;
	}
	ret=ioctl(fd,CHECK,&vm_info);
	printf("vm_info.size:%d\n",vm_info.mb.buffer_size);
	if(ret!=0){//normal
		printf("ioctl error\n");
		goto error;
	}
	close(fd);
	write_buffer_to_file(vm_info.mb.buffer_add,vm_info.flags,vm_info.mb.buffer_size);
error:
	return 0;
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
void init_vm_info(){
	vm_info.flags=0;
	vm_info.mb.buffer_add=malloc(4096);
	vm_info.mb.buffer_size=4096;
}
/* first start a thread to call a fun while function includes:
 * 1.call libvirt API to suspend vitual mechine,then thread sleep waitting for a signal
 * then begin first  detect
 * 2.when thread wake up by a signal,it resume VM and call shutdown API 
 * then begin second detect
 * */
int main_menu(virConnectPtr conn){
	int vmid;
	int pid=854;
	virDomainPtr dom=NULL;
	char *dom_source;
	list_domains(conn);
	printf("1.chose one virtual mechine,input vm id\n");
	scanf("%d",&vmid);
	/*transfer vmid to qemu process Id*/
/*	dom=virDomainLookupByID(conn,vmid);
	if(dom==NULL){
		printf("can not find %d vm\n",vmid);
		goto error_code;
	}
	dom_source=get_vm_image_path(conn,dom);
	if(dom_source==NULL){
		printf("can not get vm xml desc\n");
		goto error_code;
	}
	suspend_vm_state(dom);
	int i;
	for( i=0;i<10;i++){
		printf("sleep %d s\n",i);
		sleep(1);
	}
 	resume_vm_state(dom);	*/
	//VM_INFO vm_info;
	vm_info.pid=pid;
	init_vm_info();
	beginDetect();
error_code:
	return 0;
}


int main(int argc,char *argv[]){
	virConnectPtr conn;
	conn=virConnectOpen("qemu:///system");
	if(conn==NULL){
		printf("failed to open connection to qemu:///system!\n");
		return 1;
	}
	main_menu(conn);
	virConnectClose(conn);
	if(vm_info.mb.buffer_add)
		free(vm_info.mb.buffer_add);
	return 0;
}

