#ifndef PSYS_H
#define PSYS_H

#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/epoll.h>

// internal syscall wrapper replacements
int _p_openat(int, const char*, int, mode_t);

int _p_close(int);

int _p_socket(int, int, int);

int _p_bind(int, const struct sockaddr*, socklen_t);

int _p_connect(int, const struct sockaddr*, socklen_t);

void* _p_mmap(void*, size_t, int, int, int, off_t);

int _p_munmap(void*, size_t);

__attribute__((noreturn))
void _p_unmapself(void*, size_t);

int _p_mprotect(void*, size_t, int);

int _p_brk(void* addr);

//uintptr_t _p_sys_brk (uintptr_t);

void* _p_sbrk(intptr_t);

typedef int (*main_t) (int, char**, char**);
int faai_start_main(main_t umain, int argc, char** argv, char** envp);

int _p_pthread_create(pthread_t *restrict res, const pthread_attr_t *restrict attrp,
		void *(*entry)(void *), void *restrict arg);

struct dso;
bool faai_startup(struct dso*, struct dso*);

void _p_init_ssp(void*);

int _p_dup(int);
int _p_dup2(int, int);

int _p_accept(int, struct sockaddr*, socklen_t*);
int _p_accept4(int, struct sockaddr*, socklen_t*, int);

int _p_listen(int, int);

int _p_setsockopt(int, int, int, const void*, socklen_t);

int _p_epoll_ctl(int, int, int, struct epoll_event*);
int _p_epoll_create1(int);

int _p_pipe2(int pipefd[2], int flags);

int _p_fcntl(int, int, unsigned long);

struct dso;
void _p_save_loader_meta(struct dso*, struct dso*, struct dso*, struct dso*, struct dso*);
void _p_restore_loader_meta(struct dso**, struct dso**, struct dso**, struct dso**, struct dso**);

int _p_madvise(void*, size_t, int);

void* _p_mremap(void*, size_t, size_t, int, void*);

void faai_cleanup(void);

void* ntmemset(void* dest, int32_t c, size_t n);
#ifdef TIME_PSYS
// including unistd.h causes problems
extern pid_t gettid(void);
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "errno-report.h"
extern FILE* __timingdat;
extern char* __timingact;

#define TIMER_RESTART() \
	do { ERRNO_REPORT_Z_NR(clock_gettime, CLOCK_MONOTONIC, &__d0); } while (0)

#define TIMER_START() \
	struct timespec __d0, __d1; \
	double __elapsed; \
	TIMER_RESTART()

#define TIMER_STOP(desc) \
	do { if (__timingact) { \
			assert(__timingdat); \
			ERRNO_REPORT_Z_NR(clock_gettime, CLOCK_MONOTONIC, &__d1); \
			__elapsed = ((double) (__d1.tv_sec - __d0.tv_sec)) + ((double) (__d1.tv_nsec - __d0.tv_nsec))/1e9; \
			double __start = (double) __d0.tv_sec + (double) __d0.tv_nsec / 1e9; \
			__elapsed *= 1e6; __start *= 1e6; \
			fprintf(__timingdat, "%d, %0.3f, %0.3f, %s\n", gettid(), __start, __elapsed, desc); \
		} } while (0)

#else
#define TIMER_RESTART()
#define TIMER_START()
#define TIMER_STOP(...)
#endif // TIME_PSYS

#ifdef DOPT_VSTACK
void* _p_vpush(size_t);
void _p_vpop(void);
#else
#define _p_vpush(x) alloca(x)	
#define _p_vpop(...)
#endif

#endif // PSYS_H
