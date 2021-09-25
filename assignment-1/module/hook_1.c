/*
Name : Nuka Siva Kumar          Roll No: 18111069
Discussed_With: 
         Prof. Deba      How mmap() will assign memory ?
         KP Arun         How can we used kernal linked_list ?
         Gourav          Discussion about mmap() memory allocation
         Supriya Suresh  To resolve Errors.
         Sunil           Discussion about how can we can count TLB_MISSES with pti enabled.
         
         Others          Some raw conversation about Assignment.
*/

#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/mm.h>
#include<linux/mm_types.h>
#include<linux/file.h>
#include<linux/fs.h>
#include<linux/path.h>
#include<linux/slab.h>
#include<linux/dcache.h>
#include<linux/sched.h>
#include<linux/uaccess.h>
#include<linux/fs_struct.h>
#include <asm/tlbflush.h>
#include<linux/uaccess.h>
#include<linux/device.h>
#include "mem_tracker.h"
#include "interface.h"

static int command;
static unsigned long tlb_misses, readwss, writewss, unused;
//created by me
struct read_command *cmd;
static unsigned long gptr;
static pte_t *gpte;
unsigned long int num_pages, vma_start, vma_end; 
struct vma_area_list{				// Node will store mmap start , end address
	unsigned long start;
	unsigned long end;
	struct vma_area_list *next;
};
struct vma_area_list *head = NULL;
int *tlb_misses_list;				// keep track tlb_misses for every page
int *unused_misses_list;			// Keep track unused pages
int *read_misses_list;				// keep track read only misses
int *write_misses_list;				// keep track write misses 

static ssize_t memtrack_command_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", command);
}

static ssize_t memtrack_command_set(struct kobject *kobj,
                                   struct kobj_attribute *attr,
                                   const char *buf, size_t count)
{
        /*TODO    Part of assignment, needed to be implemented by you*/
	sscanf(buf,"%d",&command);
        return count;
}

static struct kobj_attribute memtrack_command_attribute = __ATTR(command,0644,memtrack_command_show, memtrack_command_set);

static ssize_t memtrack_tlb_misses_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%lu\n", tlb_misses);
}
static struct kobj_attribute memtrack_tlb_misses_attribute = __ATTR(tlb_misses, 0444,memtrack_tlb_misses_show, NULL);

static ssize_t memtrack_readwss_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%lu\n", readwss);
}
static struct kobj_attribute memtrack_readwss_attribute = __ATTR(readwss, 0444,memtrack_readwss_show, NULL);

static ssize_t memtrack_writewss_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%lu\n", writewss);
}
static struct kobj_attribute memtrack_writewss_attribute = __ATTR(writewss, 0444,memtrack_writewss_show, NULL);


static ssize_t memtrack_unused_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%lu\n", unused);
}
static struct kobj_attribute memtrack_unused_attribute = __ATTR(unused, 0444,memtrack_unused_show, NULL);
static struct attribute *memtrack_attrs[] = {
        &memtrack_command_attribute.attr,
        &memtrack_tlb_misses_attribute.attr,
        &memtrack_readwss_attribute.attr,
        &memtrack_writewss_attribute.attr,
        &memtrack_unused_attribute.attr,
        NULL,
};
struct attribute_group memtrack_attr_group = {
        .attrs = memtrack_attrs,
        .name = "memtrack",
};


//	get_pte() function will give pte entry for address

static pte_t* get_pte(unsigned long address, unsigned long *addr_vma)
{
        pgd_t *pgd;
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *ptep;
        struct mm_struct *mm = current->mm;
        struct vm_area_struct *vma = find_vma(mm, address);
        if(!vma){
                 printk(KERN_INFO "No vma yet\n");
                 goto nul_ret;
        }

        *addr_vma = (unsigned long) vma;

        pgd = pgd_offset(mm, address);
        if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
                goto nul_ret;
        //printk(KERN_INFO "pgd(va) [%lx] pgd (pa) [%lx] *pgd [%lx]\n", (unsigned long)pgd, __pa(pgd), pgd->pgd);
        p4d = p4d_offset(pgd, address);
        if (p4d_none(*p4d))
                goto nul_ret;
        if (unlikely(p4d_bad(*p4d)))
                goto nul_ret; 
        pud = pud_offset(p4d, address);
        if (pud_none(*pud))
                goto nul_ret;
        if (unlikely(pud_bad(*pud)))
                goto nul_ret;
        //printk(KERN_INFO "pud(va) [%lx] pud (pa) [%lx] *pud [%lx]\n", (unsigned long)pud, __pa(pud), pud->pud);

        pmd = pmd_offset(pud, address);
        if (pmd_none(*pmd))
                goto nul_ret;
        if (unlikely(pmd_trans_huge(*pmd))){
                printk(KERN_INFO "I am huge\n");
                goto nul_ret;
        }
        //printk(KERN_INFO "pmd(va) [%lx] pmd (pa) [%lx] *pmd [%lx]\n", (unsigned long)pmd, __pa(pmd), pmd->pmd);
        ptep = pte_offset_map(pmd, address);
        if(!ptep){
                printk(KERN_INFO "pte_p is null\n\n");
                goto nul_ret;
        }
        //printk(KERN_INFO "pte(va) [%lx] pte (pa) [%lx] *pte [%lx]\n", (unsigned long)ptep, __pa(ptep), ptep->pte);
        return ptep;

        nul_ret:
               printk(KERN_INFO "Address could not be translated\n");
               return NULL;
}

