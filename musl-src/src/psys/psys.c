#include "psys-impl.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>

#include <unistd.h>
//#include <bits/syscall.h>
#include <sys/mman.h>
#include <math.h>

#include "pthread_impl.h"

#pragma GCC diagnostic ignored "-Wpointer-arith"

#ifdef TIME_PSYS
FILE* __timingdat = NULL;
char* __timingact = NULL;
extern FILE* fmemopen_inplace(void* restrict, size_t, const char* restrict);

#define TIMING_FILE "timing.csv"
#define TIMING_EARLY_BUFSIZE PAGE_SIZE*4

#define TIMER_INIT_EARLY() \
	void* __timingdat_buf; \
	do { \
		__timingact = getenv("TIME_PSYS"); \
		if (__timingact) { \
			__timingdat_buf = _v_mmap(NULL, TIMING_EARLY_BUFSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);\
			assert(__timingdat_buf != MAP_FAILED); \
			ERRNO_REPORT_NZ(__timingdat, fmemopen_inplace, &__timingdat_buf, TIMING_EARLY_BUFSIZE, "w"); \
			ERRNO_REPORT_Z_NR(setvbuf, __timingdat, NULL, _IONBF, 0); \
		} \
	} while (0)

#define TIMER_INIT_COMPLETE() \
	do { if (__timingact) { \
			ERRNO_REPORT_Z_NR(fclose, __timingdat); \
			if (st & _FAAI_FRESH_) { \
				ERRNO_REPORT_NZ(__timingdat, fopen, TIMING_FILE, "a"); \
			} else { \
				int fd; \
				ERRNO_REPORT_GEZ(fd, open, TIMING_FILE, O_WRONLY | O_APPEND); \
				ERRNO_REPORT_GEZ_NR(dup2, fd, ProcessCtx->timedat_fd); \
				ERRNO_REPORT_Z_NR(close, fd); \
				ERRNO_REPORT_NZ(__timingdat, fdopen, ProcessCtx->timedat_fd, "a"); \
			} \
			ERRNO_REPORT_GEZ_NR(fputs, __timingdat_buf, __timingdat); \
			__timingdat_buf = (void*) ((uint64_t) __timingdat_buf & ~(PAGE_SIZE-1)); \
			ERRNO_REPORT_Z_NR(_v_munmap, __timingdat_buf, TIMING_EARLY_BUFSIZE); \
		} } while (0) 

#else

#define TIMER_INIT_EARLY()
#define TIMER_INIT_COMPLETE()

#endif

__attribute__((target("sse4")))
static uint32_t crc32 (void* data_v, const unsigned words) {
	uint32_t* data = data_v;
	uint32_t chk = 0;
	for (unsigned i = 0; i < words; ++i) {
		chk = __builtin_ia32_crc32si(chk, *(data++));
	}

	return chk;
}

ProcessContext* ProcessCtx;
atomic_uint next_ctx;
pthread_barrier_t faai_restore_barrier;

atomic_uint pStat = 0;

static pthread_barrier_t fake_sigpwr_barrier;
static double fake_sigpwr_delay = 0.0;
void* fake_sigpwr_thread (void* unused) {
	assert(fake_sigpwr_delay != 0.0);
	double s, ns;
	ns = 1e9*modf(fake_sigpwr_delay, &s);
	struct timespec delay = {(time_t) s, (long) ns};
	pthread_barrier_wait(&fake_sigpwr_barrier);
	int r;
	do {
		r = nanosleep(&delay, &delay);
	} while (r == -1 && errno == EINTR);
	assert(r == 0);
	printf("\n\n*****FAKE SIGPWR AFTER %.0f S: %.0f NS*****\n\n", s, ns);
	fflush(stdout);
	kill(0, SIGPWR);
	return NULL;
}

__attribute__((target("rdrnd")))
unsigned faai_get_random (void) {
	unsigned rand;
	__builtin_ia32_rdrand32_step(&rand);
	return rand;
}

// NOTE: in musl, TLS does not work in signal handlers, so this function doesn't work there
ThreadContext* myContext (void) {
	ThreadContext* ctx = (ThreadContext*) pthread_getspecific(ProcessCtx->tctx_key);
	return ctx;
}

