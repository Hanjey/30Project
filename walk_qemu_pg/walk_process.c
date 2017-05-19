#include <linux/kernel.h>  
#include <linux/module.h>  
#include <linux/init.h>  
#include <linux/sched.h>  
#include <linux/list.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/file.h>

#include<linux/proc_fs.h>
#include "walk_process.h"
/*we can get physics page belong to one process:
 * 1、pagetable
 * page table can indicate page in mmaping  and not mapping but in swap cache
 * 2、pagecache
 * pagecache is mainly caused by read ahead manism
 * **/
/*define globle data zone*/ 
typedef struct _pfn_recoard{
	unsigned long pfn_m[500];
	unsigned long index;
}pfn_recoard;
/*define pfn data base*/

static pfn_recoard pfn_rec;
struct memory_block{
	unsigned long buffer_add;
	int buffer_size;
};


/*pid: vm qemu process id
 *mb: userspace address kernel need to write info to,mainly hash valu
 *flags:flags indicate if the first or second 
 * */
typedef struct _vm_info{
	int pid;
	struct memory_block mb;
	int flags;
}vm_info_t;
#define INDEX_PAGE_IN_RAM4K   0
#define INDEX_PAGE_IN_CACHE4K 1
#define INDEX_PAGE_IN_RAM2M   2
#define INDEX_PAGE_IN_CACHE2M 3
#define INDEX_COUNT 	      4
typedef struct _page_recoard{
	struct page *page_in_ram4k[300];
    struct page *page_in_cache4k[100];
	struct page *page_in_ram2m[100];
	struct page *page_in_cache2m[50];
	unsigned int index[4];
	char page_name[4][20];
}PageRecord;
typedef struct _page_vaddr_record{
	unsigned long pvaddr[500];
	int index;
}PageVaddrSet;

PageRecord *page_record;
PageVaddrSet *page_vset;
#define long_to_pfn(val) val>>12
/*get target process struct  by pid*/
static int getTargetProcess(struct task_struct **tprocess,int pid){
	struct task_struct *task,*p;
	struct list_head *pos;
	int count=0;
	task=&init_task;
        list_for_each(pos,&task->tasks){
                 p=list_entry(pos,struct task_struct,tasks);
		 if(p->pid==pid){
		 	*tprocess=p;
			break;
		 }
                 count++;
                // printk("%5d----------%s\n",p->pid,p->comm);
         }
	printk("<0> process count:%d\n",count);
	return 0;
}
/*walk all page table*/
#define huge_page_1g(pud) (pud_val(pud)>>0x7)&0x1
#define huge_page_2m(pmd) (pmd_val(pmd)>>0x7)&0x1

