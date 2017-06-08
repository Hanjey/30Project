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

/*---define globle data zone*/ 
typedef struct _pfn_recoard{
	unsigned long pfn_m[500];
	unsigned long index;
}pfn_recoard;
/*---define pfn data base*/
static pfn_recoard pfn_rec;

/*info  block communicate with userspace*/
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
/*struct record all kinds of page*/
#define INDEX_PAGE_IN_RAM4K   0
#define INDEX_PAGE_IN_CACHE4K 1
#define INDEX_PAGE_IN_RAM2M   2
#define INDEX_PAGE_IN_CACHE2M 3
#define INDEX_COUNT 	      4
#define PAGE_CACHE_COUNT 2048
typedef struct _page_recoard{
	struct page *page_in_ram[300];
	unsigned int index;
}PageRecord;
/*---------*/
typedef struct _page_vaddr_record{
	unsigned long pvaddr[500];
	int index;
}PageVaddrSet;
/*--------structure store PageCache temporary*/
typedef struct _page_cache{
	struct page *page_cache[PAGE_CACHE_COUNT];
	unsigned int index;
}PageCache;
/*point to memory storing hash value*/
char *page_hash;
PageCache *page_cache;
/*toatl page num*/
int page_num;
PageRecord *page_record;
//PageVaddrSet *page_vset;
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
static int tatal_page;
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
/*static int get physics address by virtual address*/
static int  *trans_vaddr_to_paddr(struct vm_area_struct *vma,unsigned long address){
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
	spinlock_t *ptl;
	struct page *page;
	struct mm_struct *mm;
	mm=vma->vm_mm;
	if(mm==NULL)return -1;
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
		printk("vma is huge\n");
		page=follow_huge_pmd(mm,address,pmd,0);
		/*handle huge page*/
		if(page!=NULL)
			tatal_page+=512;
		return 512;
	}
	if(pmd_trans_huge(*pmd)){
		page = pte_page(*(pte_t *)pmd);
		unsigned long pfn_temp=page_to_pfn(page);
		if(page!=NULL){ 
			tatal_page+=512;    
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
		if(check_page_cache(ptep))
			pagenum_in_cache++;
	}else{
		page=pte_page(pte);
		//page = vm_normal_page(vma, address, pte);
		if(page!=NULL)tatal_page++;
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
static int get_mapping_info_by_vma(struct vm_area_struct *vma_temp){
	unsigned long temp_address;
	int temp_num;
	temp_address=vma_temp->vm_start;
	while(temp_address<vma_temp->vm_end){
		/*temp_num indicate a range*/
		temp_num = trans_vaddr_to_paddr(vma_temp,temp_address);
		if(temp_num==-1)break;
		temp_address+=temp_num<<PAGE_SHIFT;
	}
	return 0;
}
/*get all mapping information by a process*/
static int tatal_vma;
static int walk_vma_list_for_process(struct task_struct *task_temp){
	struct list_head *pos;
	struct vm_area_struct *vma_temp;
	vma_temp=task_temp->mm->mmap;
	if(vma_temp==NULL)return -1;
	do{
		//printk("vma range:start:%lx end:%lx\n",vma_temp->vm_start,vma_temp->vm_end);
		tatal_vma+=(vma_temp->vm_end-vma_temp->vm_start)>>PAGE_SHIFT;
		get_mapping_info_by_vma(vma_temp);
		vma_temp=vma_temp->vm_next;
		cond_resched();
	}while(vma_temp!=NULL&&vma_temp!=task_temp->mm->mmap);	
	return 0;
}

/* PageAnon: wheather a page maps a file or just a annon page
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
static int get_page_hash_value(PageRecord *p_record){
	int j=0;
	int i=0;
	struct page *temp_page;
	u32 hash_temp;
	char str[9];
	page_num=0;
	if(page_num<=0)
		return -1;
	/*in fact it is a 2 ventor array*/
	page_hash=(char *)kmalloc(page_num*9,GFP_KERNEL);
	memset(page_hash,0,page_num*9);	
	for(j=0;j<page_record->index;j++){
		temp_page=page_record->page_in_ram[j];
		printk("page_count:%d\n",temp_page->_mapcount);
		hash_temp=calc_check_num(temp_page);
	//	printk("hash value:%08x\n",hash_temp);
		sprintf(page_hash+i,"%08x",hash_temp);
	//	printk("%s\n",(char *)(page_hash+i));
		i+=9;
	}
	return 0;
}
/*view_hash_value*/
static int view_hash_value(void){
	int i=0;
	if(page_hash==NULL){
		printk("page hash is NULL\n");
		return -1;
	}
	while(i<page_num){
		printk("%08x\n",page_hash[i]);
		i++;
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
static int check_page_cache(pte_t *pte){
	if(pte_file(*pte))return 0;
	struct page *found_page=NULL;
	swp_entry_t entry;
	entry=pte_to_swp_entry(*pte);
	found_page = lookup_swap_cache_chen(entry);
	printk("in check_page_cache------\n");
	if(found_page!=NULL){
		tatal_page++;
		return 1;
	}
	return 0;
}
/*get record page vaddr*/
static int trans_page_to_vadd(PageRecord *page_record,PageVaddrSet *page_vset){
	return 0;
}
/*page_to_pfn transfer a page structure to pfn
 *pte_page    transfer a pte to page structure
 * */
/*init page_record function*/
static int init_page_record(PageRecord *page_record){
	memset(page_record,0,sizeof(PageRecord));
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

/*walk radix tree*/
struct page *trans_slot_to_page(void **pagep){
	struct page *page_temp;
	page_temp = radix_tree_deref_slot(pagep);
	return page_temp;
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

int walk_radix_tree(struct radix_tree_root *root,PageCache *page_cache){
//	unsigned long page_cache[100];
	//int i=0,j=0;
	int res=0;
	void **results;
	unsigned int max_items;
	/*if(PAGE_CACHE_COUNT-page_cache->index<100){
		printk("page cache memory is not enough\n");
		return -1;
	}*/
//	results=(page_cache->page_cache+page_cache->index);
	results=(page_cache->page_cache);
	max_items=maxindex(root->height);
	//printk("max_items:%d\n",max_items);
	res=radix_tree_gang_lookup(root,results,0,max_items);
	page_cache->index+=res;
//	printk("total %d page\n",res);
	return res;
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

int get_page_cache_by_process(struct task_struct *task_temp,PageCache *page_cache){
	struct files_struct *file_set;
	struct fdtable *fdt;
    struct file *temp_file;
	struct file **file_fd;
	struct address_space *address_temp;
	int i=0;
	int total_num;
	file_set=task_temp->files;
	if(file_set==NULL){
		return 0;
	}
	file_fd=file_set->fd_array;
	fdt=file_set->fdt;
	/*walk fd arrary*/
	while(i<fdt->max_fds){
		if(fd_is_open(i,fdt)){
			temp_file=file_fd[i];
			if(temp_file->f_mapping!=NULL){
				address_temp=(temp_file->f_mapping);
				total_num+=walk_radix_tree(&address_temp->page_tree,page_cache);
			}else{
				printk("%d fd temp_file->f_mapping is NULL\n",i);
			}
		}
		i++;
	}
//	printk("total cache:%d\n",total_num);
	return 0;
}
/*view page cache*/
void view_page_cache(void){
	int i,j=0,k=0;
	struct page *temp_page;
	int a;
	for(i=0;i<page_cache->index;i++){
		temp_page=page_cache->page_cache[i];
		a=atomic_read(&temp_page->_mapcount);
		if(a>=0)
			j++;
		else
			k++;
	}
	printk("there are %d page in cache\n",page_cache->index);
	printk("there are %d page mapped in page table\n",j);
	printk("there are %d page not  mapping\n",k);
}
/*get page in memory but not in pagetable*/
int get_page_not_maped(PageRecord *page_record){
	return 0;
}
/*main function to execute*/
static int mainfun(vm_info_t *vm_info){
	struct task_struct *tpro=0;
	int pid=vm_info->pid;
	int i;
	unsigned long buffer_add=vm_info->mb.buffer_add;
	int flags=vm_info->flags;
	if(flags==1)goto hash_begin;
	page_record=kmalloc(sizeof(PageRecord),GFP_KERNEL);
//	page_vset=kmalloc(sizeof(PageVaddrSet),GFP_KERNEL);
//	page_cache=kmalloc(sizeof(PageCache),GFP_KERNEL);
//	page_cache->index=0;
	//printk("PageRecord size :%d\n",sizeof(PageRecord));
	init_page_record(page_record);
	getTargetProcess(&tpro,pid);
	if(tpro==NULL){
		printk("find no process!\n");
		return -1;
	}
	printk("target process id:%d  pname:%s\n",tpro->pid,tpro->comm);
	walk_vma_list_for_process(tpro);
	printk("all mapping page:%d tatal memory:%d KB\n",tatal_vma,tatal_vma*4);
	printk("page in ram:%d\n memory in ram %d KB\n",tatal_page,tatal_page*4);
	printk("page in cache or in file:%d\n",pagenum_in_cache);
	//walk_page_table(tpro,page_record);
	//get_page_cache_by_process(tpro,page_cache);
//	view_page_cache();
	/*hash every page and write to buffer_add*/
   // get_page_hash_value(page_record);
	/*getProcessThreads(tpro);
	for(i=0;i<pthreads.index;i++){
		get_page_cache_by_process(pthreads.pthreads_p[i],page_cache);	
	}
	printk("there are %d page in cache\n",page_cache->index);*/
	//view_page_cache();
    //view_hash_value();
hash_begin:
	//trans_page_to_vadd(page_record,page_vset);
//	view_page_record(page_record);
	//view_page_vadd(page_vset);
	return 0;
}

/*export page content to file in userspace**/
static int export_page_to_file(struct file *file,unsigned long page_vadd){
	
	return 0;
}
/*reclaim_memory once finish current hash*/
void reclaim_memory(void);

/*talk to userspace*/
#define CHEN_WALK 0xEF
#define CHECK _IOR(CHEN_WALK,0x1,unsigned int)

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
			mainfun(&vm_info);
			/*printk("pid get from userspace:%d\n",vm_info.pid);
			printk("buffer_add get from userspace:%p\n",vm_info.mb.buffer_add);
			ret=copy_to_user((void *)vm_info.mb.buffer_add,(void *)page_hash,page_num*9);
			printk("page_num:%d\n",page_num);
			vm_info.mb.buffer_size=page_num*9;
			if(ret){
				printk("fail to write info to userspace\n");
			}
			ret=copy_to_user(arg,&vm_info,sizeof(vm_info_t));
			if(ret){
				printk("fail to update vm_info information\n");
			}*/
			reclaim_memory();
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
static struct file_operations ad_fops={
	.unlocked_ioctl=chen_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl=chen_compat_ioctl,
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
	tatal_page=0;
	tatal_vma=0;
	page_num=0;
	pthreads.index=0;
	if(page_record){
		kfree(page_record);
		page_record=NULL;
	}
	if(page_hash){
		kfree(page_hash);
		page_hash=NULL;
	}
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
