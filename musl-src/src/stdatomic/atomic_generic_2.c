
#include "stdatomic-impl.h"

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2
INSTANTIATE_STUB_LF(2, uint16_t)
#else
INSTANTIATE_STUB_LC(2, uint16_t)
#endif

INSTANTIATE_SYNC(2, uint16_t)
