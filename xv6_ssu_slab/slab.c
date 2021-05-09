#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "slab.h"

typedef int bool;
#define true 1
#define false 0

struct {
	struct spinlock lock;
	struct slab slab[NSLAB];
} stable;

bool get_bit(char* size, int i)
{
	char *temp = size;
	temp += (i / 8);
	i = i - (i / 8) * 8;
	unsigned char c = 0x80;
	if((*(temp + i) & (c >> i)) != 0){
		return true;
	}
	else{
		return false;
	}
}

char* set_bit(char* size, int i){
	char *temp = size;
	temp += (i / 8);
	i = i - (i / 8) * 8;
	unsigned char c = 0x80;
	*(temp + i) = (*(temp + i) | (c >> i));
	return temp;
}

char* clear_bit(char* size, int i){
	char *temp = size;
	temp += (i / 8);
	i = i - (i / 8) * 8;
	unsigned char c = 0x80;
	*(temp + i) = *(temp + i) & ~(c >> i);
	return temp;
}


void slabinit(){
	int slab_size = 8;
	acquire(&stable.lock);
	for(int i = 0; i <= slab_size; i++){
		stable.slab[i].size = slab_size << i;
		stable.slab[i].num_pages = 1;
		stable.slab[i].num_free_objects = 4096 / slab_size;
		stable.slab[i].num_used_objects = 0;
		stable.slab[i].num_objects_per_page = 4096 / slab_size;
		stable.slab[i].bitmap = kalloc();
		memset(stable.slab[i].bitmap, 0, 4096);

		stable.slab[i].page[0] = kalloc();
		memset(stable.slab[i].page[0], 0, 4096);
	}
	release(&stable.lock);
}

char *kmalloc(int size){
	if(size > 2048 || size <= 0){
		return 0;
	}

	int pgsize = 0x00000001;

	while(pgsize < size){
		pgsize = pgsize << 1;
	}

	struct slab *slab;


	acquire(&stable.lock);
	for(slab = stable.slab; slab < &stable.slab[NSLAB]; slab++){
		if(slab -> size == pgsize){
			if(slab -> num_free_objects > 0){
				char *objects = 0x0;
				if((slab -> num_used_objects) < (slab -> num_objects_per_page)){
					for(int i = 0; i < slab -> num_objects_per_page; i++){
						if(get_bit((slab -> bitmap), i)==0){
							slab -> num_free_objects--;
							slab -> num_used_objects++;
							slab -> bitmap = set_bit(slab -> bitmap, i);
							objects = slab -> page[slab -> num_pages - 1] + (i * slab -> size);
							break;
						}
					}
				}
				else{
					int idx = -1;
					for(int i = 0; i < (slab -> num_objects_per_page) * (slab -> num_pages); i++){
					       if(get_bit((slab -> bitmap),i)==0){
						       slab -> bitmap = set_bit((slab -> bitmap),i);
						       idx = i;
						       break;
					       }
					}
					if(idx != -1){
						while(1){
							if(idx >= 0 && idx < (slab -> num_objects_per_page))
								break;
							idx -= slab -> num_objects_per_page;
						}
					}
					else{
						cprintf("ERROR\n");
					       	release(&stable.lock);
						return 0;
					}
					slab -> num_free_objects--;
					slab -> num_used_objects++;
					objects = slab -> page[slab -> num_pages - 1] + (idx * slab -> size);
				}
				release(&stable.lock);
				return objects;
			}
			else{
			if(slab -> num_pages + 1 >= MAX_PAGES_PER_SLAB) {return 0;}
			slab -> num_pages++;
			slab -> page[slab -> num_pages - 1] = kalloc();
			slab -> num_free_objects += slab -> num_objects_per_page;
			release(&stable.lock);
			char *objects = kmalloc(size);
			return objects;
			}
		}
	}
	release(&stable.lock);
	return 0;	
}

void kmfree(char *addr){
	struct slab *slab;
	if(size > 2048 || size <= 0){
		acquire(&stable.lock);
		for(slab = stable.slab; slab < &stable.slab[NSLAB]; slab++){
			int count = 0;
			for(int i = 0; i < (slab -> num_pages); i++){
				for(int j = 0; j < (slab -> num_objects_per_page); j++){
					if(addr == slab -> page[i] + (j * slab -> size)){
						slab -> num_free_objects++;
						slab -> num_used_objects--;
						clear_bit((slab -> bitmap), i *  slab -> num_objects_per_page + j);
						if((slab -> num_used_objects) < ((slab -> num_pages - 1) + count) * slab -> num_objects_per_page){
							slab -> num_free_objects -= slab -> num_objects_per_page;
							kfree(slab -> page[slab -> num_pages - 1 + count]);
							count--;
						}
						release(&stable.lock);
						slab -> num_pages += count;
						return;
					}
				}
			}
			slab -> num_pages += count;
		}
		release(&stable.lock);
		return;
	}
						
}

void slabdump(){
	cprintf("__slabdump__\n");

	struct slab *s;

	cprintf("size\tnum_pages\tused_objects\tfree_objects\n");

	for(s = stable.slab; s < &stable.slab[NSLAB]; s++){
		cprintf("%d\t%d\t\t%d\t\t%d\n", 
			s->size, s->num_pages, s->num_used_objects, s->num_free_objects);
	}
}