// critical section primitives (delay signal handling in inconvenient places)
void faai_startcrit(void) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		assert(!myContext() || !myContext()->incrit && "No nesting!");
		sigset_t blok;
		sigemptyset(&blok);
		sigaddset(&blok, SIGUSR2);
		ERRNO_REPORT_Z_NR(pthread_sigmask, SIG_BLOCK, &blok, NULL);
		if (myContext()) myContext()->incrit = true;
	}
}

void faai_endcrit(void) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		assert(!myContext() || myContext()->incrit && "Not in critical section!");
		sigset_t unblok;
		sigemptyset(&unblok);
		sigaddset(&unblok, SIGUSR2);
		ERRNO_REPORT_Z_NR(pthread_sigmask, SIG_UNBLOCK, &unblok, NULL);
		if (myContext()) myContext()->incrit = false;
	}
}

/*
 * We use two handlers to save context at a power
 * failure. The first catches SIGPWR, then propagates
 * it to all threads via SIGUSR2.
 */

// sets user-defined power failure handler
void faai_register_handler(simple_handler h) {
	assert(ProcessCtx->UserHandler == NULL);
	ProcessCtx->UserHandler = h;
}
	
// Debug code to make sure structs are intact
// mostly defunct
void faai_validate_internal (void) {
	assert(kill(ProcessCtx->pid, 0) == 0 && "Invalid PID!");
	//pmem_meta_is_valid(); // check heap cursor and brk
	
	fdtab_is_valid(); // check FDs

	// check thread contexts
	unsigned ctx_count = 0;
	for (int t = 0; t < MAX_THREADS; ++t) {
		if (ProcessCtx->ThreadSet[t].restore) {
			++ctx_count;
			// check rsp and rbp
			/* this check could be done using the TCB
			assert(ProcessCtx->ThreadSet[t].restore->gpRegs[2] > ProcessCtx->ThreadSet[t].stackAddr &&
					ProcessCtx->ThreadSet[t].restore->gpRegs[2] <
					ProcessCtx->ThreadSet[t].stackAddr + ProcessCtx->ThreadSet[t].stackSize/sizeof(uint64_t));
			assert(ProcessCtx->ThreadSet[t].restore->gpRegs[7] > ProcessCtx->ThreadSet[t].stackAddr &&
					ProcessCtx->ThreadSet[t].restore->gpRegs[7] < 
					ProcessCtx->ThreadSet[t].stackAddr + ProcessCtx->ThreadSet[t].stackSize/sizeof(uint64_t));
			*/
		}
	}
	assert(ctx_count == atomic_load(&ProcessCtx->num_ctx));
}


// Signal propagating handler (attached to idle master thread)
void faai_sigpwr_handler (int signum) {
	if (!(atomic_fetch_or(&pStat, _FAAI_EXITING_) & _FAAI_EXITING_)) { // avoid exit race
		assert(signum == SIGPWR);
		TIMER_START();
		atomic_store(&ProcessCtx->exit_count, 0);
		// send SIGUSR2 to all live threads
		for (unsigned t = 0; t < MAX_THREADS; ++t) {
			siginfo_t info; // struct to pass context address (TLS doesn't work in handlers?)
			info.si_code = SI_QUEUE;
			info.si_pid = getpid();
			info.si_uid = getuid();
			if (ProcessCtx->ThreadSet[t].restore != NULL) {
				info.si_value = (union sigval) (void*) &ProcessCtx->ThreadSet[t];
				int s = syscall(SYS_rt_tgsigqueueinfo, getpid(),
						ProcessCtx->ThreadSet[t].ltid, SIGUSR2, &info);
				assert(s == 0);
			}
		}
		// save file descriptor state
		save_fd_tracking();
		// wait for all threads to exit
		while(atomic_load(&ProcessCtx->exit_count) < atomic_load(&ProcessCtx->num_ctx));
#ifndef REAL_PMEM
		ERRNO_REPORT_Z_NR(msync, ProcessCtx, sizeof(ProcessCtx), MS_SYNC);
		pmap_sync_heaps();
#endif
		TIMER_STOP("process save");
#ifdef TIME_PSYS
		ProcessCtx->timedat_fd = fileno(__timingdat);
		if (__timingact)
			ERRNO_REPORT_Z_NR(fflush, __timingdat);
#endif
#ifndef NDEBUG
		faai_validate_internal();
#endif
		// terminate without cleanup
		_exit(85);
	}
}

