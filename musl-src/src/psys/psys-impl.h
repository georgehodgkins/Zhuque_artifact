#ifndef _PSYS_IMPL_H_
#define _PSYS_IMPL_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "psys.h"
#include "errno-report.h"

#include <fcntl.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>

#include "stdatomic-impl.h"
#include "syscall.h"

#define MAX_THREADS          72
#define PMEM_STACK_SIZE (PAGE_SIZE*8)

//#define USE_VSTACK 1

#include "pfile-impl.h"
#include "pheap-impl.h"

typedef void (*simple_handler) (int); // user failure handler typedef
	
typedef struct {
	uint8_t fxSaveArea[512]; // FP, SSE, and MMX state
	uint64_t* gpRegs[8]; // callee-saved GP registers and RFLAGS
	sigset_t sigmask; // thread signal mask
} RestoreData __attribute__((aligned (16)));

typedef struct {
	pid_t ltid; // Linux TID
	pthread_t ptid; // Pthreads TID
	bool incrit;

	// per-thread heaps
	vmtab_entry_t* brkent;
	void* curbrk;
	vmtab_entry_t* vbrkent;
	void* vcurbrk;

	// register file
	RestoreData *restore;

} ThreadContext;

#ifdef DOPT_VSTACK
void pmap_setup_vstack(ThreadContext*);
#endif

// TODO: use LL for thread contexts
typedef struct {
	pid_t pid; // Linux PID
	uintptr_t canary; // value used for stack protection
	atomic_uint num_ctx;
	atomic_uint_least64_t exit_count;	
	simple_handler UserHandler;

	// heap info
	atomic_int hides, lodes;
	int heap_dir;
	vmtab_entry_t vmtab[VMTAB_SIZE];
	vmtab_entry_t* brkent;
	void* curbrk;

	// saved loader metadata
	struct dso *dso_head, *dso_tail, *dso_syms_tail,
		*dso_fini_head, *dso_lazy_head, *dso_head_next;

	// reference counting for heap descriptors
	atomic_uint desref[VMTAB_SIZE];

	// file descriptor table
	fd_entry_t fdtab[FDTAB_SIZE];
	atomic_int hifd; // largest registered fd
	// reference counting for mapped files 
	atomic_uint next_mrtab;
	mapref_t mrtab[VMTAB_SIZE];
	// table mapping device ids to fds for their .nvmhlink dir
	atomic_uint next_d2h;
	dev2hlink_t dev2hlink[D2HTAB_SIZE];

	pthread_key_t tctx_key;
	ThreadContext ThreadSet[MAX_THREADS];
#ifdef TIME_PSYS
	int timedat_fd;
#endif
} ProcessContext;

#define _FAAI_DET_ 0x1 // has faai_startup() been called?
#define _FAAI_ACTIVE_ 0x2 // should we do faai stuff?
#define _FAAI_FRESH_ 0x4 // is this a fresh start?
#define _FAAI_EXITING_ 0x8 // has the failure handler or destructor been called?
extern atomic_uint pStat;

#include <dlfcn.h>
extern ProcessContext* ProcessCtx;
ThreadContext* myContext(void);

unsigned faai_get_random(void);

void faai_startcrit(void);
void faai_endcrit(void);

// open/close without recording fds
// use these instead of _p_* if file will be closed before control
// returns to user code
// openat with AT_FDCWD as first arg is equivalent to open
int _v_openat(int, const char*, int, ...);
int _v_close(int);

void* _v_mmap(void*, size_t, int, int, int, off_t);

int __pthread_create_impl(pthread_t *restrict res, const pthread_attr_t *restrict attrp,
		void *(*entry)(void *), void *restrict arg, bool);

// setjmp/longjmp replacements for failure/restoration
extern void __attribute__((noreturn, naked))
piglongjmp(uint64_t**, sigset_t*, uint8_t*, ThreadContext*);

extern ThreadContext* pigsetjmp(uint64_t**, sigset_t*, uint8_t*, atomic_uint_least64_t*);

void pmem_meta_is_valid(void);
void fdtab_is_valid(void);

#endif // _PSYS_IMPL_H_