//  find_page_index() will findout page_no with respect user mmap area.
int find_page_index ( unsigned long address){
        return (address >> 12) - (gptr >> 12);
}


/*
fault_hook() fuction will resolve RSVD_FAULT and
 it enables us to count TLB_MISSES readonly miss
-es for every page and write misses for every page
and counting unused user pages.
*/
static int fault_hook(struct mm_struct *mm, struct pt_regs *regs, unsigned long error_code, unsigned long address)
{
   /*TODO Fault handler*/
    if(command == 0)
    {
        if( gptr <= address && vma_end > address){		
                unsigned long *ptr;
		unsigned long int vmaa;
                tlb_misses += 1;
		tlb_misses_list[find_page_index(address)] += 1;
		stac();
		gpte = get_pte(address, &vmaa);        
                gpte->pte &= ~(0x1UL << 50);			// resolve RSVD_FAULT
                ptr  = (unsigned long *)address;
                *ptr = 100;
		clac();
                //printk(KERN_INFO "Inside fault_hook function  value is %lu address %lx and error_code %lx ",tlb_misses, address, error_code);
                gpte->pte |= 0x1UL << 50;

                return 0;
        }
        return -1;
     
    }
    else if (command == 1)
    {
     
    }
    else if (command == 2)
    {
	if( gptr <= address && vma_end > address){
                unsigned long *ptr;
                unsigned long int vmaa;
		 /*
                Counting unused pages and readonly misses for every page
                and writemisses for everypage
                */                
		tlb_misses += 1;
                unused_misses_list[find_page_index(address)] += 1;
		if( error_code & 0x2 ){
                        if (write_misses_list[find_page_index(address)] == 0){
                        	writewss += 1;
				if(read_misses_list[find_page_index(address)] != 0)
					readwss -= 1; 
			}
                        read_misses_list[find_page_index(address)] = 0;
                        write_misses_list[find_page_index(address)] += 1;
                }
 		else{
			 if ( write_misses_list[find_page_index(address)] == 0 ) {
                        	if (read_misses_list[find_page_index(address)] == 0)
                                	readwss += 1; 
                         	read_misses_list[ find_page_index(address)] += 1;
		    	}	
                 }
                //printk(KERN_INFO " %ld %ld %ld",writewss, readwss, tlb_misses); 	
		stac();
                gpte = get_pte(address, &vmaa);
                gpte->pte &= ~(0x1UL << 50);
                ptr  = (unsigned long *)address;
                *ptr = 100;
		clac();
                //printk(KERN_INFO "command 2 hook function  value is %lu address %lx and error_code %lx ",tlb_misses, address, error_code);
                gpte->pte |= 0x1UL << 50;

                return 0;
        }
        return -1;
    }
    return 0;
}

/* 
This function will add vma_area_list node to end of linked
-list to preserve mmap area state before user mmap has done
*/
void add_to_end(struct vma_area_list *newData){
    struct vma_area_list *temp1;
    if (head == NULL){
        head = (struct vma_area_list *)kmalloc(sizeof(struct vma_area_list),GFP_KERNEL);
        head = newData;
    } else {
        temp1 = (struct vma_area_list *)kmalloc(sizeof(struct vma_area_list),GFP_KERNEL);
        temp1 = head;
        while(temp1->next!=NULL) {
            temp1 = temp1->next;
        }
        temp1->next = newData;
    }
}