// Context saving handler
#pragma GCC diagnostic push // preserve pointer arith
#pragma GCC diagnostic ignored "-Wunused-parameter" // sometimes werror is stupid
void faai_sigusr2_handler (int signum, siginfo_t* info, void* ucontext) {
#pragma GCC diagnostic pop
	assert(signum == SIGUSR2);
	ThreadContext* ctx = (ThreadContext*) info->si_ptr;
	ctx = pigsetjmp(ctx->restore->gpRegs, &ctx->restore->sigmask, ctx->restore->fxSaveArea,
			&ProcessCtx->exit_count);
	// pigsetjmp never returns when called from here -- exits directly to preserve stack
	// -----RESTORATION POINT----- (well, technically it's the return from "pigsetjmp", but whatever)
	// set new context
	pthread_setspecific(ProcessCtx->tctx_key, ctx);
	
	// Update volatile tids
	assert(ctx->ptid == pthread_self());
	ctx->ltid = syscall(SYS_gettid, 0);
	
	// recreate volatile signal stack
	setup_alt_stack();

	// run user handler if registered
	if (ProcessCtx->UserHandler != NULL)
		ProcessCtx->UserHandler(SIGPWR);

	// wait for everyone to finish initialization
	pthread_barrier_wait(&faai_restore_barrier);
}

#ifdef USE_PIGFRAME

// easier to define these here than build against the custom headers
#define PR_SET_PIGFRAME 54
#define PR_GET_PIGFRAME 55

#define PIGFRAME_DEV "/dev/dax"
#define PIGFRAME_SIZE 0x1000 // devdax alignment; the actual size of the pigframe is ~150B, but we need a page
#define PIGFRAME_DEV_SIZE 0x179D00000 // total number of bytes in the devdax available for frames
#define PIGFRAME_MAP_FLAGS (MAP_SHARED)

static void setup_pigframe(int idx) {
#ifdef REAL_PMEM // use devdax for pigframe
	off_t mypig = idx * PIGFRAME_SIZE;
	assert(mypig >= 0 && mypig < PIGFRAME_DEV_SIZE);
	int pigfd;
	ERRNO_REPORT_GEZ(pigfd, open, PIGFRAME_DEV, O_RDWR);
	void* pigframe = _v_mmap(NULL, PIGFRAME_SIZE, PROT_READ | PROT_WRITE,
		PIGFRAME_MAP_FLAGS, pigfd, mypig);
	assert(pigframe != MAP_FAILED);
	ERRNO_REPORT_Z_NR(close, pigfd);
#else // !REAL_PMEM, use DRAM for pigframe
	void* pigframe = _v_mmap(NULL, PIGFRAME_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, -1, 0);
	assert(pigframe != MAP_FAILED);
#endif
	*(uint64_t*) pigframe = 0; // ensure page is present
	ERRNO_REPORT_Z_NR(mlock, pigframe, PIGFRAME_SIZE);
	ERRNO_REPORT_Z_NR(syscall, SYS_prctl, PR_SET_PIGFRAME, pigframe);
}

#else // !USE_PIGFRAME
#define setup_pigframe(...)
#endif
	
#define ALIGN_UP(x, m) (x + (m - x%m))
static void create_thread_context(void) {
	int ctx_ind;
	unsigned loopcnt = 0;
	do { // once we use all array entries once, just search for the first open block
		ctx_ind = atomic_fetch_add(&next_ctx, 1);
		++loopcnt;
	} while (ProcessCtx->ThreadSet[ctx_ind % MAX_THREADS].restore != NULL && loopcnt < MAX_THREADS);
	assert(loopcnt < MAX_THREADS && "Maximum thread count exceeded!");
	ThreadContext* ctx = &ProcessCtx->ThreadSet[ctx_ind % MAX_THREADS];

	ctx->ltid = syscall(SYS_gettid, 0);
	ctx->ptid = pthread_self();

	setup_pigframe(ctx_ind % MAX_THREADS);

	// fxsave buf must be 16B aligned
	//ERRNO_REPORT_Z_NR(posix_memalign, (void**) &ctx->restore, 16, ALIGN_UP(sizeof(RestoreData), 16));
	ctx->restore = psys_alloc_aligned(sizeof(RestoreData), 16);

	// unmask SIGUSR2 and save updated signal mask
	sigset_t set, oldset;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);
	pthread_sigmask(SIG_UNBLOCK, &set, &oldset);
	sigdelset(&oldset, SIGUSR2);
	ctx->restore->sigmask = oldset;
