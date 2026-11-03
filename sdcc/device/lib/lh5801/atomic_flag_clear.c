/* atomic_flag_clear.c - the external definition of stdatomic.h's inline
   atomic_flag_clear() for the LH5801 port (as device/lib/atomic_flag_clear.c
   for the ports that build the generic one): a single store of 0, atomic
   as it is. atomic_flag_test_and_set is atomic_flag_test_and_set.asm. */

#define __SDCC_ATOMIC_EXTERN extern

#include <stdatomic.h>
