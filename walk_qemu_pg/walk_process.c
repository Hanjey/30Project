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
static int walk_page_table(struct task_struct *p){
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
									if(is_pte_ram(*pte))
										count_4k_ram++;
									printk("pte:%lu\n",pte_val(*pte));
									/*record 4K size page*/
									count_4k++;
								}
								pte++;
								count_pt++;
							}
						}
						/*if pmd point to a 2m page and is in ram*/
						if((!pmd_none(*pmd)&&pmd_large(*pmd))){
							if(is_pmd_ram(*pmd))
									count_2m_ram++;
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
/*get annon page virtual address by page*/
static unsigned int trans_page_vadd_anno(struct page *page){
	struct anon_vma *anon_vma;
	pgoff_t pgoff;
	struct anon_vma_chain *avc;
	unsigned long address;
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
	unsigned long address;
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


static int mainfun(void){
	struct task_struct *tpro=0;
	getTargetProcess(&tpro,795);
	if(tpro==NULL){
		printk("find no process!\n");
		return -1;
	}
	printk("target process id:%d  pname:%s\n",tpro->pid,tpro->comm);
	walk_page_table(tpro);
	return 0;
}
static  __init int walk_init(void){
	mainfun();
	printk("init walk process\n");
	return 0;
}
static  __exit void walk_exit(void){
	printk("exit exit process\n");
}
module_init(walk_init);
module_exit(walk_exit);