#define PAGE_PRESENT_CHEN 0X1
#define is_pmd_ram(x) (pmd_val(x) & PAGE_PRESENT_CHEN)
#define is_pte_ram(x) (pte_val(x) & PAGE_PRESENT_CHEN)
static int walk_page_table(struct task_struct *p, PageRecord *page_record){
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int count_4k=0;                                                                                                                      
	int count_4k_ram=0;                                                                                                                  
    int count_2m=0;                                                                                                                      
    int count_2m_ram=0;                                                                                                                   
    int count_1g=0;
	int count_pg=0,count_pu=0,count_pm=0,count_pt=0;
	int i;
	struct page *page;
	if(p->mm==NULL)
		return -1;
	if(p->mm->pgd==NULL)
		return -1;
	pgd=p->mm->pgd;
	printk("page table base:%p\n",pgd);
	while(count_pg<511){
		if(!pgd_none(*pgd)){
			printk("pgd:%lu\n",pgd_val(*pgd));
			pud=(pud_t *)pgd_page_vaddr(*pgd);
			/*walk for one pgd*/
			while(count_pu<511){
			//	pud=(pud_t *)pgd_page_vaddr(*pgd_temp);
				if((!pud_none(*pud))){
					printk("pud:%lu\n",pud_val(*pud));
					pmd=(pmd_t *)pud_page_vaddr(*pud);
					/*walk for one pud*/
					while(count_pm<511){
					//	pmd=(pmd_t *)pud_page_vaddr(*pud_temp);
						if((!pmd_none(*pmd))&&(!pmd_large(*pmd))){
							printk("pmd:%lu\n",pmd_val(*pmd));
							pte=(pte_t *)page_address(pmd_page(*(pmd)));
							/*walk for one pmd*/
							while(count_pt<511){
							 //	pte=(pte_t *)page_address(pmd_page(*(pmd)));
								if((!pte_none(*pte))){
									page=pte_page(*pte);
									if(is_pte_ram(*pte)){
										i =page_record->index[INDEX_PAGE_IN_RAM4K];
										page_record->page_in_ram4k[i]=page;
										page_record->index[INDEX_PAGE_IN_RAM4K]++;
									}else{
										i =page_record->index[INDEX_PAGE_IN_CACHE4K];
										page_record->page_in_cache4k[i]=page;
										page_record->index[INDEX_PAGE_IN_CACHE4K]++;
									}
									count_4k++;
								}
								pte++;
								count_pt++;
							}
						}
						/*if pmd point to a 2m page and is in ram*/
						if((!pmd_none(*pmd)&&pmd_large(*pmd))){
							page=pmd_page(*pmd);
							if(is_pmd_ram(*pmd)){
								i =page_record->index[INDEX_PAGE_IN_RAM2M];
								page_record->page_in_ram2m[i]=page;
								page_record->index[INDEX_PAGE_IN_RAM2M]++;
							}else{
								i =page_record->index[INDEX_PAGE_IN_CACHE2M];                                                               							  page_record->page_in_cache2m[i]=page;                                                                       								page_record->index[INDEX_PAGE_IN_CACHE2M]++;                                                                     
							}
							count_2m++;
							/*record 2M size page*/
						}
						pmd++;
						count_pm++;
					}
				}
				if((huge_page_1g(*pud))==1){
					count_1g++;
					//记录1G大小的页面
				}
				pud++;
				count_pu++;
			}
		}
		pgd++;
		count_pg++;
	}
	printk("4k page count:%d\n",count_4k);
	printk("4k page in ram count:%d\n",count_4k_ram); 
	printk("2m page count:%d\n",count_2m);
	printk("2m page in ram count:%d\n",count_2m_ram); 
	printk("1g page count:%d\n",count_1g);
	return 0;
}
/*when page->mapping ow bit clear, points to inode address_space , or NULL.If page mapped as anonymous 
 * memory, low bit is set, and it points to anon_vma object:
 * */

/*if page anno*/
/*
static inline int PageAnon(struct page *page)
{
		return ((unsigned long)page->mapping & PAGE_MAPPING_ANON) != 0;
}
*/
/*view page record information*/
static int view_page_record(PageRecord *p_record){
	int j=0;
	for(j=0;j<page_record->index[INDEX_PAGE_IN_RAM4K];j++){
		printk("%s-----%p\n",page_record->page_name[INDEX_PAGE_IN_RAM4K],page_record->page_in_ram4k[j]);	
	}
	for(j=0;j<page_record->index[INDEX_PAGE_IN_CACHE4K];j++){     
	    printk("%s-----%p\n",page_record->page_name[INDEX_PAGE_IN_CACHE4K],page_record->page_in_cache4k[j]);
	}
	for(j=0;j<page_record->index[INDEX_PAGE_IN_RAM2M];j++){     
	    printk("%s-----%p\n",page_record->page_name[INDEX_PAGE_IN_RAM2M],page_record->page_in_ram2m[j]);
	}
	for(j=0;j<page_record->index[INDEX_PAGE_IN_RAM2M];j++){     
	    printk("%s-----%p\n",page_record->page_name[INDEX_PAGE_IN_CACHE2M],page_record->page_in_cache2m[j]);
	}
	return 0;
}
static int view_page_vadd(PageVaddrSet *page_vset){
	int i=0;
	while(i<page_vset->index){
		printk("vaddr:%lu\n",page_vset->pvaddr[i]);
	}
	return 0;
}
/*get annon page virtual address by page*/
static unsigned int trans_page_vadd_anno(struct page *page){
	struct anon_vma *anon_vma;
	pgoff_t pgoff;
	struct anon_vma_chain *avc;
	unsigned long address=0;
    anon_vma = page_lock_anon_vma_read(page);
	if (!anon_vma)
			return -1;
	pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	anon_vma_interval_tree_foreach(avc, &anon_vma->rb_root, pgoff, pgoff) {
				struct vm_area_struct *vma = avc->vma;
				address = page_address_in_vma(page, vma);
	}
	return address;
}

