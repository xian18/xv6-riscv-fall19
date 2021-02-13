#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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

uint64 sys_sigalarm(void)
{
  int ticks = -1;
  void (*handler)() = 0;
  struct proc *p = myproc();
  argint(0, &ticks);
  if (ticks < 0)
  {
    return -1;
  }
  argaddr(1, (uint64 *)&handler);
  acquire(&p->lock);
  acquire(&p->siglock);
  p->ticks = ticks;
  p->handler = handler;
  p->cooldown = ticks;
  release(&p->siglock);
  release(&p->lock);
  return 0;
}

uint64 sys_sigreturn(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  acquire(&p->siglock);
  p->cooldown = p->ticks;
  p->context = p->sigcontext;
  *(p->tf) = p->sigtf;
  release(&p->siglock);
  release(&p->lock);
  return 0;
}