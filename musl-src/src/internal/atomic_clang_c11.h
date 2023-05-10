#ifndef _STDATOMIC_CLANG_C11_H_
#define _STDATOMIC_CLANG_C11_H_ 1

#include <atomic_stub.h>

#define ATOMIC_VAR_INIT(...) __VA_ARGS__
#define atomic_init __c11_atomic_init

/* Map operations to the special builtins that clang provides. */

/* Map all non-explicit macros to the builtin with forced memory order. */
#define atomic_fetch_add(X, Y)                  __c11_atomic_fetch_add((X), (Y), memory_order_seq_cst)
#define atomic_fetch_sub(X, Y)                  __c11_atomic_fetch_sub((X), (Y), memory_order_seq_cst)
#define atomic_fetch_and(X, Y)                  __c11_atomic_fetch_and((X), (Y), memory_order_seq_cst)
#define atomic_fetch_or(X, Y)                   __c11_atomic_fetch_or((X), (Y), memory_order_seq_cst)
#define atomic_fetch_xor(X, Y)                  __c11_atomic_fetch_xor((X), (Y), memory_order_seq_cst)
#define atomic_load(X)                          __c11_atomic_load((X), memory_order_seq_cst)
#define atomic_store(X, V)                      __c11_atomic_store((X), (V), memory_order_seq_cst)
#define atomic_exchange(X, V)                   __c11_atomic_exchange((X), (V), memory_order_seq_cst)
#define atomic_compare_exchange_weak(X, E, V)   __c11_atomic_compare_exchange_weak((X), (E), (V), memory_order_seq_cst, memory_order_seq_cst)
#define atomic_compare_exchange_strong(X, E, V) __c11_atomic_compare_exchange_strong((X), (E), (V), memory_order_seq_cst, memory_order_seq_cst)

/* Map allexplicit macros to the corresponding builtin. */
#define atomic_fetch_add_explicit                                  __c11_atomic_fetch_add
#define atomic_fetch_sub_explicit                                  __c11_atomic_fetch_sub
#define atomic_fetch_and_explicit                                  __c11_atomic_fetch_and
#define atomic_fetch_or_explicit                                   __c11_atomic_fetch_or
#define atomic_fetch_xor_explicit                                  __c11_atomic_fetch_xor
#define atomic_load_explicit                                       __c11_atomic_load
#define atomic_store_explicit                                      __c11_atomic_store
#define atomic_exchange_explicit                                   __c11_atomic_exchange
#define atomic_compare_exchange_strong_explicit                    __c11_atomic_compare_exchange_strong
#define atomic_compare_exchange_weak_explicit                      __c11_atomic_compare_exchange_weak

#define INSTANTIATE_STUB_LF(N, T)                                       \
T __impl_fetch_add_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_add(_X, _V, _mo);                           \
}                                                                       \
T __impl_fetch_sub_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_sub(_X, _V, _mo);                           \
}                                                                       \
T __impl_fetch_and_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_and(_X, _V, _mo);                           \
}                                                                       \
T __impl_fetch_xor_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_xor(_X, _V, _mo);                           \
}                                                                       \
T __impl_fetch_or_ ## N(_Atomic(T)* _X, T const _V, int _mo) {          \
  return __c11_atomic_fetch_or(_X, _V, _mo);                            \
}                                                                       \
T __impl_add_fetch_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_add(_X, _V, _mo) + _V;                      \
}                                                                       \
T __impl_sub_fetch_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_sub(_X, _V, _mo) - _V;                      \
}                                                                       \
T __impl_and_fetch_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_and(_X, _V, _mo) & _V;                      \
}                                                                       \
T __impl_xor_fetch_ ## N(_Atomic(T)* _X, T const _V, int _mo) {         \
  return __c11_atomic_fetch_xor(_X, _V, _mo) ^ _V;                      \
}                                                                       \
T __impl_or_fetch_ ## N(_Atomic(T)* _X, T const _V, int _mo) {          \
  return __c11_atomic_fetch_or(_X, _V, _mo) | _V;                       \
}                                                                       \
T __impl_load_ ## N(_Atomic(T)* _X, int _mo) {                          \
  return __c11_atomic_load(_X, _mo);                                    \
}                                                                       \
void __impl_store_ ## N(_Atomic(T)* _X, T const _V, int _mo) {          \
  __c11_atomic_store(_X, _V, _mo);                                      \
}                                                                       \
T __impl_exchange_ ## N(_Atomic(T)* _X, T const _V, int _mo) {          \
  return __c11_atomic_exchange(_X, _V, _mo);                            \
}                                                                       \
_Bool __impl_compare_exchange_ ## N(_Atomic(T)* _X, T* _E, T const _V, int _mos, int _mof) { \
  return __c11_atomic_compare_exchange_strong(_X, _E, _V, _mos, _mof);  \
}                                                                       \
 INSTANTIATE_STUB_NAND(N, T)

#define INSTANTIATE_STUB(N, T) INSTANTIATE_STUB_ ## N(T)

#endif