#ifdef DOPT_VSTACK
	pmap_setup_vstack(ctx);
#endif	
	pthread_setspecific(ProcessCtx->tctx_key, ctx);
	atomic_fetch_add(&ProcessCtx->num_ctx, 1);
}


// called by pthread_specific destructor
void cleanup_thread_context(void* ctx_v) {
	ThreadContext* ctx = (ThreadContext*) ctx_v;
	ctx->restore = NULL;
	atomic_fetch_sub(&ProcessCtx->num_ctx, 1);
}

/*
 * 3. Thread Management
 */

typedef struct {
	main_t umain;
	int argc;
	char** argv;
	char** envp;
} main_call; // args passed to main

typedef struct {
	void* (*fn) (void*);
	void* arg;
	ThreadContext* ctx;
} inject_param; // args passed to faai_thread_wrapper

typedef void* (*pthr_hook_t) (void*);

static atomic_int mainret = 0;

// fixes issues with control flow in faai_thread_wrapper after restoration
static void* main_thread_wrapper (void* mc_v) {
	main_call* mc = (main_call*) mc_v;
	int ret = mc->umain(mc->argc, mc->argv, mc->envp);
	assert(atomic_load(&mainret) == 0);
	atomic_store(&mainret, ret);
	atomic_fetch_or(&pStat, _FAAI_EXITING_); // block signal handler after main() completes
	return NULL;
}

static void* faai_thread_wrapper (void* inject_v) {
	
	// NOTE: do not call functions in the main function body or in the else block
	// they might clobber saved stack frames during restoration
	
	// TODO: local copies of params
	inject_param *inject = (inject_param*) inject_v;
	void* ret = NULL; // thread return value
	if (inject->ctx == NULL) { // starting new thread
		create_thread_context();
		setup_alt_stack();
		
		ret = inject->fn(inject->arg);
		
	} else { // we are restoring an existing context
		ThreadContext* ctx = inject->ctx;
		// save the correct return address so we can replace the incorrect one pushed when piglongjmp is called
		/*register void* realret __asm__("r8");
		__asm__ (
			"movq -0x8(%%rsp), %0 \n"
			: "=r" (realret)
		);*/

		piglongjmp(ctx->restore->gpRegs, &ctx->restore->sigmask, ctx->restore->fxSaveArea, ctx);

		// this code should be unreachable. when the control flow we jump into above returns
		// up the stack, it should be to one of the points defined in the first if block
		assert(false && "Unreachable!");
	}

	// force loading return value when "returning" from piglongjmp
	__asm__ inline (
		"movq %%rax, %0\n"
		: "=m" (ret)
	);
	return ret;
}

static void relaunch_thread (ThreadContext* ctx) {
	inject_param* inject = psys_alloc(sizeof(inject_param));
	assert(inject != NULL);
	inject->fn = NULL;
	inject->arg = NULL;
	inject->ctx = ctx;

	ERRNO_REPORT_Z_NR(__pthread_create_impl, &ctx->ptid, NULL, faai_thread_wrapper, inject, true);
}

