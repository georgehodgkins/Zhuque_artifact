#ifndef _STDATOMIC_STUB_H_
#define _STDATOMIC_STUB_H_ 1

#include <atomic_generic.h>

#define INSTANTIATE_STUB_LCA(N, T)                                      \
T __impl_fetch_add_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = V;                                                              \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E + V;                                                          \
  }                                                                     \
  return E;                                                             \
}                                                                       \
T __impl_fetch_sub_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = -V;                                                             \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E - V;                                                          \
  }                                                                     \
  return E;                                                             \
}                                                                       \
T __impl_fetch_and_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = 0;                                                              \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E & V;                                                          \
  }                                                                     \
  return E;                                                             \
}                                                                       \
T __impl_fetch_xor_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = V;                                                              \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E ^ V;                                                          \
  }                                                                     \
  return E;                                                             \
}                                                                       \
T __impl_fetch_or_ ## N(void volatile* X, T const V, int MO) {          \
  T E = 0;                                                              \
  T R = V;                                                              \
  int mof = MO == memory_order_relaxed ? memory_order_relaxed : memory_order_consume; \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E | V;                                                          \
  }                                                                     \
  return E;                                                             \
}                                                                       \
T __impl_add_fetch_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = V;                                                              \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E + V;                                                          \
  }                                                                     \
  return R;                                                             \
}                                                                       \
T __impl_sub_fetch_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = -V;                                                             \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E - V;                                                          \
  }                                                                     \
  return R;                                                             \
}                                                                       \
T __impl_and_fetch_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = 0;                                                              \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E & V;                                                          \
  }                                                                     \
  return R;                                                             \
}                                                                       \
T __impl_xor_fetch_ ## N(void volatile* X, T const V, int MO) {         \
  T E = 0;                                                              \
  T R = V;                                                              \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E ^ V;                                                          \
  }                                                                     \
  return R;                                                             \
}                                                                       \
T __impl_or_fetch_ ## N(void volatile* X, T const V, int MO) {          \
  T E = 0;                                                              \
  T R = V;                                                              \
  int mof = MO == memory_order_relaxed ? memory_order_relaxed : memory_order_consume; \
  while (!__impl_compare_exchange(N, X, &E, &R, MO, mof)){              \
    R = E | V;                                                          \
  }                                                                     \
  return R;                                                             \
}

#define INSTANTIATE_STUB_NAND(N, T)                                     \
T __impl_fetch_nand_ ## N(void volatile* X, T const V, int MO) {        \
  T E = 0;                                                              \
  T R = ~0;                                                             \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!atomic_compare_exchange_strong_explicit((_Atomic(T)*)X, &E, R, MO, mof)){ \
    R = ~(E & V);                                                       \
  }                                                                     \
  return E;                                                             \
}                                                                       \
T __impl_nand_fetch_ ## N(void volatile* X, T const V, int MO) {        \
  T E = 0;                                                              \
  T R = ~E;                                                             \
  int mof = (MO == memory_order_relaxed                                 \
             ? memory_order_relaxed                                     \
             : memory_order_consume);                                   \
  while (!atomic_compare_exchange_strong_explicit((_Atomic(T)*)X, &E, R, MO, mof)){ \
    R = ~(E & V);                                                       \
  }                                                                     \
  return R;                                                             \
}


#define INSTANTIATE_STUB_LCM(N, T)                                      \
T __impl_load_ ## N(void volatile* X, int MO) {                         \
  T ret;                                                                \
  __impl_load(N, X, &ret, MO);                                          \
  return ret;                                                           \
}                                                                       \
void __impl_store_ ## N(void volatile* X, T const V, int MO) {          \
  __impl_store(N, X, &V, MO);                                           \
}                                                                       \
T __impl_exchange_ ## N(void volatile* X, T const V, int MO) {          \
  T ret;                                                                \
  __impl_exchange(N, X, &V, &ret, MO);                                  \
  return ret;                                                           \
}                                                                       \
_Bool __impl_compare_exchange_ ## N(void volatile* X, T* E, T const V, int MOS, int MOf) { \
  return __impl_compare_exchange(N, X, E, &V, MOS, MOf);                \
}                                                                       \
 INSTANTIATE_STUB_NAND(N, T)

#define INSTANTIATE_STUB_LC(N, T) INSTANTIATE_STUB_LCA(N, T) INSTANTIATE_STUB_LCM(N, T)


#define INSTANTIATE_SYNCA(N, T)                                         \
T __impl_fetch_and_add_ ## N(void volatile* X, T const V) {             \
  return __impl_fetch_add_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_fetch_and_sub_ ## N(void volatile* X, T const V) {             \
  return __impl_fetch_sub_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_fetch_and_and_ ## N(void volatile* X, T const V) {             \
  return __impl_fetch_and_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_fetch_and_or_ ## N(void volatile* X, T const V) {              \
  return __impl_fetch_or_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_fetch_and_xor_ ## N(void volatile* X, T const V) {             \
  return __impl_fetch_xor_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_add_and_fetch_ ## N(void volatile* X, T const V) {             \
  return __impl_add_fetch_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_sub_and_fetch_ ## N(void volatile* X, T const V) {             \
  return __impl_sub_fetch_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_and_and_fetch_ ## N(void volatile* X, T const V) {             \
  return __impl_and_fetch_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_or_and_fetch_ ## N(void volatile* X, T const V) {              \
  return __impl_or_fetch_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}                                                                       \
T __impl_xor_and_fetch_ ## N(void volatile* X, T const V) {             \
  return __impl_xor_fetch_ ## N((_Atomic(T)*)X, V, memory_order_seq_cst); \
}

#define INSTANTIATE_SYNCM(N, T)                                         \
_Bool __impl_bool_compare_and_swap_ ## N(void volatile* X, T E, T const V) { \
  T R = E;                                                              \
  return __impl_compare_exchange_ ## N((_Atomic(T)*)X, &R, V,           \
                                                      memory_order_seq_cst, memory_order_seq_cst); \
}                                                                       \
T __impl_val_compare_and_swap_ ## N(void volatile* X, T E, T const V) { \
   T R = E;                                                             \
  __impl_compare_exchange_ ## N((_Atomic(T)*)X, &R, V,                  \
                                               memory_order_seq_cst, memory_order_seq_cst); \
  return R;                                                             \
}

#define INSTANTIATE_SYNC(N, T) INSTANTIATE_SYNCA(N, T) INSTANTIATE_SYNCM(N, T)

#endif
