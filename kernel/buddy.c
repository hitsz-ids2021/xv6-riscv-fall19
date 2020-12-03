#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// Buddy allocator

static int nsizes;     // the number of entries in bd_sizes array

#define LEAF_SIZE     16                         // The smallest block size
#define MAXSIZE       (nsizes-1)                 // Largest index in bd_sizes array
#define BLK_SIZE(k)   ((1L << (k)) * LEAF_SIZE)  // Size of block at size k
#define HEAP_SIZE     BLK_SIZE(MAXSIZE) 
#define NBLK(k)       (1 << (MAXSIZE-k))         // Number of block at size k
#define ROUNDUP(n,sz) (((((n)-1)/(sz))+1)*(sz))  // Round up to the next multiple of sz

typedef struct list Bd_list;

// The allocator has sz_info for each size k. Each sz_info has a free
// list, an array alloc to keep track which blocks have been
// allocated, and an split array to to keep track which blocks have
// been split.  The arrays are of type char (which is 1 byte), but the
// allocator uses 1 bit per block (thus, one char records the info of
// 8 blocks).
struct sz_info {
  Bd_list free;
  char *alloc;
  char *split;
};
typedef struct sz_info Sz_info;

static Sz_info *bd_sizes; 
static void *bd_base;   // start address of memory managed by the buddy allocator
static struct spinlock lock;

// Return 1 if bit at position index in array is set to 1
int bit_isset(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  return (b & m) == m;
}

// Set bit at position index in array to 1
void bit_set(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  array[index/8] = (b | m);
}

// Clear bit at position index in array
void bit_clear(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  array[index/8] = (b & ~m);
}

//进行异或操作
void bit_xor(char *array, int index){
  //异或：m的其他位都是0，index位是1
  char m = (1 << (index % 8));
  array[index/8] = array[index/8] ^ m;
}

// Print a bit vector as a list of ranges of 1 bits
void
bd_print_vector(char *vector, int len) {
  int last, lb;
  
  last = 1;
  lb = 0;
  for (int b = 0; b < len; b++) {
    if (last == bit_isset(vector, b))
      continue;
    if(last == 1)
      printf(" [%d, %d)", lb, b);
    lb = b;
    last = bit_isset(vector, b);
  }
  if(lb == 0 || last == 1) {
    printf(" [%d, %d)", lb, len);
  }
  printf("\n");
}

// Print buddy's data structures
void
bd_print() {
  for (int k = 0; k < nsizes; k++) {
    printf("size %d (blksz %d nblk %d): free list: ", k, BLK_SIZE(k), NBLK(k));
    lst_print(&bd_sizes[k].free);
    printf("  alloc:");
    bd_print_vector(bd_sizes[k].alloc, NBLK(k));
    if(k > 0) {
      printf("  split:");
      bd_print_vector(bd_sizes[k].split, NBLK(k));
    }
  }
}

// What is the first k such that 2^k >= n?
int
firstk(uint64 n) {
  int k = 0;
  uint64 size = LEAF_SIZE;

  while (size < n) {
    k++;
    size *= 2;
  }
  return k;
}

// Compute the block index for address p at size k
int
blk_index(int k, char *p) {
  int n = p - (char *) bd_base;
  return n / BLK_SIZE(k);
}

// Convert a block index at size k back into an address
void *addr(int k, int bi) {
  int n = bi * BLK_SIZE(k);
  return (char *) bd_base + n;
}

// allocate nbytes, but malloc won't return anything smaller than LEAF_SIZE
void *
bd_malloc(uint64 nbytes)
{
  int fk, k;
  acquire(&lock);

  // Find a free block >= nbytes, starting with smallest k possible
  fk = firstk(nbytes);
  for (k = fk; k < nsizes; k++) {
    //每一级列表都是空的
    if(!lst_empty(&bd_sizes[k].free))
      break;
  }
  if(k >= nsizes) { // No free blocks?
    release(&lock);
    return 0;
  }

  // Found a block; pop it and potentially split it.
  char *p = lst_pop(&bd_sizes[k].free);
  //使用1bit来表示Buddy
  //下标从0开始
  //原文：bit_set(bd_sizes[k].alloc, blk_index(k, p));
  bit_xor(bd_sizes[k].alloc, blk_index(k, p)/2);
  

  for(; k > fk; k--) {
    // split a block at size k and mark one half allocated at size k-1
    // and put the buddy on the free list at size k-1
    char *q = p + BLK_SIZE(k-1);   // p's buddy
    bit_set(bd_sizes[k].split, blk_index(k, p));

    //原文：bit_set(bd_sizes[k-1].alloc, blk_index(k-1, p));
    bit_xor(bd_sizes[k-1].alloc, blk_index(k-1, p)/2);

    lst_push(&bd_sizes[k-1].free, q);
  }
  release(&lock);
  return p;
}

