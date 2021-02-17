#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

char *sys_mmap(void)
{
  char *addr;
  int length;
  int prot, flags, fd, offset;
  struct file *f;
  struct vmr_t *vmr;
  int vmrd;
  struct proc *p = myproc();
  argaddr(0, (uint64 *)&addr);
  argint(1, &length);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, &offset);

  if (p->vmr_start - length <= PGROUNDUP(p->sz))
  {
    return (char *)-1;
  }

  if ((length % PGSIZE) != 0)
  {
    panic("sys_mmap: not page align");
  }

  f = p->ofile[fd];
  if (f == 0)
  {
    return (char *)-1;
  }
  if ((prot & PROT_READ || prot & PROT_EXEC) && !f->readable)
  {
    return (char *)-1;
  }
  if (prot & PROT_WRITE && flags & MAP_SHARED && !f->writable)
  {
    return (char *)-1;
  }

  for (vmrd = 0; vmrd < NPVMAS; vmrd++)
  {
    if (p->vmrs[vmrd] == 0)
    {
      break;
    }
  }
  if (vmrd == NPVMAS)
  {
    return (char *)-1;
  }

  vmr = allocvmr();
  if (vmr == 0)
  {
    return (char *)-1;
  }
  filedup(f);
  p->vmrs[vmrd] = vmr;
  vmr->file = f;
  vmr->flags = flags;
  vmr->prot = prot;
  vmr->length = length;
  vmr->offset = offset;
  if (addr == 0)
  {
    vmr->uvaddr = PGROUNDDOWN(p->vmr_start - length); // page aligin
    p->vmr_start = vmr->uvaddr;
  }
  else
  {
    vmr->uvaddr = (uint64)addr;
  }
  return (char *)vmr->uvaddr;
}

uint64 sys_munmap(void)
{
  char *addr;
  int length;
  struct vmr_t *vmr;
  int vmrd;
  struct proc *p = myproc();
  argaddr(0, (uint64 *)&addr);
  argint(1, &length);
  if (length <= 0)
  {
    return -1;
  }
  if ((length % PGSIZE) != 0 || ((uint64)addr % PGSIZE) != 0)
  {
    panic("sys_munmap: not page align");
  }
  for (vmrd = 0; vmrd < NPVMAS; vmrd++)
  {
    if (p->vmrs[vmrd])
    {
      vmr = p->vmrs[vmrd];
      if ((uint64)addr == (uint64)vmr->uvaddr)
      {
        if (length == vmr->length)
        {
          writebackvmr(vmr);
          uvmunmap(p->pagetable, (uint64)addr, length, 1);
          freevmr(vmr);
          p->vmrs[vmrd] = 0;
          return 0;
        }

        uint64 start = PGROUNDDOWN((uint64)addr);
        uint64 end = PGROUNDDOWN((uint64)addr + length);
        vmr->file->off = vmr->offset;
        for (; start < end; start += PGSIZE)
        {
          if (vmr->flags & MAP_SHARED)
          {
            if (filewrite(vmr->file, start, PGSIZE) < 0)
              return -1;
          }
          uvmunmap(p->pagetable, start, PGSIZE, 1);
          vmr->offset += PGSIZE;
          vmr->length -= PGSIZE;
        }
        vmr->uvaddr = end;
        return 0;
      }
      else if ((uint64)addr + length == (uint64)vmr->uvaddr + vmr->length)
      {
        uint64 start = PGROUNDDOWN((uint64)addr);
        uint64 end = PGROUNDUP((uint64)addr + length);
        vmr->file->off = vmr->offset + start - vmr->uvaddr;
        for (; start < end; start += PGSIZE)
        {
          if (vmr->flags & MAP_SHARED)
          {
            if (filewrite(vmr->file, start, PGSIZE) < 0)
              return -1;
          }
          uvmunmap(p->pagetable, start, PGSIZE, 1);
          vmr->length -= PGSIZE;
        }
        return 0;
      }
    }
  }
  return 0;
}