/*get file mapping page virtual address by page*/
static unsigned int trans_page_vadd_file(struct page *page){
	struct address_space *mapping = page->mapping;
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct vm_area_struct *vma;
	unsigned long address=0;
	mutex_lock(&mapping->i_mmap_mutex);
	vma_interval_tree_foreach(vma, &mapping->i_mmap, pgoff, pgoff) {
			 address = page_address_in_vma(page, vma);
	}
	return address;
}
/*check if unmapping page existed in swap cache or page cache*/
static int check_page_cache(pte_t *un_map_ptes,int page_num){
	struct page *found_page=NULL;
	unsigned long found_pfn;
	int i=0;
	swp_entry_t entry;
	while(i<page_num){
		entry=pte_to_swp_entry(un_map_ptes[i]);
		found_page=find_get_page(swap_address_space(entry),entry.val);
		if(found_page!=NULL){
			found_pfn=page_to_pfn(found_page);
			pfn_rec.pfn_m[pfn_rec.index++]=found_pfn;
		}
		i++;	
	}
	return 0;
}
static int trans_page_to_vadd(PageRecord *page_record,PageVaddrSet *page_vset){
	int i=0;
	struct page *temp_page;
	unsigned long vaddr;
	if(page_record->index[INDEX_PAGE_IN_RAM4K]>0){
		while(i<page_record->index[INDEX_PAGE_IN_RAM4K]){
			temp_page=page_record->page_in_ram4k[i];
			if(PageAnon(temp_page)){
				vaddr=trans_page_vadd_anno(temp_page);
				page_vset->pvaddr[page_vset->index]=vaddr;
				page_vset->index++;
			}
			if(temp_page->mapping){
				vaddr=trans_page_vadd_file(temp_page);
				page_vset->pvaddr[page_vset->index]=vaddr;
				page_vset->index++;
			}
		}	
	}
	if(page_record->index[INDEX_PAGE_IN_CACHE4K]>0){
	
	}
	if(page_record->index[INDEX_PAGE_IN_RAM2M]>0){
	
	}
	if(page_record->index[INDEX_PAGE_IN_CACHE2M]>0){
	
	}	
	return 0;
}
/*page_to_pfn transfer a page structure to pfn
 *pte_page    transfer a pte to page structure
 * */
/*init page_record function*/
static int init_page_record(PageRecord *page_record,PageVaddrSet *page_vset){
	memset(page_record,0,sizeof(PageRecord));
	memset(page_vset,0,sizeof(PageVaddrSet));
	page_vset->index=0;
	page_record->index[INDEX_PAGE_IN_RAM4K]=0;                                                                                           
	page_record->index[INDEX_PAGE_IN_CACHE4K]=0;                                                                                         
	page_record->index[INDEX_PAGE_IN_RAM2M]=0;                                                                                           
	page_record->index[INDEX_PAGE_IN_CACHE2M]=0;
	strcpy(page_record->page_name[INDEX_PAGE_IN_RAM4K],"page_in_ram4k");
	strcpy(page_record->page_name[INDEX_PAGE_IN_CACHE4K],"page_in_cache4k");
    strcpy(page_record->page_name[INDEX_PAGE_IN_CACHE4K],"page_in_ram2m");
	strcpy(page_record->page_name[INDEX_PAGE_IN_CACHE4K],"page_in_cache2m");
	return 0;
}
/* pid: qemu process id corresponding to a vm 
 * buffer_add: userspace buffer address to store hash value
 * flags:0 first time to hash
 * flags:1 second time to hash
 * */
