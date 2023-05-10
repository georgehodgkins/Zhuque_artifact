#include <sys/syscall.h>
# because signal.h is not asm guarded:
#define SIG_SETMASK 2
.intel_syntax noprefix
.global piglongjmp
.type piglongjmp,@function
# rdi = regs, rsi = sigmask, rdx = fxsave area, rcx = ctx pointer
piglongjmp:
	# restore RFLAGS without corrupting stack
	pop rbx
	push [rdi]
	popfq
	push rbx
	# save syscall clobbers
	movq rbx, rax
	movq r12, rdi
	movq r13, rcx
	movq r14, rdx
	movq r15, r10
	# restore signal mask w/ inline syscall
	movq rax, SYS_rt_sigprocmask # syscall number
	movq rdi, SIG_SETMASK # syscall op
	# rsi is already new mask
	movq rdx, 0 # old mask pointer (not needed)
	movq r10, 8 # size of sigset_t in bytes
	syscall
	# restore clobbered registers
	movq rax, rbx
	movq rdi, r12
	movq rcx, r13
	movq rdx, r14
	movq r10, r15
	mfence
	# restore FP, MMX, & SSE state
	fxrstor [rdx]
	# restore GP regs
	movq rbx, [rdi + 8*1]
	movq rsp, [rdi + 8*2]
	movq r12, [rdi + 8*3]
	movq r13, [rdi + 8*4]
	movq r14, [rdi + 8*5]
	movq r15, [rdi + 8*6]
	# set return value to context addr and return to pigsetjmp point
	movq rbp, [rdi + 8*7] // stack frame
	movq rax, rcx
	ret
