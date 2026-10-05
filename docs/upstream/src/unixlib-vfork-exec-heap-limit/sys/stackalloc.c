/* Non-contiguous stack chunk allocator.
   Copyright (c) 2002-2010 UnixLib Devlopers.
   Written by Alex Waugh, January 2002. */

/****************************************************************************
 * This file implements a heap that sits in application space and is intended
 * for allocating stack chunks.
 * The heap is split into blocks, and all memory is allocated in multiples of
 * the block size.
 * Each block has room for 4K of data, plus 8 bytes of overhead.
 * The top and bottom blocks in the heap are dummy blocks of 4 bytes each,
 * which give the appearance of being allocated blocks. This simplifies the
 * checking needed when trying to coalesce blocks.
 * Free blocks are coalesced where possible into groups of consecutive free
 * blocks. The linked list of free blocks contains only the first block in
 * each such group. The final block in each group contains a pointer to the
 * first block in the group.
 *
 * This file should be compiled without stack checking, as it is not
 * reentrant.
 *
 * Warning: __stackalloc and __stackfree must not use more than 256 bytes
 *          of stack space
 ***************************************************************************/

#include <stddef.h>
#include <swis.h>

#include <internal/os.h>
#include <internal/unix.h>
#include <pthread.h>

/* #define DEBUG 1 */
#ifdef DEBUG
#  include <sys/debug.h>
#endif

/* Increase 'appspace_himem' (and hence the wimpslot, if needed) by incr bytes.
   'appspace_limit' contains the actual wimpslot size, which may be greater
   than or equal-to 'appspace_himem'.  */
unsigned int
__stackalloc_incr_wimpslot (unsigned int incr)
{
  struct ul_memory *mem = &__ul_memory;
  struct ul_global *gbl = &__ul_global;

  /* 'appspace_himem' is never raised above the permitted RAM limit that the
     program was started with (see struct ul_memory): in a process that was
     started by vfork () + exec () the memory between that limit and
     'appspace_limit' is the copy of its parent, and a heap or stack that grew
     over it would destroy the parent.  The caller gets an error (brk:
     ENOMEM) and malloc falls back to mmap.  A process whose 'appspace_himem'
     is already above that value (a fork () child: __fork_post) is not
     affected: the difference wraps around.  */
  if (incr > mem->appspace_himem_max - mem->appspace_himem)
    return 0;

  /* If the amount requested does not exceed the current application space
     limit, then we can satisfy the request easily.  */
  if (mem->appspace_himem + incr <= mem->appspace_limit)
    {
#ifdef DEBUG
      debug_printf ("-- __stackalloc_incr_wimpslot: no need to increase\n");
#endif
      mem->appspace_himem += incr;
      return mem->appspace_himem;
    }

  /* We need to increase the application space size.  */
  unsigned int new_wimpslot = mem->appspace_himem + incr;

  /* The amount requested is rounded up to the next 32K in order to
     reduce the number of calls that we make to Wimp_SlotSize during
     the execution of the application.  */
  new_wimpslot = (new_wimpslot
		  + __DA_WIMPSLOT_ALIGNMENT) & ~__DA_WIMPSLOT_ALIGNMENT;

#ifdef DEBUG
  debug_printf ("-- __stackalloc_incr_wimpslot: attempting to"
		"increase wimpslot to %d\n", new_wimpslot);
#endif

  struct __sul_process *sulproc = gbl->sulproc;
  new_wimpslot = (unsigned int) sulproc->sul_wimpslot (sulproc->pid,
						       (void *) new_wimpslot);
  if (new_wimpslot == 0)
    {
#ifdef DEBUG
      debug_printf ("-- __stackalloc_incr_wimpslot: request failed\n");
#endif
      return 0;
    }

#ifdef DEBUG
  debug_printf ("-- __stackalloc_incr_wimpslot: increased wimpslot to %08x\n",
		new_wimpslot);
#endif

  /* Application space limit has been successfully increased.  */
  mem->appspace_limit = new_wimpslot;

  /* But check that application space has been increased by enough.  */
  if (mem->appspace_himem + incr > mem->appspace_limit)
    {
#ifdef DEBUG
      debug_printf ("-- __stackalloc_incr_wimpslot: wimpslot not increased by enough\n");
#endif
      return 0;
    }

  mem->appspace_himem += incr;
  return mem->appspace_himem;
}

#if __UNIXLIB_CHUNKED_STACK

#define BLOCK_FREE 0
#define BLOCK_DATA_SIZE 4096
#define DUMMY_BLOCK_SIZE 4

