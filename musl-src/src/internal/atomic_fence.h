#ifndef _STDATOMIC_ATOMIC_FENCE_H_
#define _STDATOMIC_ATOMIC_FENCE_H_ 1

#include <atomic_constants.h>


void atomic_thread_fence(memory_order mo);

void atomic_signal_fence(memory_order mo);

#define kill_dependency(X)                      \
({                                              \
  register __typeof__(X) kill_dependency = (X); \
  kill_dependency;                              \
 })

#ifndef __ATOMIC_FORCE_SYNC
# define atomic_thread_fence(MO) __atomic_thread_fence(MO)
# define atomic_signal_fence(MO) __atomic_signal_fence(MO)
#else
# define atomic_thread_fence(MO)                        \
({                                                      \
  if (MO != memory_order_relaxed) __sync_synchronize(); \
  else __asm__ volatile("# relaxed fence");             \
 })
#define atomic_signal_fence(MO) __asm__ volatile("# signal fence")
#endif

#endif
