
#include "stdatomic-impl.h"

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
INSTANTIATE_STUB_LF(8, uint64_t)
#else
INSTANTIATE_STUB_LC(8, uint64_t)
#endif

INSTANTIATE_SYNC(8, uint64_t)
