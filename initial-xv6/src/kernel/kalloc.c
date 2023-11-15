// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define MAXPAGES (PGROUNDUP(PHYSTOP)/PGSIZE)

int usage_array[MAXPAGES];
struct spinlock usage_lock;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void increase_usage(uint64 page_num){
    acquire(&usage_lock);
    if (!usage_array[page_num] || usage_array[page_num]>0){
        usage_array[page_num]++;
    }
    release(&usage_lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");

  initlock(&usage_lock, "usage_array");
  acquire(&usage_lock);
  for (int i = MAXPAGES - 1; i > -1; i--) {
    usage_array[i] = 1;
  }
  release(&usage_lock);

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

    uint64 usage_index = (uint64)pa/PGSIZE;
    acquire(&usage_lock);
    if(usage_array[usage_index] == 1){
        usage_array[usage_index] = 0;
        release(&usage_lock);
    }
    else if(usage_array[usage_index] > 1){
        usage_array[usage_index]--;
        release(&usage_lock);
        return;
    }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
      memset((char *) r, 5, PGSIZE); // fill with junk
      increase_usage((uint64)r/PGSIZE);
  }

  return (void*)r;
}
