// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

//get the reference count index
#define REFCIND(x) PGROUNDDOWN(x) / PGSIZE

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.
                   
                    
//array of reference counts for the page                    
int refc[REFCIND(PHYSTOP) + 1];

//protect ref table
struct spinlock ref_lock;

void kinit() {
  char *p = (char *)PGROUNDUP((uint64)end);
  bd_init(p, (void *)PHYSTOP);
  memset(refc, 0, PGROUNDDOWN(PHYSTOP) / PGSIZE * sizeof(int));
  initlock(&ref_lock, "ref_lock");
}


void add_refc(void *pa) {
  acquire(&ref_lock);
  refc[REFCIND((uint64)pa)]++;
  release(&ref_lock);
}

void dec_refc(void *pa) {
  int ref = refc[REFCIND((uint64)pa)];
  if (ref <= 0) {
    panic("dec_ref: 0 ref");
  }
  refc[REFCIND((uint64)pa)]--;
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  acquire(&ref_lock);
  dec_refc(pa);
  if (refc[REFCIND((uint64)pa)] <= 0) {
    bd_free(pa);
  }
  release(&ref_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) { 
  void* pa = bd_malloc(PGSIZE);
  add_refc(pa);
  return pa;
}