// Find the size of the block that p points to.
int
size(char *p) {
  for (int k = 0; k < nsizes; k++) {
    if(bit_isset(bd_sizes[k+1].split, blk_index(k+1, p))) {
      return k;
    }
  }
  return 0;
}

// Free memory pointed to by p, which was earlier allocated using
// bd_malloc.
void
bd_free(void *p) {
  void *q;
  int k;
  //p是memory块
  acquire(&lock);
  for (k = size(p); k < MAXSIZE; k++) {
    //bi的buddy
    int bi = blk_index(k, p);
    int buddy = (bi % 2 == 0) ? bi+1 : bi-1;
    bit_xor(bd_sizes[k].alloc, bi/2); 
    // is buddy allocated?
    // break out of loop
    if (bit_isset(bd_sizes[k].alloc, buddy/2)) {  
      break;   
    }
    // budy is free; merge with buddy
    q = addr(k, buddy);
    lst_remove(q);    // remove buddy from free list
    //如果buddy是偶数，表明buddy才是嫡长子，p是私生子
    if(buddy % 2 == 0) {
      p = q;
    }
    // at size k+1, mark that the merged buddy pair isn't split
    // anymore
    bit_clear(bd_sizes[k+1].split, blk_index(k+1, p));
  }
  lst_push(&bd_sizes[k].free, p);
  release(&lock);
}

// Compute the first block at size k that doesn't contain p
int
blk_index_next(int k, char *p) {
  int n = (p - (char *) bd_base) / BLK_SIZE(k);
  //p可能不与Block的边界对齐：n++
  if((p - (char*) bd_base) % BLK_SIZE(k) != 0)
      n++;
  return n ;
}

int
log2(uint64 n) {
  int k = 0;
  while (n > 1) {
    k++;
    n = n >> 1;
  }
  return k;
}

// Mark memory from [start, stop), starting at size 0, as allocated. 
void
bd_mark(void *start, void *stop)
{
  int bi, bj;

  if (((uint64) start % LEAF_SIZE != 0) || ((uint64) stop % LEAF_SIZE != 0))
    panic("bd_mark");

  for (int k = 0; k < nsizes; k++) {
    bi = blk_index(k, start);
    bj = blk_index_next(k, stop);
    for(; bi < bj; bi++) {
      if(k > 0) {
        // if a block is allocated at size k, mark it as split too.
        bit_set(bd_sizes[k].split, bi);
      }
      //原文：bit_set(bd_sizes[k].alloc, bi);
      bit_xor(bd_sizes[k].alloc, bi/2);
    }
  }
}


int
bd_initfree_pair(int k, int bi, int direction) {
  int buddy = (bi % 2 == 0) ? bi+1 : bi-1;
  int free = 0;
  //如果bit_isset，说明有一边被使用，另一边free
  //如果bit_isset=0，要么二者都空闲，要么二者都被使用；
  //二者都空闲，则上一层必定是空闲；他们的Free必定已经被计数过。而且他们未被分割
  if(bit_isset(bd_sizes[k].alloc, bi/2)) {
    free = BLK_SIZE(k);
    //direction为1，表明在处理右侧和无效区。将左侧的块加入free
    //direction为0，表明在处理左侧和元信息。将右侧的块加入free
    if(bi % 2 == direction) 
      lst_push(&bd_sizes[k].free, addr(k, buddy)); 
    else
      lst_push(&bd_sizes[k].free, addr(k, bi));
  }
  return free;
}


  
// Initialize the free lists for each size k.  For each size k, there
// are only two pairs that may have a buddy that should be on free list:
// bd_left and bd_right.
int
bd_initfree(void *bd_left, void *bd_right) {
  int free = 0;

  for (int k = 0; k < MAXSIZE; k++) {   // skip max size
    //
    int left = blk_index_next(k, bd_left);
    int right = blk_index(k, bd_right);
    //元信息分配在空间的头部：一定是左侧的block被分配
    free += bd_initfree_pair(k, left,0);
    if(right <= left)
      continue;
    //无效区在空间的尾部：一定是右侧的block被分配
    free += bd_initfree_pair(k, right,1);
  }
  return free;
}

