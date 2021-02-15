// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define BUCKET_NUM 13
struct
{
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  // struct buf head;
  struct buf *bucket_list[BUCKET_NUM];
  struct spinlock bucketlock[BUCKET_NUM];
} bcache;

uint hashBucket(uint dev, uint blockno)
{
  return (blockno * NDEV + dev) % BUCKET_NUM;
}

void binit(void)
{
  struct buf *b;
  int i;

  for (i = 0; i < BUCKET_NUM; i++)
  {
    initlock(bcache.bucketlock + i, "bcache-buket");
  }
  bcache.buf[0].next = bcache.buf;
  bcache.buf[0].prev = bcache.buf;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->refcnt = 0;
    b->next = bcache.buf[0].next;
    b->prev = bcache.buf;
    initsleeplock(&b->lock, "buffer");
    bcache.buf[0].next->prev = b;
    bcache.buf[0].next = b;
  }

  for (i = 0; i < BUCKET_NUM; i++)
    bcache.bucket_list[i] = 0;
  bcache.bucket_list[0] = bcache.buf;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket = hashBucket(dev, blockno);

  //printf("bget(): %d %d, wait for bucket lock %d\n", dev, blockno, bucket);
  acquire(bcache.bucketlock + bucket);
  //printf("bget(): %d %d, get bucket lock %d\n", dev, blockno, bucket);

  // Is the block already cached?
  b = bcache.bucket_list[bucket];
  if (b != 0)
  {
    do
    {
      if (b->dev == dev && b->blockno == blockno)
      {
        b->refcnt++;
        release(bcache.bucketlock + bucket);
        //printf("bget(): found the buf %d %d, release bucket lock %d\n", dev, blockno, bucket);
        //printf("bget(): found the buf %d %d, wait for buf lock %d\n", dev, blockno, ((uint64)b - (uint64)bcache.buf) / (sizeof(struct buf)));
        acquiresleep(&b->lock);
        //printf("bget(): found the buf %d %d, get buf lock %d\n", dev, blockno, ((uint64)b - (uint64)bcache.buf) / (sizeof(struct buf)));
        return b;
      }
      b = b->next;
    } while (b != bcache.bucket_list[bucket]);

    // has free buf in bucket ?
    do
    {
      if (b->refcnt == 0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(bcache.bucketlock + bucket);
        //printf("bget(): found the buf %d %d, release bucket lock %d\n", dev, blockno, bucket);
        //printf("bget(): found the buf %d %d, wait for buf lock %d\n", dev, blockno, ((uint64)b - (uint64)bcache.buf) / (sizeof(struct buf)));
        acquiresleep(&b->lock);
        //printf("bget(): found the buf %d %d, get buf lock %d\n", dev, blockno, ((uint64)b - (uint64)bcache.buf) / (sizeof(struct buf)));
        return b;
      }
      b = b->next;
    } while (b != bcache.bucket_list[bucket]);
  }

  release(bcache.bucketlock + bucket);
  //printf("bget(): not found the buf %d %d, release bucket lock %d\n", dev, blockno, bucket);

  // get free buf from other bucket
  for (int i = 0; i < BUCKET_NUM; i++)
  {
    //printf("bget(): not found the buf %d %d, wait for bucket lock %d\n", dev, blockno, i);
    acquire(bcache.bucketlock + i);
    //printf("bget(): not found the buf %d %d, get bucket lock %d\n", dev, blockno, i);
    b = bcache.bucket_list[i];
    if (b != 0)
    {
      // has free buf in bucket ?
      do
      {
        if (b->refcnt == 0)
        {
          if (b->next == b)
          {
            bcache.bucket_list[i] = 0;
          }
          else
          {
            b->next->prev = b->prev;
            b->prev->next = b->next;
            if (bcache.bucket_list[i] == b)
            {
              bcache.bucket_list[i] = b->next;
            }
          }
          release(bcache.bucketlock + i);
          //printf("bget(): found free buf %d %d, release bucket lock %d\n", dev, blockno, i);

          // init buf
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          //printf("bget(): found free buf %d %d, wait for bucket lock %d\n", dev, blockno, bucket);
          acquire(bcache.bucketlock + bucket);
          //printf("bget(): found free buf %d %d, get bucket lock %d\n", dev, blockno, bucket);
          if (bcache.bucket_list[bucket])
          {
            b->next = bcache.bucket_list[bucket]->next;
            b->prev = bcache.bucket_list[bucket];
            bcache.bucket_list[bucket]->next->prev = b;
            bcache.bucket_list[bucket]->next = b;
          }
          else
          {
            bcache.bucket_list[bucket] = b;
            b->next = b;
            b->prev = b;
          }
          release(bcache.bucketlock + bucket);
          //printf("bget(): found free buf %d %d, release bucket lock %d\n", dev, blockno, bucket);

          //printf("bget(): found the buf %d %d, wait for buf lock %d\n", dev, blockno, ((uint64)b - (uint64)bcache.buf) / (sizeof(struct buf)));
          acquiresleep(&b->lock);
          //printf("bget(): found the buf %d %d, get buf lock %d\n", dev, blockno, ((uint64)b - (uint64)bcache.buf) / (sizeof(struct buf)));
          return b;
        }
        b = b->next;
      } while (b != bcache.bucket_list[i]);
    }

    release(bcache.bucketlock + i);
    //printf("bget(): not found free buf %d %d, release bucket lock %d\n", dev, blockno, i);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the free list.
void brelse(struct buf *b)
{
  int bucket = hashBucket(b->dev, b->blockno);
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  //printf("brelse(): release the buf %d %d, reease buf lock %d\n", b->dev, b->blockno, ((uint64)b - (uint64)bcache.buf) / (sizeof(struct buf)));

  //printf("brelse(): %d %d, wait for bucket lock %d\n", b->dev, b->blockno, bucket);
  acquire(bcache.bucketlock + bucket);
  //printf("brelse(): %d %d, get bucket lock %d\n", b->dev, b->blockno, bucket);
  b->refcnt--;
  release(bcache.bucketlock + bucket);
  //printf("brelse(): %d %d, release bucket lock %d\n", b->dev, b->blockno, bucket);
}

void bpin(struct buf *b)
{
  int bucket = hashBucket(b->dev, b->blockno);

  //printf("bpin(): %d %d, wait for bucket lock %d\n", b->dev, b->blockno, bucket);
  acquire(bcache.bucketlock + bucket);
  //printf("bpin(): %d %d, get bucket lock %d\n", b->dev, b->blockno, bucket);
  b->refcnt++;
  release(bcache.bucketlock + bucket);
  //printf("bpin(): %d %d, release bucket lock %d\n", b->dev, b->blockno, bucket);
}

void bunpin(struct buf *b)
{
  int bucket = hashBucket(b->dev, b->blockno);

  //printf("bunpin(): %d %d, wait for bucket lock %d\n", b->dev, b->blockno, bucket);
  acquire(bcache.bucketlock + bucket);
  //printf("bunpin(): %d %d, get bucket lock %d\n", b->dev, b->blockno, bucket);
  b->refcnt--;
  release(bcache.bucketlock + bucket);
  //printf("bunpin(): %d %d, release bucket lock %d\n", b->dev, b->blockno, bucket);
}
