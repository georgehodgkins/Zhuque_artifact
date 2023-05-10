
#include "stdatomic-impl.h"

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1
INSTANTIATE_STUB_LF(1, uint8_t)
#else
INSTANTIATE_STUB_LC(1, uint8_t)
#endif

INSTANTIATE_SYNC(1, uint8_t)
