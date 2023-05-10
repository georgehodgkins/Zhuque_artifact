#ifndef _STDATOMIC_ATOMIC_FLAG_H_
#define _STDATOMIC_ATOMIC_FLAG_H_ 1

#include <atomic_constants.h>

#ifndef __GCC_ATOMIC_TEST_AND_SET_TRUEVAL
# define __GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1
#endif

typedef struct atomic_flag atomic_flag;
struct atomic_flag {
	_Bool f;
};

_Bool atomic_flag_test_and_set(volatile atomic_flag*);
_Bool atomic_flag_test_and_set_explicit(volatile atomic_flag*, memory_order);
void atomic_flag_clear(volatile atomic_flag*);
void atomic_flag_clear_explicit(volatile atomic_flag*, memory_order);

#define ATOMIC_FLAG_INIT { .f = 0, }

#define atomic_flag_test_and_set(A) atomic_flag_test_and_set_explicit((A), memory_order_seq_cst)
#define atomic_flag_clear(A) atomic_flag_clear_explicit((A), memory_order_seq_cst)

#ifndef __ATOMIC_FORCE_SYNC
# define atomic_flag_test_and_set_explicit(A, MO)  (__atomic_test_and_set(&((A)->f), MO) == __GCC_ATOMIC_TEST_AND_SET_TRUEVAL)
# define atomic_flag_clear_explicit(A, MO)          __atomic_clear(&(A)->f, MO)
#else
# define atomic_flag_test_and_set_explicit(A, O)                        \
({                                                                      \
  register _Bool atomic_flag_test_and_set_explicit                      \
    = (__sync_lock_test_and_set(&(A)->f, __GCC_ATOMIC_TEST_AND_SET_TRUEVAL) == __GCC_ATOMIC_TEST_AND_SET_TRUEVAL); \
  /* gcc guarantees that this was an acquire operation. */              \
  /* synchronize even stronger if we need to */                         \
  if ((O) == memory_order_seq_cst) __sync_synchronize();                \
  atomic_flag_test_and_set_explicit;                                    \
 })
# define atomic_flag_clear_explicit(A, O)                       \
({                                                              \
  /* gcc guarantees that this will be a release operation. */   \
  /* synchronize even stronger if we need to */                 \
  if ((O) == memory_order_seq_cst) __sync_synchronize();        \
  __sync_lock_release(&(A)->f);                                 \
    })
#endif

#endif
