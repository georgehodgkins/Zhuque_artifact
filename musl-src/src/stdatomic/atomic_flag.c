#include "atomic_flag.h"

_Bool (atomic_flag_test_and_set)(volatile atomic_flag* f) {
	return atomic_flag_test_and_set(f);
}

_Bool (atomic_flag_test_and_set_explicit)(volatile atomic_flag* f, memory_order mo) {
	return atomic_flag_test_and_set_explicit(f, mo);
}

void (atomic_flag_clear)(volatile atomic_flag* f) {
	atomic_flag_clear(f);
}

void (atomic_flag_clear_explicit)(volatile atomic_flag* f, memory_order mo) {
	atomic_flag_clear_explicit(f, mo);
}