void faai_cleanup(void);
int faai_start_main(main_t umain, int argc, char** argv, char** envp) {
	unsigned st = atomic_load(&pStat);
	pthread_t real_main_tid;
	if (st & _FAAI_ACTIVE_) {	
#ifndef NDEBUG
		faai_validate_internal();
#endif
		if (st & _FAAI_FRESH_) {
			// spawn fake sigpwr thread if requested
			if (fake_sigpwr_delay != 0.0) {
				pthread_t thr;
				pthread_barrier_init(&fake_sigpwr_barrier, NULL, 2);
				int s = __pthread_create_impl(&thr, NULL, fake_sigpwr_thread, NULL, false);
				assert(s == 0);
				pthread_detach(thr);
			}
		
			/* --- run main for new process --- */
			// construct param object for main thread wrapper	
			main_call* arg = psys_alloc(sizeof(main_call));
			arg->umain = umain;
			arg->argc = argc;
			arg->argv = argv;
			arg->envp = envp;
			// construct param object for general thread wrapper
			inject_param* inject = psys_alloc(sizeof(inject_param));
			assert(inject != NULL);
			inject->fn = &main_thread_wrapper;
			inject->arg = (void*) arg;
			inject->ctx = NULL;
			// launch the thread
			int s = __pthread_create_impl(&real_main_tid, NULL, faai_thread_wrapper, inject, false);
			assert(s == 0);
		} else { // reloading context
			TIMER_START();
			assert(ProcessCtx->ThreadSet[0].restore != NULL); // this should always be main()
			real_main_tid = ProcessCtx->ThreadSet[0].ptid;
			// fixup thread list
			struct pthread* self = (struct pthread*) __pthread_self();
		   	self->next = self;
			self->prev = self;	
			// init restore barrier
			pthread_barrier_init(&faai_restore_barrier, NULL, atomic_load(&ProcessCtx->num_ctx));
			// restart threads
			relaunch_thread(&ProcessCtx->ThreadSet[0]); 
			atomic_fetch_add(&next_ctx, 1);
			for (unsigned i = 1; i < MAX_THREADS; ++i) {
				if (ProcessCtx->ThreadSet[i].restore != NULL) {
					atomic_store(&next_ctx, i);
					relaunch_thread(&ProcessCtx->ThreadSet[i]);
				}
			}
			TIMER_STOP("thread relaunch");
		}

		// this thread will handle SIGPWR, unblock it
		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGPWR);
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);

		// trigger delay countdown
		if (fake_sigpwr_delay != 0.0) pthread_barrier_wait(&fake_sigpwr_barrier);

		// wait for threads to finish
		void* retval = NULL;
		pthread_join(real_main_tid, &retval);

		faai_cleanup();
		
		return atomic_load(&mainret);
	} else { // no FAAI
		return umain(argc, argv, envp);
	}
}

int _p_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
        void *(*start_routine) (void *), void *arg) {
	FAAI_CHECK_INIT();

	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		inject_param* wrapper_arg = psys_alloc(sizeof(inject_param));
		wrapper_arg->fn = start_routine;
		wrapper_arg->arg = arg;
		wrapper_arg->ctx = NULL;
		int s = __pthread_create_impl(thread, attr, faai_thread_wrapper, wrapper_arg, false);
		return s;
	} else return __pthread_create_impl(thread, attr, start_routine, arg, false);
}

/*
 * 5. Constructor and Destructor
 */
