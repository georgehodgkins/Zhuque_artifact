
#include "stdatomic-impl.h"

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
INSTANTIATE_STUB_LF(4, uint32_t)
#else
INSTANTIATE_STUB_LC(4, uint32_t)
#endif

INSTANTIATE_SYNC(4, uint32_t)
