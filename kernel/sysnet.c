//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

struct sock
{
  struct sock *next;    // the next socket in the list
  uint32 raddr;         // the remote IPv4 address
  uint16 lport;         // the local UDP port number
  uint16 rport;         // the remote UDP port number
  struct spinlock lock; // protects the rxq
  struct mbufq rxq;     // a queue of packets waiting to be received
  uint nread;
};

static struct spinlock lock;
static struct sock *sockets;

void sockinit(void)
{
  initlock(&lock, "socktbl");
}

int sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock *)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  si->nread = lport;
  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos)
  {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
        pos->rport == rport)
    {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char *)si);
  if (*f)
    fileclose(*f);
  return -1;
}

//
// Your code here.
//
// Add and wire in methods to handle closing, reading,
// and writing for network sockets.
//

int sockwrite(struct sock *s, uint64 addr, int n)
{
  int i;
  char ch;
  struct proc *pr = myproc();
  struct mbuf *buf;
  //printf("sockwrite(): acquire sock lock %p\n", s);
  acquire(&s->lock);
  //printf("sockwrite(): get sock lock %p\n", s);
  buf = mbufalloc(MBUF_DEFAULT_HEADROOM);
  for (i = 0; i < n; i++)
  {
    if (copyin(pr->pagetable, &ch, addr + i, 1) == -1)
      break;
    buf->head[i] = ch;
  }
  buf->len = n;
  net_tx_udp(buf, s->raddr, s->lport, s->rport);
  release(&s->lock);
  //printf("sockwrite(): release sock lock %p\n", s);
  return n;
}

int sockread(struct sock *s, uint64 addr, int n)
{

  int i;
  struct proc *pr = myproc();
  char ch;

  //printf("sockread(): acquire sock lock %p\n", s);
  acquire(&s->lock);
  //printf("sockread(): get sock lock %p\n", s);
  while (mbufq_empty(&s->rxq))
  {
    if (myproc()->killed)
    {
      //printf("sockread(): proc is killed, release sock lock %p\n", s);
      release(&s->lock);
      return -1;
    }
    //printf("sockread(): mbuf queue is empty, sleep %p, release sock lock %p\n", &s->lock, s);
    sleep(&s->nread, &s->lock);
    //printf("sockread(): wacked up %p, get sock lock %p\n", &s->lock, s);
  }
  struct mbuf *tmp = mbufq_pophead(&s->rxq);
  for (i = 0; i < tmp->len; i++)
  {
    ch = (tmp->head)[i];
    if (copyout(pr->pagetable, addr + i, &ch, 1) == -1)
      break;
  }
  release(&s->lock);
  //printf("sockread(): release sock lock %p\n", s);
  return i;
}

void sockclose(struct sock *s)
{
  struct sock *pos;

  //printf("sockclose(): acquire sockets lock\n");
  acquire(&lock);
  //printf("sockclose(): get sockets lock\n");
  if (sockets == s)
  {
    sockets = s->next;
  }
  else
  {
    pos = sockets;
    while (pos)
    {
      //printf("sockclose(): acquire sock lock %p\n", pos);
      acquire(&pos->lock);
      //printf("sockclose(): get sock lock %p\n", pos);
      if (pos->next == s)
      {
        break;
      }
      release(&pos->lock);
      //printf("sockclose(): release sock lock %p\n", pos);
      pos = pos->next;
    }
    if (!pos)
    {
      panic("sock not in chain");
    }
    pos->next = s->next;
    release(&pos->lock);
    //printf("sockclose(): release sock lock %p\n", pos);
  }

  release(&lock);
  //printf("sockclose(): get sockets lock\n");

  //printf("sockclose(): acquire sock lock %p\n", s);
  acquire(&s->lock);
  //printf("sockclose(): get sock lock %p\n", s);
  if (!mbufq_empty(&s->rxq))
  {
    wakeup(&s->nread);
    //printf("sockclose(): wackup %p\n", &s->nread);
  }
  while (!mbufq_empty(&s->rxq))
  {
    mbuffree(mbufq_pophead(&s->rxq));
  }

  release(&s->lock);
  //printf("sockclose(): release sock lock %p\n", s);
  kfree(s);
}

// called by protocol handler layer to deliver UDP packets
void sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  //
  // Your code here.
  //
  // Find the socket that handles this mbuf and deliver it, waking
  // any sleeping reader. Free the mbuf if there are no sockets
  // registered to handle it.
  //
  struct sock *pos;
  //printf("sockrecvudp(): acquire sockets lock\n");
  acquire(&lock);
  //printf("sockrecvudp(): get sockets lock\n");
  pos = sockets;
  while (pos)
  {

    //printf("sockrecvudp(): acquire sock lock %p\n", pos);
    acquire(&pos->lock);
    //printf("sockrecvudp(): get sock lock %p\n", pos);
    if (pos->raddr == raddr &&
        pos->lport == lport &&
        pos->rport == rport)
    {
      release(&lock);
      //printf("sockrecvudp(): release sockets lock\n");
      break;
    }
    release(&pos->lock);
    pos = pos->next;
    //printf("sockrecvudp(): release sock lock %p\n", pos);
  }
  if (!pos)
  {
    release(&lock);
    //printf("sockrecvudp(): release sockets lock\n");
    mbuffree(m);
    return;
  }
  wakeup(&pos->nread);
  //printf("sockrecvudp(): wackup %p\n", &pos->nread);
  mbufq_pushtail(&pos->rxq, m);
  release(&pos->lock);
  //printf("sockrecvudp(): release sock lock %p\n", pos);
}