// Main init function
bool faai_startup(struct dso* libc_obj, struct dso* exec_obj) {
	// block recursive init
	unsigned comp = 0;
	if (!atomic_compare_exchange_strong(&pStat, &comp, 1)) { // sets _FAAI_DET_ flag
		printf("Double (probably recursive) FAAI initialization! Aborting.");
		abort();
	}
	unsigned st = _FAAI_DET_;

	TIMER_INIT_EARLY();
	TIMER_START();
	
	// see if we are activated
	char* faai_confline = getenv("LD_RELOAD");
	if (!faai_confline) {
		TIMER_INIT_COMPLETE();
		return false; // flags are correct already 
	}

	// init signal mask
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPWR);
	pthread_sigmask(SIG_BLOCK, &set, NULL);	

	// get & check directory
	char* confline = strcpy(alloca(strlen(faai_confline) + 1), faai_confline);
	char* delim = strchr(confline, ';');
	if (delim) *delim = 0;
	int dirfd;
	ERRNO_REPORT_GEZ(dirfd, _v_openat, AT_FDCWD, confline, O_DIRECTORY | O_PATH); 
	
	// map and validate context
	int ctxfd = _v_openat(dirfd, "ctx", O_RDWR);
   	if (ctxfd >= 0) {
		ProcessCtx = (ProcessContext*) _v_mmap(NULL, sizeof(ProcessContext),
				PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE | MAP_POPULATE
#ifdef REAL_PMEM
				| MAP_SYNC
#endif
		, ctxfd, 0);

		if (ProcessCtx == MAP_FAILED) {
			__dl_seterr("Could not remap context! mmap says: %s", strerror(errno));
			return false;
		}
		assert(kill(ProcessCtx->pid, 0) == -1 && errno == ESRCH && "FAAI does not support multi-process");
		ERRNO_REPORT_GEZ(ProcessCtx->heap_dir, _v_openat, dirfd, "heap", O_DIRECTORY|O_PATH);
	} else { // create files
		atomic_fetch_or(&pStat, _FAAI_FRESH_);
		st |= _FAAI_FRESH_;
		ERRNO_REPORT_GEZ(ctxfd, _v_openat, dirfd,
				"ctx", O_RDWR | O_CREAT | O_EXCL, 0644);
		ERRNO_REPORT_Z_NR(ftruncate, ctxfd, sizeof(ProcessContext));
		ProcessCtx = (ProcessContext*) _v_mmap(NULL, sizeof(ProcessContext),
				PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE | MAP_POPULATE
#ifdef REAL_PMEM
				| MAP_SYNC
#endif
		, ctxfd, 0);
		assert(ProcessCtx != MAP_FAILED);
		memset(ProcessCtx, 0, sizeof(ProcessContext));
		ERRNO_REPORT_Z_NR(mkdirat, dirfd, "heap", 0700);
		ERRNO_REPORT_GEZ(ProcessCtx->heap_dir, _v_openat, dirfd, "heap", O_DIRECTORY|O_PATH, 0);
	}
	// done with ctx fd
	ERRNO_REPORT_Z_NR(_v_close, ctxfd);
	// make open fds contiguous for easier restoration
	// extra dup catches errors on dirfd
	int tmpfd;
	ERRNO_REPORT_GEZ(tmpfd, dup, dirfd); // get a duplicate for dirfd
	ERRNO_REPORT_GEZ_NR(dup2, ProcessCtx->heap_dir, dirfd); // point dirfd to heap file
	ERRNO_REPORT_Z_NR(_v_close, tmpfd); // close old dirfd
	ERRNO_REPORT_Z_NR(_v_close, ProcessCtx->heap_dir); // close old heap dir
	ProcessCtx->heap_dir = dirfd; // heapdir is now the value that used to be dirfd
	
	bool skip_loading = true;
	pthread_key_create(&ProcessCtx->tctx_key, cleanup_thread_context); 

#ifdef TIME_PSYS
	const char* __foon = NULL;
#endif

	if (st & _FAAI_FRESH_) {
		pfile_fresh_start();
		pmap_fresh_start(libc_obj, exec_obj);
		skip_loading = false;
#ifdef TIME_PSYS
		__foon = "fresh startup";
#endif
	} else {
		// have to split up initialization to satisfy dependencies
		pmap_restore_heaps();
		pfile_restore();
		pmap_restore_file_backed();
#ifdef TIME_PSYS
		__foon = "restart";
#endif
	}
	TIMER_INIT_COMPLETE();

    atomic_init(&next_ctx, 0);

	// set pid
	ProcessCtx->pid = getpid();

	// register our two handlers
	// these will be passed on to created threads
	// although this thread will be the only one to catch SIGPWR
	struct sigaction act;
	act.sa_handler = faai_sigpwr_handler;
	act.sa_flags = 0;
	ERRNO_REPORT_Z_NR(sigaction, SIGPWR, &act, NULL);
	act.sa_handler = NULL;
	act.sa_sigaction = faai_sigusr2_handler;
	act.sa_flags = SA_SIGINFO;
	ERRNO_REPORT_Z_NR(sigaction, SIGUSR2, &act, NULL);
	// and, the kludge handler for SIGBUS
	//ERRNO_REPORT_Z_NR(sigaction, SIGBUS, &sigbus_act, NULL);
	
	// enable function interposition
	st = atomic_fetch_or(&pStat, _FAAI_ACTIVE_);

