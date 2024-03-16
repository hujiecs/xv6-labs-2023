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

#define NBUCKET 13

// when bcache[i].lock is hold, bcache[i] structure won't change
// buf in this bucket cannot move to another bucket, and refcnt don't change.
struct {
  struct spinlock lock;
  struct buf *head;
} bcache[NBUCKET];

// serialize the logic to find a unused buf
struct spinlock evictlock;

struct buf bufs[NBUF];

void
binit(void)
{
  for (int i = 0; i < NBUCKET; i++)
    initlock(&bcache[i].lock, "bcache");
  
  for (int i = 0; i < NBUF; i++) {
    initsleeplock(&bufs[i].lock, "buffer");
    int bucket = i % NBUCKET;
    bufs[i].next = bcache[bucket].head;
    bcache[bucket].head = &bufs[i];
  }

  initlock(&evictlock, "bcache_evict");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket = blockno % NBUCKET;
  
  acquire(&bcache[bucket].lock);
  b = bcache[bucket].head;
  while (b) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache[bucket].lock);
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  }
  release(&bcache[bucket].lock);

  acquire(&evictlock);

  // check again, it's possible blockno is cached by 
  // another thread so we cannot evict for such case
  b = bcache[bucket].head;
  while (b) {
    if (b->dev == dev && b->blockno == blockno) {
      acquire(&bcache[bucket].lock);
      b->refcnt++;
      release(&bcache[bucket].lock);
      release(&evictlock);
      acquiresleep(&b->lock);

      return b;
    }
    b = b->next;
  }

  // now we know we have to evict one buf
  for (int i = 0; i < NBUCKET; i++) {
    acquire(&bcache[i].lock);
    b = bcache[i].head;
    struct buf *prev = 0;
    while (b) {
      if (b->refcnt == 0) {
        if (i != bucket) {
          // remove b from bcache[i]
          if (prev)
            prev->next = b->next;
          else
            bcache[i].head = b->next;
          release(&bcache[i].lock);

          // there's a issue between we release lock-i and acquire lock-bucket
          // another thread may doing the same thing, evict another "b" block
          // now two buf point to same block, that's why we need evictlock
          // see https://blog.miigon.net/posts/s081-lab8-locks for details

          // insert b in bcache[bucket]
          acquire(&bcache[bucket].lock);
          b->next = bcache[bucket].head;
          bcache[bucket].head = b;
        }

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache[bucket].lock);
        release(&evictlock);
        acquiresleep(&b->lock);

        return b;
      }
      prev = b;
      b = b->next;
    }
    release(&bcache[i].lock);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket = b->blockno % NBUCKET;
  acquire(&bcache[bucket].lock);
  b->refcnt--;
  release(&bcache[bucket].lock);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache[bucket].lock);
  b->refcnt++;
  release(&bcache[bucket].lock);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache[bucket].lock);
  b->refcnt--;
  release(&bcache[bucket].lock);
}


