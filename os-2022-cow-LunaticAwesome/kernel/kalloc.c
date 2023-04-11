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

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.

struct {
  struct spinlock lock;
  uint16 ref[(PHYSTOP - KERNBASE) / PGSIZE];
} page_ref;

uint64 get_index(uint64 pa) { return (PGROUNDDOWN(pa - KERNBASE)) / PGSIZE; }

void inc_ref(uint64 pa) {
  acquire(&page_ref.lock);
  ++page_ref.ref[get_index(pa)];
  release(&page_ref.lock);
}

uint16 get_ref(uint64 pa) { return page_ref.ref[get_index(pa)]; }

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void ref_assign(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  acquire(&page_ref.lock);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    page_ref.ref[get_index((uint64)p)] = 1;
  }
  release(&page_ref.lock);
}

void kinit() {
  initlock(&kmem.lock, "kmem");
  initlock(&page_ref.lock, "page_ref");
  ref_assign(end, (void *)PHYSTOP);
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  struct run *r;
  acquire(&page_ref.lock);
  if (--page_ref.ref[get_index((uint64)pa)] == 0) {
    release(&page_ref.lock);
    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
      panic("kfree");
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else {
    release(&page_ref.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) {
    kmem.freelist = r->next;
    inc_ref((uint64)r);
  }
  release(&kmem.lock);

  if (r) memset((char *)r, 5, PGSIZE);  // fill with junk
  return (void *)r;
}