//find_vma_area() function will correctly set the vma_end
void find_vma_area(void)
{
	struct vma_area_list *temp;
        temp = (struct vma_area_list *)kmalloc(sizeof(struct vma_area_list),GFP_KERNEL);
        temp = head;
        while(temp->next!=NULL) {
	    if( vma_end == temp->end){
		vma_end = temp->start;
		break;
	    }
            temp = temp->next;
        }
}
// select_top 5 from misses_list ( we can pass tlb, read, write misses list )
void select_top(int *misses_list){
        int i=0, max_0, max_1, max_2, max_3, max_4;
	max_0 = max_1 = max_2 = max_3 = max_4 = 0, i=0;
        cmd->valid_entries = 0;
	for( ; i < num_pages ; i++)
	{
		if (misses_list[i] == 0)
			continue;
                else{
			if( misses_list[i] >= misses_list[max_0] ){
					max_4 = max_3;
					max_3 = max_2;
					max_2 = max_1;
					max_1 = max_0;
					max_0 = i;
			}	
	        	else if ( misses_list[i] >= misses_list[max_1] ){
					max_4 = max_3;
                                        max_3 = max_2;
                                        max_2 = max_1;
                                        max_1 = i;
			}
			else if ( misses_list[i] >= misses_list[max_2] ){
					max_4 = max_3;
                                        max_3 = max_2;
                                        max_2 = i;
			}
			else if ( misses_list[i] >= misses_list[max_3]){
					max_4 = max_3;
					max_3 = i;
			}
			else if ( misses_list[i] > misses_list[max_4]){
					max_4 = i;
			}			
		}
	}		
	//printk(KERN_INFO " %d - %d - %d - %d - %d - ",max_0, max_1, max_2, max_3, max_4);
		if ( misses_list[max_0] !=0 ){
			cmd->valid_entries += 1;
			cmd->toppers[0].vaddr = gptr + max_0 * 4096;
			cmd->toppers[0].count = misses_list[max_0];
		}
		if ( misses_list[max_1] !=0 && max_1 != max_0 ){
                        cmd->valid_entries += 1;
                        cmd->toppers[1].vaddr = gptr + max_1 * 4096;
                        cmd->toppers[1].count = misses_list[max_1];
                }
		if ( misses_list[max_2] !=0 && max_2 != max_1 ){
                        cmd->valid_entries += 1;
                        cmd->toppers[2].vaddr = gptr + max_2 * 4096;
                        cmd->toppers[2].count = misses_list[max_2];
                }
		if ( misses_list[max_3] !=0 && max_3 != max_2){
                        cmd->valid_entries += 1;
                        cmd->toppers[3].vaddr = gptr + max_3 * 4096;
                        cmd->toppers[3].count = misses_list[max_3];
                }
		if ( misses_list[max_4] !=0 && max_4 != max_3){
                        cmd->valid_entries += 1;
                        cmd->toppers[4].vaddr = gptr + max_4 * 4096;
                        cmd->toppers[4].count = misses_list[max_4];
                }
}
ssize_t handle_read(char *buff, size_t length)
{
   /*TODO Read handler*/
   //printk(KERN_INFO "  %d ", command);
   cmd = (struct read_command*)buff;
   if(command == 0)
   {
        if (cmd->command == FAULT_START)
        {
                unsigned long iterator;
                struct mm_struct *mm = current->mm;
 	        struct vm_area_struct *vma = find_vma(mm, gptr);
		tlb_misses = 0;

		vma_start = vma->vm_start;
		vma_end = vma->vm_end;
		find_vma_area();
		//printk(KERN_INFO " start [%lx] end [%lx] ", vma_start, vma_end);
		//printk(KERN_INFO " start [%lx] end unused_misses_list[%lx] ", gptr, vma_end);
				
		num_pages = vma_end - gptr;
		num_pages >>= 12;
		tlb_misses_list = (int *) kmalloc(sizeof(int) * num_pages, GFP_KERNEL);   //counting tlb_misses_for each page

		// poissioning all pages and setting tlb_misses_list values to 0
		for (iterator = gptr ; iterator  < vma_end ; iterator += 4096)
		{	unsigned long vmaa;
			gpte = get_pte(iterator, &vmaa);
                	gpte->pte |= 0x1UL << 50;
			tlb_misses_list[ find_page_index(iterator) ] = 0;
			//printk(KERN_INFO "[%lx] -- %d --%d -- [%lx] ",iterator, find_page_index(iterator), tlb_misses_list[find_page_index(iterator)], vma_end );
		} 
        }
        else if( cmd->command == TLBMISS_TOPPERS ){
               select_top(tlb_misses_list);
	}
	else{

	}
   }
   else if (command == 1)
   {

   }
  else if(command == 2)
   {
	if(cmd->command == FAULT_START)
	{
		unsigned long iterator;
                struct mm_struct *mm = current->mm;
                struct vm_area_struct *vma = find_vma(mm, gptr);
                
		tlb_misses = 0;
		readwss = 0;
		writewss = 0;
		unused = 0;
                
		vma_start = vma->vm_start;
                vma_end = vma->vm_end;
                find_vma_area();
                //printk(KERN_INFO " start [%lx] end [%lx] ", vma_start, vma_end);
                //printk(KERN_INFO " start [%lx] end [%lx] ", gptr, vma_end);
		/*
		It allocation memory for unused, read, write misses list and 
		setting these values appropriately. and creating RSVD_FAULTs 
		*/
                num_pages = vma_end - gptr;
                num_pages >>= 12;
                unused_misses_list = (int *) kmalloc(sizeof(int) * num_pages, GFP_KERNEL);   //counting tlb_misses_for each page
                read_misses_list = (int *) kmalloc(sizeof(int) * num_pages, GFP_KERNEL);
                write_misses_list = (int *) kmalloc(sizeof(int) * num_pages, GFP_KERNEL);
                if (read_misses_list == NULL || write_misses_list==NULL || unused_misses_list == NULL)
                        printk(KERN_INFO "not defined in memory");
		//printk(KERN_INFO " %ld - %ld",num_pages, gptr+4096 - gptr);
                for (iterator = gptr ; iterator  < vma_end ; iterator += 4096)
                {       unsigned long vmaa;
                        gpte = get_pte(iterator, &vmaa);
                        gpte->pte |= 0x1UL << 50;
                        unused_misses_list[ find_page_index(iterator) ] = -1;
                        read_misses_list[find_page_index(iterator)] = 0;
                        write_misses_list[find_page_index(iterator)] = 0;
                        //printk(KERN_INFO "[%lx] -- %d --%d -- [%lx] ",iterator, find_page_index(iterator), tlb_misses_list[find_page_index(iterator)], vma_end );
                }
	}
	else if(cmd->command == READ_TOPPERS){
		select_top(read_misses_list);
	}	
	else if(cmd->command == WRITE_TOPPERS){
		select_top(write_misses_list);
	}
	else{
	}
   }
   else
   {

   }
   return 0;
}


