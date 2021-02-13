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

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}
static uint64 pa_vec_start;
static uint64 pa_vec_num;
static uint64 pa_vec_end;

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  pa_vec_start = (uint64)pa_start;
  pa_vec_num = (pa_end - pa_start) / PGSIZE + 1;
  pa_vec_end = PGROUNDUP(pa_vec_start + pa_vec_num * sizeof(uint64));
  memset((void *)pa_vec_start, 0, pa_vec_end - pa_vec_start);
  p = (char *)PGROUNDUP((uint64)pa_vec_end);
  kmem.freelist = 0;
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < (char *)pa_vec_end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 idx = pa_vec_start + (((uint64)pa - pa_vec_end) / PGSIZE) * sizeof(uint64);
  if (*(uint64 *)idx > 1)
  {
    *(uint64 *)idx = *(uint64 *)idx - 1;
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  r = (struct run *)pa;
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
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
  {
    memset((char *)r, 5, PGSIZE); // fill with junk
    uint64 idx = pa_vec_start + (((uint64)r - pa_vec_end) / PGSIZE) * sizeof(uint64);
    *(uint64 *)idx = 1;
  }
  return (void *)r;
}

//reuse the same physical memory when fork proc
void kalloc_reuse(void *pa)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < (char *)pa_vec_end || (uint64)pa >= PHYSTOP)
    panic("kalloc_reuse");

  uint64 idx = pa_vec_start + (((uint64)pa - pa_vec_end) / PGSIZE) * sizeof(uint64);
  *(uint64 *)idx = *(uint64 *)idx + 1;
}