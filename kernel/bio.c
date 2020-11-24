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

struct {
  uint unused[NBUF];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf bucket_head[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.bucket_lock[i], "bcache");
  }
  // Create linked list of buffers
  for (b = bcache.buf; b < bcache.buf+NBUF; b++)
  {
    initsleeplock(&b->lock,"buffer");
    b->used = 0;
  }
  
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for (int i = 0; i < NBUCKET; i++)
  {
    bcache.bucket_head[i].prev = &bcache.bucket_head[i];
    bcache.bucket_head[i].next = &bcache.bucket_head[i];
  }
  
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint id = ((((uint64)dev) << 32) | blockno) % NBUCKET;

  acquire(&bcache.bucket_lock[id]);

  // Is the block already cached?
  for(b = bcache.bucket_head[id].next; b != &bcache.bucket_head[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached; recycle an unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
      // b->dev = dev;
      // b->blockno = blockno;
      // b->valid = 0;
      // b->refcnt = 1;
      // release(&bcache.lock);
      // acquiresleep(&b->lock);
      // return b;
  //   }
  // }
  for (int i = 0; i < NBUF; i++)
  {
    if (!bcache.buf[i].used && __sync_bool_compare_and_swap(&bcache.buf[i].used, 0, 1)) {
        b = &bcache.buf[i];
        if (b->refcnt == 0)
        {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          b->next = bcache.bucket_head[id].next;
          b->prev = &bcache.bucket_head[id];
          bcache.bucket_head[id].next->prev = b;
          bcache.bucket_head[id].next = b;
          release(&bcache.bucket_lock[id]);
          acquiresleep(&b->lock);
          return b;
        }
    } 
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
    virtio_disk_rw(b->dev, b, 0);
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
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint id = ((((uint64)b->dev) << 32) | b->blockno) % NBUCKET;

  acquire(&bcache.bucket_lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    if (!__sync_bool_compare_and_swap(&b->used, 0, 1))
    {
      panic("brelse_cas");
    }
    
  }
  
  release(&bcache.bucket_lock[id]);
}

void
bpin(struct buf *b) {
  uint id = ((((uint64)b->dev) << 32) | b->blockno) % NBUCKET;

  acquire(&bcache.bucket_lock[id]);
  b->refcnt++;
  release(&bcache.bucket_lock[id]);
}

void
bunpin(struct buf *b) {
  uint id = ((((uint64)b->dev) << 32) | b->blockno) % NBUCKET;

  acquire(&bcache.bucket_lock[id]);
  b->refcnt--;
  release(&bcache.bucket_lock[id]);
}


