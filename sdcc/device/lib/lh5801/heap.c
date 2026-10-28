/*-------------------------------------------------------------------------
   heap.c - the memory malloc() hands out on the PC-1500 (LH5801 port).

   Paul Chambre, 2026

   malloc() (device/lib/malloc.c) takes its blocks from __sdcc_heap,
   __sdcc_heap_size bytes, and sets the heap up on its first call. It is
   only linked into a program that uses malloc(), and like any other DATA
   it is part of the RAM the program reserves with NEW.

   1K by default. For another size, define both in the program -- a
   program's own modules come before the library, so they win:

     char __sdcc_heap[4096];
     const unsigned int __sdcc_heap_size = sizeof (__sdcc_heap);
-------------------------------------------------------------------------*/

#ifndef HEAP_SIZE
#define HEAP_SIZE 1024
#endif

char __sdcc_heap[HEAP_SIZE];
const unsigned int __sdcc_heap_size = HEAP_SIZE;