static int mainfun(vm_info_t *vm_info){
	int pid=vm_info->pid;
	unsigned long buffer_add=vm_info->mb.buffer_add;
	int flags=vm_info->flags;
	if(flags==1)goto hash_begin;
		struct task_struct *tpro=0;
		page_record=kmalloc(sizeof(PageRecord),GFP_KERNEL);
		page_vset=kmalloc(sizeof(PageVaddrSet),GFP_KERNEL);
		//printk("PageRecord size :%d\n",sizeof(PageRecord));
		init_page_record(page_record,page_vset);
		getTargetProcess(&tpro,pid);
		if(tpro==NULL){
			printk("find no process!\n");
			return -1;
		}
		printk("target process id:%d  pname:%s\n",tpro->pid,tpro->comm);
		walk_page_table(tpro,page_record);
		/*hash every page and write to buffer_add*/
hash_begin:
	//trans_page_to_vadd(page_record,page_vset);
	view_page_record(page_record);
	//view_page_vadd(page_vset);
	return 0;
}

/*export page content to file in userspace**/
static int export_page_to_file(struct file *file,unsigned long page_vadd){

	return 0;
}
/*talk to userspace*/
#define CHEN_WALK 0xEF
#define CHECK _IOR(CHEN_WALK,0x1,unsigned int)

struct proc_dir_entry *p_chen=NULL;
char entry_name[]="chen_walk";
static long ad_ioctl(struct file * filp,unsigned int ioctl,unsigned long arg){
	vm_info_t  vm_info;
	int ret=0;
	char *str="hello,chenmeng\n";
	switch(ioctl){
		case CHECK:
			if(ret=copy_from_user(&vm_info,(void *)arg,sizeof(vm_info_t))){
				return ret;
			}
			printk("pid get from userspace:%d\n",vm_info.pid);
			ret=copy_to_user((void *)vm_info.mb.buffer_add,str,strlen(str));
			vm_info.mb.buffer_size=strlen(str);
			if(ret){
				printk("fail to write info to userspace\n");
			}
			ret=copy_to_user(arg,&vm_info,sizeof(vm_info_t));
			if(ret){
				printk("fail to update vm_info information\n");
			}
			//mainfun(&vm_info);
			break;
		default:
			printk("unknown command!\n");
			break;
	}
	return 0;
}


static long ad_compat_ioctl(struct file * filp,unsigned int ioctl,unsigned long arg)
{
	return	ad_ioctl(filp,ioctl,arg);
}
static struct file_operations ad_fops={
	.unlocked_ioctl=ad_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl=ad_compat_ioctl,
#endif
};

struct proc_dir_entry* create_file_entry(void)
{
	p_chen=proc_create(entry_name,0,NULL,&ad_fops);
	return p_chen; 
}
void del_file_entry(void)
{
	if(p_chen)
		remove_proc_entry(entry_name,NULL);
}

void reclaim_memory(void){
	if(page_record)
		kfree(page_record);
	if(page_vset)
		kfree(page_vset);
}


static  __init int walk_init(void){
	create_file_entry();
	printk("init walk process\n");
	return 0;
}
static  __exit void walk_exit(void){
	reclaim_memory();
	del_file_entry();
	printk("exit exit process\n");
}
module_init(walk_init);
module_exit(walk_exit);



