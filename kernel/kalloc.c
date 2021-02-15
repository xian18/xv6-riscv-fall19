// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end, int cpuid);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;

void kinit()
{
  uint64 pa_start = PGROUNDUP((uint64)end);
  uint64 pa_end = PGROUNDDOWN(PHYSTOP);
  uint64 size = (pa_end - pa_start) / NCPU + PGSIZE;
  uint64 j = pa_start;
  for (int i = 0; i < NCPU; i++, j += size)
  {
    initlock(kmem.lock + i, "kmem");
    if (i != NCPU - 1)
    {
      freerange((void *)j, (void *)j + size, i);
    }
    else
    {
      freerange((void *)j, (void *)pa_end, i);
    }
  }
}

void freerange(void *pa_start, void *pa_end, int cpuid)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  struct run *r;
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {

    if (((uint64)p % PGSIZE) != 0 || (char *)p < end || (uint64)p >= PHYSTOP)
      panic("kfree");

    // Fill with junk to catch dangling refs.
    memset((char *)p, 1, PGSIZE);

    r = (struct run *)p;

    acquire(kmem.lock + cpuid);
    r->next = kmem.freelist[cpuid];
    kmem.freelist[cpuid] = r;
    release(kmem.lock + cpuid);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;
  push_off();
  int id = cpuid();
  pop_off();
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(kmem.lock + id);
  r->next = kmem.freelist[id];
  kmem.freelist[id] = r;
  release(kmem.lock + id);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();
  pop_off();

  acquire(kmem.lock + id);
  r = kmem.freelist[id];
  if (r)
  {
    kmem.freelist[id] = r->next;
    release(kmem.lock + id);
  }
  else
  {
    release(kmem.lock + id);
    for (int i = 0; i < NCPU; i++)
    {
      acquire(kmem.lock + i);
      r = kmem.freelist[i];
      if (r)
      {
        kmem.freelist[i] = r->next;
        release(kmem.lock + i);
        break;
      }
      release(kmem.lock + i);
    }
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
