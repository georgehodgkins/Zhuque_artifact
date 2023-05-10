#include "atomic_fence.h"

void (atomic_thread_fence)(memory_order mo) {
	atomic_thread_fence(mo);
}

void (atomic_signal_fence)(memory_order mo) {
	atomic_signal_fence(mo);
}
