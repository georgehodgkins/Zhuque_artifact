#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <syscall.h>
#include <linux/prctl.h>
#include <sys/mman.h>

/* This is a basic test for the kernel "pigframe" feature.
 * It allocates a pigframe in anonymous memory, activates it,
 * populates the registers with random values, and performs
 * a pure syscall (sync). Then it prints the contents of the
 * pigframe next to the random values. All registers should match
 * exactly except for rip and rsp, which should be pretty close.
 * 
 * On a kernel without pigframe, this will fail with EINVAL at the
 * first prctl() call.
 */

#define urand() ((uint64_t) rand())
struct kpigcontext {
	__u64				r15;
	__u64				r14;
	__u64				r13;
	__u64				r12;
	__u64				rbp;
	__u64				rbx;
	__u64				r11;
	__u64				r10;
	__u64				r9;
	__u64				r8;
	__u64				rax;
	__u64				rcx;
	__u64				rdx;
	__u64				rsi;
	__u64				rdi;
	__u64				orig_blank;
	__u64				rip;
	__u64				cs;		/* RFLAGS */
	__u64				flags;
	__u64				rsp;
} test_ctx, pig_ctx;

#define PR_SET_PIGFRAME 54
#define PR_GET_PIGFRAME 55

int main () {
	srand(time(NULL));
	// set up and test the pigframe
	volatile void* pig = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
		MAP_ANON | MAP_PRIVATE, -1, 0);
	assert(pig != MAP_FAILED);
	int s = mlock((void*)pig, 4096);
	assert(s == 0);
	s = syscall(SYS_prctl, PR_SET_PIGFRAME, (void*)pig);
	assert(s == 0);
	void* wilbur;
	s = syscall(SYS_prctl, PR_GET_PIGFRAME, &wilbur);
	assert(s == 0);
	assert(wilbur == pig);

	// gen random values for all unused regs (not rbp, rsp, rip)
	test_ctx.r8 = urand();
	test_ctx.r9 = urand();
	test_ctx.r10 = urand();
	// r11 clobbered by syscall (flags)
	test_ctx.r12 = urand();
	test_ctx.r13 = urand();
	test_ctx.r14 = urand();
	test_ctx.r15 = (uint64_t) &test_ctx;
	test_ctx.rdi = urand();
	test_ctx.rsi = urand();
	test_ctx.rbx = urand();
	test_ctx.rdx = urand();
	test_ctx.cs = 0xdeadbeef;
	test_ctx.rax = 162; // sync() - pure syscall not in vdso (and also not fork lol)
	test_ctx.rsp = (__u64) &pig; // approximately rsp
	// rcx clobbered by syscall (return addr)

	register struct kpigcontext* ctptr asm ("r15") = &test_ctx;
	
	// copy random context to registers, then do syscall
	asm volatile (
		// r15 is already the ctx addr
		"movq r14, [%0 + 8*1]\n"
		"movq r13, [%0 + 8*2]\n"
		"movq r12, [%0 + 8*3]\n"
		"movq [%0 + 8*4], rbp\n"
		"movq rbx, [%0 + 8*5]\n"
		"movq r11, [%0 + 8*6]\n"
		"movq r10, [%0 + 8*7]\n"
		"movq r9, [%0 + 8*8]\n"
		"movq r8, [%0 + 8*9]\n"
		"movq rax, [%0 + 8*10]\n" // syscall nr
		"movq rcx, [%0 + 8*11]\n"
		"movq rdx, [%0 + 8*12]\n"
		"movq rsi, [%0 + 8*13]\n"
		"pushfq \n" // flags
		"popq rdi\n"
		"movq [%0 + 8*18], rdi\n"
		"movq rdi, [%0 + 8*14]\n"
		"syscall \n"
		"lea rdi, [rip]\n" // this will be one insn off from the addr stored by syscall
		"movq [%0 + 8*16], rdi\n"
		"mfence\n"
		:: "or" (ctptr) 
		: "r8" , "r9", "r10", "r11", "r12", "r13", "r14", "rsi", 
			"rdi", "rbx", "rdx", "rax", "rcx", "memory"
	);

	// copy context to buffer for stability
	memcpy(&pig_ctx, pig, sizeof(struct kpigcontext));

#define pregcomp(reg) printf(#reg"\t0x%llx\t\t0x%llx\n", test_ctx.reg, pig_ctx.reg)
	printf("reg\tgen\t\tframe\n");
	pregcomp(r8);
	pregcomp(r9);
	pregcomp(r10);
	pregcomp(r12);
	pregcomp(r13);
	pregcomp(r14);
	pregcomp(r15);
	pregcomp(rdi);
	pregcomp(rsi);
	pregcomp(rax);
	pregcomp(rbx);
	pregcomp(rdx);
	pregcomp(rbp);
	pregcomp(rsp);
	pregcomp(rip);
	printf("~rip\t%p\t\t%llx\n", &main, pig_ctx.rip);
	printf("flag\t%llx\t\t%llx\n", test_ctx.flags, pig_ctx.flags);
	fflush(stdout);

	return 0;
}