struct block
{
  /* Number of consecutive blocks allocated,
     or BLOCK_FREE for a free block.  */
  int size;
  union
  {
    struct
    {
      /* Next non-consecutive block in the freelist linked list.  */
      struct block *nextfreeblock;

      /* Number of consecutive free blocks.  */
      int numconsecutiveblocks;
    } free;

    /* The actual payload space returned to the user.  */
    char allocated[BLOCK_DATA_SIZE];
  } contents;

  /* First block in this group of consecutive free blocks.
     NULL if this block is allocated.
     Only valid in the last block in the group.  */
  struct block *startofcon;
};


/* Head of linked list of free blocks.  */
static struct block *freelist;

/* Dummy block header at the bottom of the heap.  */
static struct block *dummybottomblock;

/* Dummy block header at the top of the heap.  */
static struct block *dummytopblock;


/* Remove a block from the freelist.  */
static __inline void
REMOVE_FROM_FREELIST (struct block *remove)
{
  if (freelist == remove)
    freelist = remove->contents.free.nextfreeblock;
  else
    {
      struct block *blcklist = freelist;
      while (blcklist && blcklist->contents.free.nextfreeblock != remove)
        blcklist = blcklist->contents.free.nextfreeblock;

      if (blcklist)
        blcklist->contents.free.nextfreeblock = remove->contents.free.nextfreeblock;
    }
}

/* The adding to the freelist could perhaps be a bit more intelligent and
   add free blocks in an order such that when the freelist is searched
   blocks higher in the heap will be found first, which should help reduce
   fragmentation in a multithreaded program.  */
static __inline void
ADD_TO_FREELIST (struct block *add)
{
  add->contents.free.nextfreeblock = freelist;
  freelist = add;
}

/* Try to increase the stack heap upwards.  */
static struct block *
incr_upwards (int blocksneeded)
{
  struct ul_memory *mem = &__ul_memory;
  int realblocksneeded = blocksneeded;
  struct block *topblock = dummytopblock - 1;
  int foreign_incr = mem->appspace_himem != mem->old_himem;

  unsigned int incr;
  if (foreign_incr)
    {
      /* Someone else has increased himem, or there is our parent in the way.
         We need space for the new block and two dummy blocks */
      incr = blocksneeded * sizeof (struct block) + 2 * DUMMY_BLOCK_SIZE;
    }
  else
    {
      /* Nothing has increased himem since we last increased the
	 heap size.  */
      if (topblock->startofcon != NULL)
        {
          /* We can use the top block to provide part of the new block.  */
          topblock = topblock->startofcon;
          realblocksneeded -= topblock->contents.free.numconsecutiveblocks;
        }
      else
        {
          topblock += 1;
        }

      incr = realblocksneeded * sizeof (struct block);
    }

  if (! __stackalloc_incr_wimpslot (incr))
    return NULL;

  if (realblocksneeded != blocksneeded)
    {
      /* Only remove the block from the free list once we know the
         wimpslot incr was successful.  */
      REMOVE_FROM_FREELIST (topblock);
    }

  if (foreign_incr)
    {
      topblock = (struct block *)(mem->appspace_himem
				  - incr + DUMMY_BLOCK_SIZE);
      /* Setup a dummy block below the new block, to prevent us trying to
         coalesce with something that isn't in our heap.  */
      (topblock - 1)->startofcon = NULL;
    }

  mem->old_himem = mem->appspace_himem;

  /* Setup a new dummy block at the top of the heap.  */
  dummytopblock = topblock + blocksneeded;
  dummytopblock->size = 1;

  return topblock;
}

/* Try to increase the stack heap downwards.  */
static struct block *
incr_downwards (int blocksneeded)
{
  struct ul_memory *mem = &__ul_memory;
  int realblocksneeded = blocksneeded;
  struct block *bottomblock = dummybottomblock + 1;

  if (bottomblock->size == BLOCK_FREE)
    {
      /* We can use the bottom block to provide part of the new block */
      realblocksneeded -= bottomblock->contents.free.numconsecutiveblocks;
    }

  struct block *newbottomblock = bottomblock - realblocksneeded;

  unsigned int incr = realblocksneeded * sizeof (struct block);
  unsigned int new__stack = mem->stack - incr;

#ifdef DEBUG
  debug_printf ("incr_downwards: incr=%08x"
		" stack=%08x stack_limit=%08x\n",
		incr, mem->stack, mem->stack_limit);
#endif

  if (new__stack >= mem->stack_limit && new__stack <= mem->stack)
    {
      mem->stack = new__stack;

      if (realblocksneeded != blocksneeded)
        REMOVE_FROM_FREELIST (bottomblock);

      /* Setup a new dummy block at the bottom of the heap */
      dummybottomblock = newbottomblock - 1;
      dummybottomblock->startofcon = NULL;
    }
  else
    {
#ifdef DEBUG
      debug_printf ("-- incr_downwards: no free memory below stack heap\n");
#endif
      newbottomblock = NULL;
    }

  return newbottomblock;
}


