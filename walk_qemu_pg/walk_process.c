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
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <uapi/linux/sched.h>
#include <linux/radix-tree.h>
#include<linux/proc_fs.h>
#include <linux/rcupdate.h>
#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/kallsyms.h>
#include "./hashtest/Hash.h"
#include "walk_process.h"
/*we can get physics page belong to one process:
 * 1、pagetable
 * page table can indicate page in mmaping  and not mapping but in swap cache
 * 2、pagecache
 * pagecache is mainly caused by read ahead manism
 * **/


/*info  block communicate with userspace*/
struct memory_block{
	unsigned long buffer_add;
	int buffer_size;
};

/*pid: vm qemu process id
 *mb: userspace address kernel need to write info to,mainly hash valu
 *flags:flags indicate if the first request 0 first ;1 not first and bit 2 indicate hash times 
 * */
typedef struct _vm_info{
	int pid;
	struct memory_block mb;
	int flags;
	unsigned int error_num;
	unsigned int total_memory;
	unsigned int memory_in_ram;
}vm_info_t;

#define BYTES_PER_1M (1<<20)
/*first level store 10 pointer ,each pointer can point a 2M page, and each page can store page pointers*/
#define MAX_FIRST_LEVEL 10
#define ITEM_PER_2M ((BYTES_PER_1M<<1)>>3)
#define index_to_first(index) (index/ITEM_PER_2M)

/*struct record all kinds of page*/
/*max memory is 2G 2048M needs 2048*1024/4*8*/
#define MAX_PAGE_IN_RAM 4194304  
/*max page num a time hash 1G memory 262144 4K pages*/
#define MAX_HASH_ONCE   262144

typedef struct _page_recoard{
	unsigned long *first_level[MAX_FIRST_LEVEL];
	//	struct page *page_in_ram[MAX_PAGE_IN_RAM];
	unsigned short first_index;//first avail index in first_level
	unsigned int second_index;//index in first avail 2M page
}PageRecord;
/*each process own one*/
typedef struct _walk_vma_control{
	int tatal_vma;
	int tatal_page;
	unsigned long first_vma_address;//record first vma belong to one process
	unsigned long temp_address;//next virtual address needing to scan
	bool is_finished;
}walk_vma_controller;

/*control a hash course for a process*/
typedef struct hash_control{
	int index;//corresponding to PageRecord ->first_level,each time hash a 2M Page
	int curr_hash_num; //how many page has been hashed this time;
	bool is_finished;//if hash finished;
}hash_control;

typedef struct pro_info{
	struct task_struct *tpro;
	walk_vma_controller *wvc;
	hash_control p_hash_con;
	PageRecord *page_record;
	char *page_hash;
}pro_info;

pro_info *pro_info_t;
/*toatl page num*/
/*pthreads_p record all the threads belonging to one process
 *pthreads_p array storing threads strucure address
 *index first available position 
 * */