ssize_t handle_write(const char *buff, size_t lenth)
{
   /*TODO Write handler*/
   stac();
   gptr = *((unsigned long *)buff);
   clac();
   return 8;

   return 0;
}

/*
In this function we are capturing previous mmap state
and storing in a linked_list (not KERNAL Linked_list).
*/
int handle_open(void)
{
   /*TODO open handler*/
	
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vm_area;
    vm_area = mm->mmap;
    while(vm_area){
	struct vma_area_list *temp;
  	//printk(KERN_INFO " [%lx] - [%lx] ", vm_area->vm_start, vm_area->vm_end );
		temp = (struct vma_area_list *)kmalloc(sizeof(struct vma_area_list),GFP_KERNEL);
		temp->start = vm_area->vm_start;
		temp->end = vm_area->vm_end;
		temp->next = NULL;
		add_to_end(temp);
	vm_area = vm_area->vm_next;
    }
//   page_fault_pid = current->pid;
   rsvd_fault_hook = &fault_hook;
   return 0;
}

int handle_close(void)
{
   /*TODO open handler*/
   unsigned long iterator;
   struct vma_area_list *temp;
   // set RSVD_BIT correctly for every page
   for (iterator = gptr ; iterator  < vma_end ; iterator += 4096)
   {	   
	   unsigned long vmaa;
           //int page_no = find_page_index(iterator);
           gpte = get_pte(iterator, &vmaa);
           gpte->pte &= ~(0x1UL << 50);
	  //printk(KERN_INFO " [%lx] --[%d]-- [%d]--[%d]--[%d] " , iterator, page_no, tlb_misses_list[ page_no], read_misses_list[page_no], write_misses_list[page_no]);
           if (command == 2 ){
	   	//printk(KERN_INFO " %d - %d ",read_misses_list[page_no], write_misses_list[page_no]);
		if ( unused_misses_list[ find_page_index(iterator) ] == -1 )
		unused += 1;
           }
   }

   // Free the linked list
   temp = (struct vma_area_list *)kmalloc(sizeof(struct vma_area_list),GFP_KERNEL);
   while(head){
	temp = head;
	head = head->next;
	kfree(temp);
   }
   // Free the misses counting lists
   if (command == 0)	
  	 kfree(tlb_misses_list);
   if (command == 2){
	kfree(unused_misses_list);
	kfree(read_misses_list);
	kfree(write_misses_list);
   }
   printk(KERN_INFO " tlb_misses %lu - readwss %lu - writewss %lu - unused %lu " , tlb_misses, readwss, writewss, unused ); 
   page_fault_pid = -1;
   rsvd_fault_hook = NULL;
   printk(KERN_INFO " Now Device Closed SUccessfully ");
   return 0;
}
