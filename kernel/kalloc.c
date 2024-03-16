// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
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
  push_off();
  int cpu = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu = cpuid();

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;

  if (r == 0) {
    release(&kmem[cpu].lock);
    for (int i = 0; i < NCPU; i++) {
      if (i == cpu) continue;
      acquire(&kmem[i].lock);
      struct run *f = kmem[i].freelist;
      struct run *slow, *fast;
      if (f) {
        slow = f;
        fast = f->next;
        while (fast && fast->next) {
          slow = slow->next;
          fast = fast->next->next;
        }
        kmem[i].freelist = slow->next;
        slow->next = 0;
      }
      release(&kmem[i].lock);

      if (f) {
        acquire(&kmem[cpu].lock);
        // won't changed by other cpu so just set instead of append to freelist
        kmem[cpu].freelist = f;
        release(&kmem[cpu].lock);
        break;
      }
    }
    acquire(&kmem[cpu].lock);
  }

  if((r = kmem[cpu].freelist) != 0) {
    kmem[cpu].freelist = r->next;
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  release(&kmem[cpu].lock);

  pop_off();
  return (void*)r;
}