// Mark the range [bd_base,p) as allocated
int
bd_mark_data_structures(char *p) {
  int meta = p - (char*)bd_base;
  printf("bd: %d meta bytes for managing %d bytes of memory\n", meta, BLK_SIZE(MAXSIZE));
  bd_mark(bd_base, p);
  return meta;
}

// Mark the range [end, HEAPSIZE) as allocated
int
bd_mark_unavailable(void *end, void *left) {
  int unavailable = BLK_SIZE(MAXSIZE)-(end-bd_base);
  if(unavailable > 0)
    unavailable = ROUNDUP(unavailable, LEAF_SIZE);
  printf("bd: 0x%x bytes unavailable\n", unavailable);

  void *bd_end = bd_base+BLK_SIZE(MAXSIZE)-unavailable;
  bd_mark(bd_end, bd_base+BLK_SIZE(MAXSIZE));
  return unavailable;
}

// Initialize the buddy allocator: it manages memory from [base, end).
void
bd_init(void *base, void *end) {
  //ROUNDUP对齐
  char *p = (char *) ROUNDUP((uint64)base, LEAF_SIZE);
  int sz;

  initlock(&lock, "buddy");
  bd_base = (void *) p;

  // compute the number of sizes we need to manage [base, end)
  //nsizes是2的次方。只加一次，达到Block
  nsizes = log2(((char *)end-p)/LEAF_SIZE) + 1;
  if((char*)end-p > BLK_SIZE(MAXSIZE)) {
    nsizes++;  // round up to the next power of 2
  }

  printf("bd: memory sz is %d bytes; allocate an size array of length %d\n",
         (char*) end - p, nsizes);

  // allocate bd_sizes array
  //p是16Byte对齐的开始指针
  bd_sizes = (Sz_info *) p;
  //有nsizes层
  p += sizeof(Sz_info) * nsizes;
  memset(bd_sizes, 0, sizeof(Sz_info) * nsizes);

  // initialize free list and allocate the alloc array for each size k

  //这里要修改alloc array的malloc

  //p是移动的
  for (int k = 0; k < nsizes; k++) {
    lst_init(&bd_sizes[k].free);
    //原文：sz = sizeof(char)* ROUNDUP(NBLK(k), 8)/8;
    //现在1个bit可以对应2个memory block
    sz = sizeof(char)* ROUNDUP(NBLK(k), 16)/16;
    bd_sizes[k].alloc = p;
    memset(bd_sizes[k].alloc, 0, sz);
    p += sz;
  }

  // allocate the split array for each size k, except for k = 0, since
  // we will not split blocks of size k = 0, the smallest size.
  for (int k = 1; k < nsizes; k++) {
    sz = sizeof(char)* (ROUNDUP(NBLK(k), 8))/8;
    bd_sizes[k].split = p;
    memset(bd_sizes[k].split, 0, sz);
    p += sz;
  }
  //使p 16Byte对齐
  p = (char *) ROUNDUP((uint64) p, LEAF_SIZE);

  // done allocating; mark the memory range [base, p) as allocated, so
  // that buddy will not hand out that memory.
  int meta = bd_mark_data_structures(p);
  
  // mark the unavailable memory range [end, HEAP_SIZE) as allocated,
  // so that buddy will not hand out that memory.
  int unavailable = bd_mark_unavailable(end, p);
  void *bd_end = bd_base+BLK_SIZE(MAXSIZE)-unavailable;
  
  // initialize free lists for each size k
  int free = bd_initfree(p, bd_end);

  // check if the amount that is free is what we expect
  if(free != BLK_SIZE(MAXSIZE)-meta-unavailable) {
    printf("free %d %d\n", free, BLK_SIZE(MAXSIZE)-meta-unavailable);
    panic("bd_init: free mem");
  }
}