/* Release any free memory at the bottom of the heap.
   Returns non-zero if it managed to free anything.  */
int
__stackalloc_trim (void)
{
  struct ul_memory *mem = &__ul_memory;
  struct ul_global *gbl = &__ul_global;

  if (gbl->pthread_system_running)
    __pthread_disable_ints ();

  /* Find start of the last real block in the heap */
  struct block *bottomblock = dummybottomblock + 1;

  if (bottomblock->size != BLOCK_FREE)
    {
      if (gbl->pthread_system_running)
        __pthread_enable_ints ();
      return 0; /* Unable to free anything */
    }

  REMOVE_FROM_FREELIST (bottomblock);

  /* Move dummy block up to where the end of the bottom block was */
  dummybottomblock = bottomblock
    + bottomblock->contents.free.numconsecutiveblocks - 1;
  dummybottomblock->startofcon = NULL;
  mem->stack = (unsigned int) dummybottomblock + sizeof(struct block) - 4;

#ifdef DEBUG
  debug_printf ("-- __stackalloc_trim: __ul_memory.stack=%08x\n", mem->stack);
#endif

  if (gbl->pthread_system_running)
    __pthread_enable_ints ();

  return 1;
}

/* Initialise the stackalloc heap
   No need for thread-safety, as only called before threads initialised */
void
__stackalloc_init (void)
{
  /* The stack is set up in _syslib.s as follows:
   *
   * appspace_himem   -------------
   *                     8 bytes    - <-- dummytopblock
   * Signalhandler SP ------------- -
   *                                 \
   *                                  \
   *                   3584 bytes      \
   *                                    \
   *                                     } Signal handler chunk (4k)
   * Signalhandler SL -------------     /
   *                                   /
   *                    512 bytes     /
   *                                 /
   *                  ------------- -
   *                     8 bytes    - <-- signalhandlerblock (avoid!)
   * Initial SP       ------------- -
   *                                 \
   *                                  \
   *                   3560 bytes      \
   *                                    \
   *                                     } Initial chunk (4k)
   * Initial SL       -------------     /
   *                                   /
   *                    536 bytes     /
   *                                 /
   *                  ------------- -
   *                     8 bytes    - <-- initialblock
   * __ulmemory.stack -------------
   */

#ifdef DEBUG
  debug_printf ("-- stackalloc_init: stack=%08x  appspace_himem=%08x  "
		"appspace_limit=%08x\n",
		mem->stack, mem->appspace_himem,
		mem->appspace_limit);
#endif

  /* Record the value of himem when the initial stack chunk was setup */
  struct ul_memory *mem = &__ul_memory;
  mem->old_himem = mem->appspace_himem;

  mem->appspace_himem = mem->appspace_limit;

  struct block *initialblock = (struct block *) (mem->stack + 4);
  initialblock->size = 1;
  initialblock->startofcon = NULL;

  dummybottomblock = initialblock - 1;
  dummybottomblock->startofcon = NULL;

  /* Initialise the signal handler block */
  dummytopblock = initialblock + 1;
  dummytopblock->size = 1;
  dummytopblock->startofcon = NULL;

  dummytopblock += 1;
  dummytopblock->size = 1;

  freelist = NULL;
}