#ifdef TIME_PSYS
	TIMER_STOP(__foon);
#endif
	
	// set up fake signal if requested
	char* tok = strchr(faai_confline, ';');
	if ( (st & _FAAI_FRESH_) && tok ) {	
		char* tok2 = strchr(tok+1, ';');
		if (!tok2) tok2 = strchr(tok, '\0');
		assert(tok2 && "Bad format string!");
		size_t len = tok2 - (tok+1);
	   	char* dubt = alloca(len+1);
		strncpy(dubt, tok+1, len);
		dubt[len] = '\0';
		int s = sscanf(dubt, "%lf", &fake_sigpwr_delay);
		assert(s == 1 && "Bad format string!");
	} else fake_sigpwr_delay = 0.0;

	return skip_loading;
}

void faai_cleanup(void) {
	if (atomic_fetch_and(&pStat, ~_FAAI_ACTIVE_) & _FAAI_ACTIVE_) {
		TIMER_START();
		struct sigaction act;
		act.sa_handler = SIG_IGN;
		ERRNO_REPORT_Z_NR(__sigaction, SIGUSR2, &act, NULL);
		cleanup_fd_tracking();
		
		int dirfd;
		ERRNO_REPORT_GEZ(dirfd, _v_openat, ProcessCtx->heap_dir, "..", O_DIRECTORY | O_PATH);

		pmap_cleanup();

		// delete context file and heap dir -- no need to unmap anything
		// heap files should already have been deleted by pmap_cleanup -- dir deletion will fail if not
		ERRNO_REPORT_Z_NR(unlinkat, dirfd, "heap", AT_REMOVEDIR);
		ERRNO_REPORT_Z_NR(unlinkat, dirfd, "ctx", 0);
		ERRNO_REPORT_Z_NR(_v_close, dirfd);
		TIMER_STOP("runtime destructor");
	}
#ifdef TIME_PSYS
	if (__timingact) {
		ERRNO_REPORT_Z_NR(fclose, __timingdat);
		__timingact = NULL;
	}
#endif
}

// other than during startup, caller of this function must hold DSO lock
void _p_save_loader_meta(struct dso* head, struct dso* tail, struct dso* fini_head, struct dso* syms_tail,
	   struct dso* lazy_head) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {	
		ProcessCtx->dso_head = head;
		ProcessCtx->dso_head_next = head->next;
		ProcessCtx->dso_tail = tail;
		ProcessCtx->dso_fini_head = fini_head;
		ProcessCtx->dso_syms_tail = syms_tail;
		ProcessCtx->dso_lazy_head = lazy_head;
	}
}

void _p_restore_loader_meta(struct dso** head, struct dso** tail, struct dso** fini_head, struct dso** syms_tail,
		struct dso** lazy_head) {

	assert((atomic_load(&pStat) & _FAAI_ACTIVE_) && !(atomic_load(&pStat) & _FAAI_FRESH_));
	*head = ProcessCtx->dso_head;
	(*head)->next = ProcessCtx->dso_head_next;
	*tail = ProcessCtx->dso_tail;
	*fini_head = ProcessCtx->dso_fini_head;
	*syms_tail = ProcessCtx->dso_syms_tail;
	*lazy_head = ProcessCtx->dso_lazy_head;
}


// set / restore stack guard value 
static uintptr_t __stack_chk_guard;
void _p_init_ssp(void* entropy) {
	
	if (entropy) memcpy(&__stack_chk_guard, entropy, sizeof(uintptr_t));
	else __stack_chk_guard = (uintptr_t)&__stack_chk_guard * 1103515245;
	
	if (!(atomic_load(&pStat) & _FAAI_ACTIVE_)) { // no FAAI
		__pthread_self()->canary = __stack_chk_guard;
	} else if (atomic_load(&pStat) & _FAAI_FRESH_) { // fresh FAAI
		__pthread_self()->canary = __stack_chk_guard;
		ProcessCtx->canary = __stack_chk_guard;
	} else { // restoring FAAI
		__pthread_self()->canary = ProcessCtx->canary;
	}
}

#pragma GCC diagnostic pop
