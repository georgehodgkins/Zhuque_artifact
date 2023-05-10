#include <sys/syscall.h>
.intel_syntax noprefix
.global pigsetjmp
.type pigsetjmp,@function
# rdi = regs, rsi = sigmask, rdx = fxsave area, rcx = exit counter
# TODO: this could be better written
pigsetjmp:
	# save FP, MMX, & SSE state
	fxsave [rdx]
	# save RFLAGS
	pushfq
	pop rax
	movq [rdi], rax # gpRegs[0] = RFLAGS
	# save GP regs
	movq [rdi + 8*1], rbx # gpRegs[1] = rbx
	movq [rdi + 8*2], rsp # gpRegs[2] = rsp 
	movq [rdi + 8*3], r12 # gpRegs[3] = r12
	movq [rdi + 8*4], r13 # gpRegs[4] = r13
	movq [rdi + 8*5], r14 # gpRegs[5] = r14
	movq [rdi + 8*6], r15 # gpRegs[6] = r15
	movq [rdi + 8*7], rbp # gpRegs[7] = rbp
	# save exit counter addr
	movq r15, rcx
	# save signal mask
	movq rax, SYS_rt_sigprocmask # syscall number
	movq rdi, 0 # syscall op (no-op)
	movq rdx, rsi # mask save location
	movq rsi, 0 # new mask (not changing mask)
	movq r10, 8 # size of sigset_t in bytes
	syscall
	# increment exit counter
	sfence
	lock addq [r15], 1
	# exit directly from here
	movq rdi, 85 # exit code (ERESTART)
	movq rax, SYS_exit
	syscall
	hlt # should be unreachable