/* Allocate some space from the heap - equivalent to malloc().  */
void *
__stackalloc (size_t size)
{
  struct ul_global *gbl = &__ul_global;

  if (gbl->pthread_system_running)
    __pthread_disable_ints ();

  /* Convert size into number of blocks */
  int blocksneeded = size + BLOCK_DATA_SIZE - 1;
  blocksneeded &= ~(BLOCK_DATA_SIZE - 1);
  /* BLOCK_DATA_SIZE should be a power of two so
     this should optimise to a shift.  */
  blocksneeded /= BLOCK_DATA_SIZE;
#ifdef DEBUG
  debug_printf ("-- __stackalloc: size=%08x, blocksneeded=%08x\n",
		size, blocksneeded);
#endif

  /* Search through freelist to find the first smallest block
     that is large enough for this request.  */
  struct block *blocklist = freelist;
  struct block *blocktoalloc = NULL;
  while (blocklist)
    {
      if (blocklist->contents.free.numconsecutiveblocks == blocksneeded)
        {
          /* We have found a block of exactly the right size,
	     so no need to search any further.  */
          blocktoalloc = blocklist;
          break;
        }
      if (blocklist->contents.free.numconsecutiveblocks > blocksneeded)
	{
	  if (blocktoalloc == NULL
	      || (blocklist->contents.free.numconsecutiveblocks
		  < blocktoalloc->contents.free.numconsecutiveblocks))
	    {
	      /* We have found a block that is smaller then the best so far,
		 but still bigger than needed so carry on searching.  */
	      blocktoalloc = blocklist;
	    }
	}
      blocklist = blocklist->contents.free.nextfreeblock;
    }

  if (blocktoalloc == NULL)
    {
      /* No suitable block found, so try to increase the size of the heap.
         Try extending the stack heap downwards if room, otherwise increase
         the wimpslot */
      blocktoalloc = (struct block *) incr_downwards (blocksneeded);

      if (blocktoalloc == NULL)
        blocktoalloc = (struct block *) incr_upwards (blocksneeded);

      if (blocktoalloc == NULL)
        {
#ifdef DEBUG
          debug_printf ("-- __stackalloc: no free memory\n");
#endif
          if (gbl->pthread_system_running)
            __pthread_enable_ints ();
          return NULL;
        }
    }
  else
    {
      /* A suitable block was found, so use it */
      REMOVE_FROM_FREELIST (blocktoalloc);
      int blocksleft = blocktoalloc->contents.free.numconsecutiveblocks - blocksneeded;
      if (blocksleft > 0)
        {
          /* The block is bigger than we need, so add the remainder
	     back to the freelist.  */
          struct block *newfreeblock = blocktoalloc + blocksneeded;
          struct block *lastfreeblock = blocktoalloc
	    + blocktoalloc->contents.free.numconsecutiveblocks - 1;

          ADD_TO_FREELIST (newfreeblock);
          newfreeblock->contents.free.numconsecutiveblocks = blocksleft;
          newfreeblock->size = BLOCK_FREE;
          lastfreeblock->startofcon = newfreeblock;
        }
    }

  /* At this point, blocktoalloc contains a block of exactly
     the right size, and is not in the freelist.  */
  blocktoalloc->size = blocksneeded;
  struct block *lastblocktoalloc = blocktoalloc + blocksneeded - 1;
  lastblocktoalloc->startofcon = NULL;

  void *returnptr = (char *)blocktoalloc
		      + offsetof (struct block, contents.allocated);
#ifdef DEBUG
  debug_printf ("-- __stackalloc: returning %08x\n", returnptr);
#endif

  if (gbl->pthread_system_running)
    __pthread_enable_ints ();

  return returnptr;
}

/* Free a chunk - equivalent to free().  */
void
__stackfree (void *ptr)
{
  struct ul_global *gbl = &__ul_global;

  if (gbl->pthread_system_running)
    __pthread_disable_ints ();

#ifdef DEBUG
  debug_printf ("__stackfree: ptr=%08x\n", ptr);
#endif

  /* Find start of block this ptr refers to.  */
  struct block *blocktofree =
    (struct block *)(void *)((u_char *)ptr
			     - offsetof (struct block, contents.allocated));

  struct block *prevblock = blocktofree - 1;
  struct block *startblock = prevblock->startofcon;
  if (startblock == NULL)
    {
      /* Block below this is not free, so we need to add this
	 block to the freelist.  */
#ifdef DEBUG
      debug_printf ("-- __stackfree: adding to free list\n");
#endif
      startblock = blocktofree;
      startblock->contents.free.numconsecutiveblocks = 0;
      ADD_TO_FREELIST (startblock);
    }
#ifdef DEBUG
  else
    {
      /* Block below this is free, so we can coalesce with it and
	 hence don't need to add this block to the freelist.  */
      debug_printf ("-- __stackfree: coalescing with lower block\n");
    }
#endif

  startblock->contents.free.numconsecutiveblocks += blocktofree->size;

  struct block *nextblock = blocktofree + blocktofree->size;
  if (nextblock->size == BLOCK_FREE)
    {
      /* Block above this is free, so coalesce with it.
         This block has already been added to the freelist,
         so we need to remove the nextblock from the freelist.  */
#ifdef DEBUG
      debug_printf ("-- __stackfree: coalescing with higher block\n");
#endif
      REMOVE_FROM_FREELIST (nextblock);
      startblock->contents.free.numconsecutiveblocks += nextblock->contents.free.numconsecutiveblocks;
    }

  struct block *lastblock = startblock + startblock->contents.free.numconsecutiveblocks - 1;
  lastblock->startofcon = startblock;
  blocktofree->size = BLOCK_FREE;

  if (gbl->pthread_system_running)
    __pthread_enable_ints ();
}

#endif
