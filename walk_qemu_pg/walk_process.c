#include <linux/kernel.h>  
#include <linux/module.h>  
#include <linux/init.h>  
#include <linux/sched.h>  
#include <linux/list.h>
#include <linux/mm.h>
#include <asm/pgtable.h>

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

#define is_pmd_ram(x) (pmd_val(x) & _PAGE_PRESENT)
#define is_pte_ram(x) (pte_val(x) & _PAGE_PRESENT)
static int walk_page_table(struct task_struct *p){
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int count_pg=0,count_pu=0,count_pm=0,count_pt=0;
	if(p->mm==NULL)
		return -1;
	if(p->mm->pgd==NULL)
		return -1;
	pgd=p->mm->pgd;
	printk("page table base:%p\n",pgd);
	int count_4k=0;
	int count_2m=0;
	int count_1g=0;
	while(count_pg<511){
		if(!pgd_none(*pgd)){
			pud=(pud_t *)pgd_page_vaddr(*pgd);
			printk("pud:%p\n",pud);
			/*walk for one pgd*/
			while(count_pu<511){
			//	pud=(pud_t *)pgd_page_vaddr(*pgd_temp);
				if((pud!=NULL)&&(!pud_none(*pud))){
					pmd=(pmd_t *)pud_page_vaddr(*pud);
					printk("pmd:%p\n",pmd);
					/*walk for one pud*/
					while(count_pm<511){
					//	pmd=(pmd_t *)pud_page_vaddr(*pud_temp);
						if((pmd!=NULL)&&(!pmd_none(*pmd))){
							pte=(pte_t *)page_address(pmd_page(*(pmd)));
							printk("pmd:%p\n",pmd);
							/*walk for one pmd*/
							while(count_pt<511){
							 //	pte=(pte_t *)page_address(pmd_page(*(pmd)));
								if((pte!=NULL)&&(!pte_none(*pte))&&pte_present(*pte)&&is_pte_ram(*pte)){
									printk("pte:%p\n",pte);
									/*record 4K size page*/
									count_4k++;
								}
								pte++;
								count_pt++;
							}
						}
						if((huge_page_2m(*pmd)==1)&&(pmd_present(*pmd))&&is_pmd_ram(*pmd)){
							count_2m++;
							/*record 2M size page*/
						}
						pmd++;
						count_pm++;
					}
				}
				if(huge_page_1g(*pud)==1){
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
	printk("2m page count:%d\n",count_2m);
	printk("1g page count:%d\n",count_1g);
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
	printk("init waik process\n");
	return 0;
}
static  __exit void walk_exit(void){
	printk("exit exit process\n");
}
module_init(walk_init);
module_exit(walk_exit);



