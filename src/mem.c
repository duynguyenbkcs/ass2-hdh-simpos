
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page
	int index;	// Index of the page in the list of pages allocated
			// to the process.
	int next;	// The next page in the list. -1 if it is the last
			// page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t * get_page_table(
		addr_t index, 	// Segment level index
		struct seg_table_t * seg_table) { // first level table
	
	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;
	for (i = 0; i < seg_table->size; i++) {
		// Enter your code here
		
		if(seg_table->table[i].v_index == index) 
			return seg_table->table[i].pages;
	}
	return NULL;

}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr, 	// Given virtual address
		addr_t * physical_addr, // Physical address to be returned
		struct pcb_t * proc) {  // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL) {
		return 0;
	}
	
	int i;
	for (i = 0; i < page_table->size; i++) {
		if (page_table->table[i].v_index == second_lv) {
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			
			*physical_addr = (page_table->table[i].p_index << 10) + offset;
			return 1;
		}
	}
	return 0;	
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = (size % PAGE_SIZE == 0) ? size / PAGE_SIZE :
		size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	//todo
	int page_index[num_pages];
	if (proc->bp + size <= RAM_SIZE){		
		int i, count = 0;		
		for( i = 0; i < NUM_PAGES; i++){
		  if (_mem_stat[i].proc == 0) {page_index[count] = i; count++;}
		  if (count >= num_pages)     {mem_avail = 1; break;}
		} 	
	}	
	//end-todo

	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */
		if(ret_mem == PAGE_SIZE) proc->seg_table->size = 0;
		int j, k, find = -1;
		for( j = 0; j < num_pages; j++){
			_mem_stat[page_index[j]].proc = proc->pid;
			_mem_stat[page_index[j]].index = j;
			if(j == num_pages - 1) 
	   		      _mem_stat[page_index[j]].next = -1; 				
			else  _mem_stat[page_index[j]].next = page_index[j + 1];
			/* The first layer index */
			addr_t first_lv = get_first_lv(ret_mem + j * PAGE_SIZE);
			/* The second layer index */
			addr_t second_lv = get_second_lv(ret_mem + j * PAGE_SIZE);		
			
			for(k = 0; k < proc->seg_table->size; k++){
	           	    if(first_lv == proc->seg_table->table[k].v_index)
					{find = k; break;}			
			}
			if(find == -1){ 
				find = proc->seg_table->size;
				proc->seg_table->size++;
				proc->seg_table->table[find].pages = (struct page_table_t*)malloc(sizeof(struct page_table_t));
				proc->seg_table->table[find].pages->size = 0;	
				proc->seg_table->table[find].v_index = first_lv;
			}
			int count = proc->seg_table->table[find].pages->size;
			
			   proc->seg_table->table[find].pages->table[count].v_index = second_lv;
			   proc->seg_table->table[find].pages->table[count].p_index = page_index[j];		
			   count++;			
			proc->seg_table->table[find].pages->size = count;
			
		}
		
	}
	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */

	pthread_mutex_lock(&mem_lock);
	addr_t physical_addr;
	if (!translate(address, &physical_addr, proc)) {		
		return -1;
	}
	
	int find = physical_addr >> 10;
	int i, j, count = 0;	
	
	while(find != -1){
		_mem_stat[find].proc = 0;
		find = _mem_stat[find].next;
		count++;			
	}
	
	/* The first layer index */
	addr_t first_lv = get_first_lv(address);
	/* The second layer index */
	addr_t second_lv = get_second_lv(address);
		
	for(i=0; i < proc->seg_table->size; i++){
	       if(first_lv == proc->seg_table->table[i].v_index){
		int table_size = proc->seg_table->table[i].pages->size;
	       for(j = 0; j < table_size; j++){
		 if(proc->seg_table->table[i].pages->table[j].v_index == second_lv)
		 { memcpy((void*)&proc->seg_table->table[i].pages->table[j],
		(const void*)&proc->seg_table->table[i].pages->table[j+count],
		(void*)&proc->seg_table->table[i].pages->table[table_size] -
		(void*)&proc->seg_table->table[i].pages->table[j + count]);
		proc->seg_table->table[i].pages->size -= count; break; }
	       }break;
	       }
	}
	pthread_mutex_unlock(&mem_lock);
	return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		*data = _ram[physical_addr];
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		_ram[physical_addr] = data;
		return 0;
	}else{
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {
				
				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
					
			}
		}
	}
}