typedef struct pthreads_p{
	struct task_struct *pthreads_p[64];
	unsigned char index;
}Pthreads_p;
Pthreads_p pthreads;
#define long_to_pfn(val) val>>12
/*get target process structure  by pid*/
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
	printk("process count:%d\n",count);
	return 0;
}
/*get all threads belonging to a process,and store thread structure address to pthreads*/
static int getProcessThreads(struct task_struct *tprocess){
	pthreads.index=0;
	struct list_head *pos;
	struct task_struct *p;
	list_for_each(pos,&tprocess->thread_group){
		p=list_entry(pos,struct task_struct,thread_group);
		pthreads.pthreads_p[pthreads.index++]=p;
		printk("thread id :%d\n",p->pid);
	}
	return 0;
}
/*--------walk process  page table*/
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
	while(count_pg<512){
		if(!pgd_none(*pgd)){
			//	printk("pgd:%lu\n",pgd_val(*pgd));
			pud=(pud_t *)pgd_page_vaddr(*pgd);
			/*walk for one pgd*/
			while(count_pu<512){
				//	pud=(pud_t *)pgd_page_vaddr(*pgd_temp);
				if((!pud_none(*pud))){
					//printk("pud:%lu\n",pud_val(*pud));
					pmd=(pmd_t *)pud_page_vaddr(*pud);
					/*walk for one pud*/
					while(count_pm<512){
						//	pmd=(pmd_t *)pud_page_vaddr(*pud_temp);
						if((!pmd_none(*pmd))&&(!pmd_trans_huge(*pmd))){
							//printk("pmd:%lu\n",pmd_val(*pmd));
							pte=(pte_t *)page_address(pmd_page(*(pmd)));
							/*walk for one pmd*/
							while(count_pt<512){
								//	pte=(pte_t *)page_address(pmd_page(*(pmd)));
								if((!pte_none(*pte))){
									page=pte_page(*pte);
									if(is_pte_ram(*pte)){
										//			i =page_record->index[INDEX_PAGE_IN_RAM4K];
										//			page_record->page_in_ram4k[i]=page;
										//			page_record->index[INDEX_PAGE_IN_RAM4K]++;
									}else{//cache only reserve page in swap cache not in file cache 
										if(1){
											//	i =page_record->index[INDEX_PAGE_IN_CACHE4K];
											//	page_record->page_in_cache4k[i]=page;
											//	page_record->index[INDEX_PAGE_IN_CACHE4K]++;
										}
									}
									count_4k++;
								}
								pte++;
								count_pt++;
							}
						}
						/*if pmd point to a 2m page and is in ram*/
						if((!pmd_none(*pmd)&&pmd_trans_huge(*pmd))){
							page=pmd_page(*pmd);
							if(is_pmd_ram(*pmd)){
								//	i =page_record->index[INDEX_PAGE_IN_RAM2M];
								//	page_record->page_in_ram2m[i]=page;
								//	page_record->index[INDEX_PAGE_IN_RAM2M]++;
							}else{//cache only reserve page in swap cache not in file cache 
								if(1){
									//		i =page_record->index[INDEX_PAGE_IN_CACHE2M];  
									//		page_record->page_in_cache2m[i]=page; 
									//		page_record->index[INDEX_PAGE_IN_CACHE2M]++; 
								}								
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
	printk("--------------------------------------------\n");
	return 0;
}
static int pagenum_in_cache;
/*declare un export functions*/
typedef struct page* (*LOOKUPSWAPCACHE)(swp_entry_t);
typedef int (*PMDHUGE)(pmd_t pud);
typedef struct page* (*FOLLOWHUGEPMD)(struct mm_struct *mm, unsigned long address,
		pmd_t *pmd, int write);
//typedef struct page* (*FOLLOWTRANSHUGEPMD)(struct vm_area_struct *vma, unsigned long addr,pmd_t *pmd,unsigned int flags);

LOOKUPSWAPCACHE lookup_swap_cache_chen;
PMDHUGE pmd_huge;
FOLLOWHUGEPMD follow_huge_pmd;
//FOLLOWTRANSHUGEPMD follow_trans_huge_pmd_chen;

static  int init_ex_fun(void){
	lookup_swap_cache_chen=(LOOKUPSWAPCACHE)kallsyms_lookup_name("lookup_swap_cache");
	pmd_huge=(PMDHUGE)kallsyms_lookup_name("pmd_huge");
	follow_huge_pmd=(FOLLOWHUGEPMD)kallsyms_lookup_name("follow_huge_pmd");
	//	follow_trans_huge_pmd_chen=(FOLLOWTRANSHUGEPMD)kallsyms_lookup_name("follow_trans_huge_pmd");
	return 0;
}
static  int add_item(struct page *page);
static inline int  split_huge_page_chen(struct page *page){
	int i,j;
	while(i<512){
		add_item(page);
		page++;
		i++;
	}
}
/*return value*/

#define MEMORYLESS -1

static int extend_store(void){                                                                                                           
	pro_info_t->page_record->first_index+=1;
	int first_index= pro_info_t->page_record->first_index;                                                                               
	pro_info_t->page_record->first_level[first_index]=kmalloc(MAX_PAGE_IN_RAM,GFP_KERNEL);                                               
	memset(pro_info_t->page_record->first_level[first_index],0,MAX_PAGE_IN_RAM);                                                         
	pro_info_t->page_record->second_index=0;                                                                                             
	return 0;                                                                                                                            
}   
static int mapcount;
static  int add_item(struct page *page){        
	int count;
	count=atomic_read(&(page)->_mapcount);
	if(count==0)mapcount++;
	int first_index= pro_info_t->page_record->first_index;                
	int second_index=pro_info_t->page_record->second_index;  
	struct page **page_add=pro_info_t->page_record->first_level[first_index];
	page_add[second_index]=page;  
	if(second_index==(ITEM_PER_2M-1)){
		printk("extend store\n");		
		extend_store();  
	}else{
		pro_info_t->page_record->second_index++;
	}                        
	return 0;                                                    
}     

static int check_page_cache(pte_t *pte, walk_vma_controller *wvc);
/*static int get physics address by virtual address*/
static int  *trans_vaddr_to_paddr(struct vm_area_struct *vma,unsigned long address,walk_vma_controller *wvc){
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
	spinlock_t *ptl;
	unsigned long pfn_temp;
	struct page *page;
	struct mm_struct *mm;
	mm=vma->vm_mm;
	pgd = pgd_offset(mm, address);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		goto no_pte;	
	pud = pud_offset(pgd, address);
	if (pud_none(*pud))
		goto no_pud;
	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd))
		goto no_pmd;
	if (pmd_huge(*pmd) && vma->vm_flags & VM_HUGETLB){
		page=follow_huge_pmd(mm,address,pmd,0);
		/*handle huge page*/
		if(page!=NULL){
			split_huge_page_chen(page);
			wvc->tatal_page+=512;
		}
		return 512;
	}
	if(pmd_trans_huge(*pmd)){
		page = pte_page(*(pte_t *)pmd);
		if(page!=NULL){
			/*512 pages will be consumed!*/
			//	printk("huge page\n");
			split_huge_page_chen(page);	
			wvc->tatal_page+=512;    
			return 512;    
		}	
		return 512;
	}
	ptep = pte_offset_map_lock(mm, pmd, address, &ptl);	
	pte = *ptep;
	if (!pte_present(pte)){
		if (pte_none(pte))	{
			//	printk("pte is null\n");
			pte_unmap_unlock(ptep, ptl);
			goto no_pte;
		}
		/*check page cache*/
		if(check_page_cache(ptep,wvc))
			pagenum_in_cache++;
	}else{
		pfn_temp=pte_pfn(pte);
		page=pte_page(pte);
		//page = vm_normal_page(vma, address, pte);
		if(page!=NULL){
			//		printk("notmal page\n");
			add_item(page);
			wvc->tatal_page++;
		} 
	}
	pte_unmap_unlock(ptep, ptl);
	/*record page*/
	return 1;
no_pud:
	return 1<<18;
no_pmd:
	return 1<<9;
no_pte:
	return 1;
}
/*get mapping information for a vma*/
static int get_mapping_info_by_vma(struct vm_area_struct *vma_temp,walk_vma_controller *wvc){
	unsigned long temp_address;
	int temp_num;
	int res=0;
	temp_address=vma_temp->vm_start;
	while(temp_address<vma_temp->vm_end){
		temp_num = trans_vaddr_to_paddr(vma_temp,temp_address,wvc);
		temp_address+=temp_num<<PAGE_SHIFT;
	}
	return res;
}
/*get all mapping information by a process*/
static int walk_vma_list_for_process(struct task_struct *task_temp,walk_vma_controller *wvc){
	struct list_head *pos;
	struct vm_area_struct *vma_temp;
	vma_temp=task_temp->mm->mmap;
	if(task_temp->mm==NULL||task_temp->mm->mmap==NULL){
		printk("mmap is null\n");
		return NOMM;
	}
	if(wvc==NULL){
		printk("wvc is null\n");
		return NOMM;
	}
	int res=0;
	do{
		//printk("vma range:start:%lx end:%lx\n",vma_temp->vm_start,vma_temp->vm_end);
		res = get_mapping_info_by_vma(vma_temp,wvc);
		/*only record vma finish scanning*/
		wvc->tatal_vma+=(vma_temp->vm_end-vma_temp->vm_start)>>PAGE_SHIFT;
		vma_temp=vma_temp->vm_next;
		cond_resched();
	}while(vma_temp!=NULL&&vma_temp!=task_temp->mm->mmap);
	if(vma_temp==task_temp->mm->mmap){
		wvc->is_finished=true;
	}
	printk("finished\n");	
	return res;
}

/* 
 * PageAnon: wheather a page maps a file or just a annon page
 * when page->mapping ow bit clear, points to inode address_space , or NULL.If page mapped as anonymous 
 * memory, low bit is set, and it points to anon_vma object:
 * */
/*if page anno*/
/*
   static inline int PageAnon(struct page *page)
   {
   return ((unsigned long)page->mapping & PAGE_MAPPING_ANON) != 0;
   }
   */
/*get page hash value according to PageRecord*/
static int get_page_hash_value(unsigned int seclect_index){
	int j=0;
	int i=0;
	int ret=0;
	struct page **temp_page;
	unsigned short first_index;
	unsigned int second_index;
	unsigned int max_limit;
	u32 hash_temp;
	PageRecord *p_record;
	p_record=pro_info_t->page_record;
	if(pro_info_t->p_hash_con.index>0)goto record;
	/*in fact it is a 2 ventor array*/
	pro_info_t->page_hash=(char *)kmalloc(MAX_HASH_ONCE*9,GFP_KERNEL);
record:
	memset(pro_info_t->page_hash,0,MAX_HASH_ONCE*9);	
	temp_page=p_record->first_level[seclect_index];
	if(seclect_index==p_record->first_index)
		max_limit=p_record->second_index;
	else
		max_limit=ITEM_PER_2M;
	for(j=0;j<max_limit;j++){
		//printk("page_count:%d\n",temp_page[j]->_mapcount);
		hash_temp=calc_check_num(temp_page[j]);
		//	printk("hash value:%08x\n",hash_temp);
		sprintf(pro_info_t->page_hash+i,"%08x ",hash_temp);
		i+=9;
	}
	pro_info_t->p_hash_con.curr_hash_num=j;
end:
	return 0;
}
/*view_hash_value*/
static int view_hash_value(u32 *page_hash){
	int i=0;
	if(page_hash==NULL){
		printk("page hash is NULL\n");
		return -1;
	}
}
/*------view page record information*/
static int view_page_record(PageRecord *p_record){
	int j=0;
	for(j=0;j;j++){     
	}
	return 0;
}
/*------get annon page virtual address by page*/
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

/*------get file mapping page virtual address by page*/
static unsigned int trans_page_vadd_file(struct page *page){
	struct address_space *mapping = page->mapping;
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct vm_area_struct *vma;
	unsigned long address=0;
	mutex_lock(&mapping->i_mmap_mutex);
	vma_interval_tree_foreach(vma, &mapping->i_mmap, pgoff, pgoff) {
		address = page_address_in_vma(page, vma);
	}
	mutex_unlock(&mapping->i_mmap_mutex);
	return address;
}
//struct address_space *swapper_spaces=(struct address_space *)ffffffff81c5f520;
/*check if unmapping page existed in swap cache or page cache*/
static int check_page_cache(pte_t *pte, walk_vma_controller *wvc){
	if(pte_file(*pte))return 0;
	struct page *found_page=NULL;
	swp_entry_t entry;
	entry=pte_to_swp_entry(*pte);
	found_page = lookup_swap_cache_chen(entry);
	if(found_page!=NULL){
		add_item(found_page);
		wvc->tatal_page++;
		return 1;
	}
	return 0;
}
/*page_to_pfn transfer a page structure to pfn
 *pte_page    transfer a pte to page structure
 * */
/*init page_record function*/
static int  init_pro_info(struct pro_info *pro_info_t){
	walk_vma_controller *wvc;
	pro_info_t->page_record=kmalloc(sizeof(PageRecord),GFP_KERNEL);
	if(pro_info_t->page_record==NULL)return 1;
	memset(pro_info_t->page_record,0,sizeof(PageRecord));
	pro_info_t->page_record->first_level[0]=kmalloc(MAX_PAGE_IN_RAM,GFP_KERNEL);
	if(pro_info_t->page_record->first_level[0]==NULL)return 1;
	pro_info_t->page_record->first_index=0;
	pro_info_t->page_record->second_index=0;
	pro_info_t->wvc=kmalloc(sizeof(walk_vma_controller),GFP_KERNEL);
	wvc=pro_info_t->wvc;
	wvc->tatal_vma=0;
	wvc->tatal_page=0;
	wvc->is_finished=false;
	pro_info_t->p_hash_con.index=0;
	pro_info_t->p_hash_con.curr_hash_num=0;
	pro_info_t->p_hash_con.is_finished=false;
	return 0;
}
/* 
 * id: qemu process id corresponding to a vm 
 * buffer_add: userspace buffer address to store hash value
 * flags:0 first time to hash
 * flags:1 second time to hash
 * */

/*check all page cache belong to one process not including swap cache*/
#ifdef __KERNEL__
#define RADIX_TREE_MAP_SHIFT	(CONFIG_BASE_SMALL ? 4 : 6)
#else
#define RADIX_TREE_MAP_SHIFT	3	/** For more stressful testing */
#endif

#define RADIX_TREE_MAP_SIZE	(1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK	(RADIX_TREE_MAP_SIZE-1)

#define RADIX_TREE_TAG_LONGS	\
	((RADIX_TREE_MAP_SIZE + BITS_PER_LONG - 1) / BITS_PER_LONG)
#if LINUX_VERSION_CODE>= KERNEL_VERSION(3,10,1)
struct radix_tree_node {
	unsigned int	height;		/** Height from the bottom */
	unsigned int	count;
	union {
		struct radix_tree_node *parent;	/** Used when ascending tree */
		struct rcu_head	rcu_head;	/** Used when freeing node */
	};
	void __rcu	*slots[RADIX_TREE_MAP_SIZE];
	unsigned long	tags[RADIX_TREE_MAX_TAGS][RADIX_TREE_TAG_LONGS];
};
#endif
static inline void *indirect_to_ptr(void *ptr)
{
	return (void *)((unsigned long)ptr & ~RADIX_TREE_INDIRECT_PTR);
}

/*max item according to the specific height*/
#define RADIX_TREE_INDEX_BITS  (8 /** CHAR_BIT */ * sizeof(unsigned long))
static __init unsigned long maxindex(unsigned int height)
{
	unsigned int width = height * RADIX_TREE_MAP_SHIFT;
	int shift = RADIX_TREE_INDEX_BITS - width;

	if (shift < 0)
		return ~0UL;
	if (shift >= BITS_PER_LONG)
		return 0UL;
	return ~0UL >> shift;
}

/*test bit n is 1*/
#define SHIFT 6
#define MASK 0x3f


int chen_test_bit(const unsigned long *vaddr,int size,int offset){
	const unsigned long *p = vaddr + (offset >> SHIFT);
	int bit = offset & MASK, res;	
	if (offset >= size)
		return 0;
	res=*p&(1<<bit);
	return res;
}
/*get other page without in pagetbale*/

/*main function to execute*/
static int collect_phy_memory(vm_info_t *vm_info){
	int flags=vm_info->flags;
	int ret;
	//if(flags==1)goto collect;
	struct task_struct *tpro=0;
	walk_vma_controller *vma_controller;
	int pid=vm_info->pid;
	int i;
	getTargetProcess(&tpro,pid);
	if(tpro==NULL){
		printk("find no process!\n");
		ret= 1;
		goto error_return;
	}
	pro_info_t=kmalloc(sizeof(struct pro_info),GFP_KERNEL);
	pro_info_t->tpro=tpro;
	if(init_pro_info(pro_info_t)==1)return 2;
	printk("target process id:%d  pname:%s\n",tpro->pid,tpro->comm);
collect:
	ret=walk_vma_list_for_process(pro_info_t->tpro,pro_info_t->wvc);
	printk("mapcount:%d\n",mapcount);
	if(ret==NOMM)goto error_return;
	vma_controller=pro_info_t->wvc;
	if(1){
		vm_info->total_memory=pro_info_t->wvc->tatal_vma;
		vm_info->memory_in_ram=pro_info_t->wvc->tatal_page;
		printk("all mapping page:%d tatal memory:%d KB\n",vma_controller->tatal_vma,vma_controller->tatal_vma*4);
		printk("page in ram:%d  memory in ram %d KB\n",vma_controller->tatal_page,vma_controller->tatal_page*4);
		printk("first index :%d   second_index :%d\n",pro_info_t->page_record->first_index,pro_info_t->page_record->second_index);
	}
error_return:
	return ret;
}
static int begin_index=0;
static int hash_page(vm_info_t *vm_info){
	if(pro_info_t==NULL)
		return -1;
	int i=0;
	printk("buffer  address:%lx\n",vm_info->mb.buffer_add);
	printk("buffer  size:%d\n",vm_info->mb.buffer_size);
	int first_index =pro_info_t->page_record->first_index;
	//int begin_index = pro_info_t->p_hash_con.index;
	get_page_hash_value(begin_index);
	if(begin_index==first_index){
		vm_info->flags &= ~0x2;//set hash end
		if(vm_info->flags&0x1){  
		    /*only second time finished will set true!*/	
		   	pro_info_t->p_hash_con.is_finished=true;//if need reclaim_memory
		}else{
			begin_index=0;
		}
	}else{
		vm_info->flags |= 0x2;//set hash continue
		begin_index++;
	}
	return 0;
}
/*export page content to file in userspace*/
static int export_hashvalue(vm_info_t  *vm_info){
	int ret;
	if(pro_info_t->page_hash==NULL)
		goto error_return;
	if(vm_info->mb.buffer_add==0)
		goto error_return;
	ret=copy_to_user((void *)vm_info->mb.buffer_add,(void *)pro_info_t->page_hash,pro_info_t->p_hash_con.curr_hash_num*9);
	vm_info->mb.buffer_size=pro_info_t->p_hash_con.curr_hash_num*9;

error_return:	
	return ret;
}
static int reset_hash_control(void){
	pro_info_t->p_hash_con.index=0;                                                                                                      
	pro_info_t->p_hash_con.curr_hash_num=0;                                                                                              
	pro_info_t->p_hash_con.is_finished=false;
}
/*reclaim_memory once finish current hash*/
static int reclaim_memory(void);

/*talk to userspace*/
#define CHEN_WALK 0xEF
#define CHECK _IOR(CHEN_WALK,0x1,unsigned int)//collect page information
#define HASH  _IOR(CHEN_WALK,0x3,unsigned int)//begin hash

struct proc_dir_entry *p_chen=NULL;
char entry_name[]="chen_walk";
static long chen_ioctl(struct file * filp,unsigned int ioctl,unsigned long arg){
	vm_info_t  vm_info;
	int ret=0;
	switch(ioctl){
		case CHECK:
			if(ret=copy_from_user(&vm_info,(void *)arg,sizeof(vm_info_t))){
				return ret;
			}
			ret = collect_phy_memory(&vm_info);
			vm_info.error_num |= ret;	
			ret=copy_to_user(arg,&vm_info,sizeof(vm_info_t));
			if(ret){
				printk("fail to update vm_info information\n");
			}
			break;
		case HASH:
			if(ret=copy_from_user(&vm_info,(void *)arg,sizeof(vm_info_t))){                                                              
				return ret;
			}
			hash_page(&vm_info);
			export_hashvalue(&vm_info);
			ret=copy_to_user(arg,&vm_info,sizeof(vm_info_t));
			if(ret){
				printk("fail to update vm_info information\n");
			}	
			//export_hashvalue(&vm_info);
			/*after second time,reclaim memory*/
			if(pro_info_t->p_hash_con.is_finished){
				reclaim_memory();
			}else{
				reset_hash_control();
			}	
			break;
		default:
			printk("unknown command!\n");
			break;
	}
	return 0;
}


static long chen_compat_ioctl(struct file * filp,unsigned int ioctl,unsigned long arg)
{
	return	chen_ioctl(filp,ioctl,arg);
}
/*proc ioctl operations*/
static struct file_operations ad_fops={
	.unlocked_ioctl=chen_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl=chen_compat_ioctl,
#endif
};
/*create proc file*/
struct proc_dir_entry* create_file_entry(void)
{
	p_chen=proc_create(entry_name,0,NULL,&ad_fops);
	return p_chen; 
}
/*delete proc file*/
void del_file_entry(void)
{
	if(p_chen)
		remove_proc_entry(entry_name,NULL);
}

static int  reclaim_memory(){
	int i=0;
	if(pro_info_t==NULL)
		goto end;
	if(pro_info_t->page_record==NULL)
		goto end;
	while(i<pro_info_t->page_record->first_index){
		if(pro_info_t->page_record->first_level[i]!=NULL){
			kfree(pro_info_t->page_record->first_level[i]);
			i++;
		}	
	}
	if(pro_info_t->page_record){
		kfree(pro_info_t->page_record);
		pro_info_t->page_record=NULL;
	}
	if(pro_info_t->page_hash){
		kfree(pro_info_t->page_hash);
		pro_info_t->page_hash=NULL;
	}
	if(pro_info_t->wvc){
		kfree(pro_info_t->wvc);
		pro_info_t->wvc=NULL;
	}
end:
	return 0;
}
static  __init int walk_init(void){
	init_ex_fun();
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


MODULE_LICENSE("GPL");
