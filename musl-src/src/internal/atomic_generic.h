#ifndef _STDATOMIC_ATOMIC_GENERIC_H_
#define _STDATOMIC_ATOMIC_GENERIC_H_ 1

void __impl_load (size_t size, void volatile* ptr, void volatile* ret, int mo);
void __impl_store (size_t size, void volatile* ptr, void const volatile* val, int mo);
void __impl_exchange (size_t size, void volatile*__restrict__ ptr, void const volatile* val, void volatile* ret, int mo);
_Bool __impl_compare_exchange (size_t size, void volatile* ptr, void volatile* expected, void const volatile* desired, int mos, int mof);
void __impl_print_stat(void);

#endif
