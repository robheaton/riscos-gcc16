/* Heap of the programs of the native (RISC OS-hosted) tool chain: the gcc/g++/cpp drivers, collect2, cc1, cc1plus, as, ld, ar, make, ...

   Linked into every one of them (the build scripts add the object riscos-da.o to LDFLAGS).

   Without this a UnixLib program keeps its malloc heap in the Wimp slot, unless the OS variable <program>$Heap is set.  That is wrong for a compiler pass:
   it is started by vfork+exec, and SharedUnixLibrary keeps a copy of the parent (the driver) at the top of the slot while the child runs, limiting the child's
   memory to what lies below it.  UnixLib's heap growth (brk_rw -> __stackalloc_incr_wimpslot) does not honour that limit: the child's heap grows over the
   copy of its parent, and when the child has finished the parent is restored from garbage and dies ("abort on data transfer").  A heap in a dynamic area
   is independent of the slot.

   __dynamic_da_name is a WEAK reference in libunixlib.so; defining it in the executable is all that is needed (UnixLib README, "Link-time features").

   The MAXIMUM size of the area is only reserved logical address space (the memory is mapped as the heap grows), but the reservations of all the programs that are alive
   at once add up, and they must fit in the free logical address space: make -> gcc -> collect2 -> ld with 512 MB each (2 GB) did not ("Unable to allocate logical
   address space").  So this object does NOT define __dynamic_da_max_size: the programs that only start others (the drivers, collect2, make) get UnixLib's default of 32 MB,
   and the programs that can use a lot of memory define the big maximum themselves: riscos-da-big.o (binutils: as, ld, ...) and riscos.c (cc1, cc1plus).
   The OS variable <program>$HeapMax (an integer, in MB) overrides either for one program.  */
const char * const __dynamic_da_name = "GCC heap";
