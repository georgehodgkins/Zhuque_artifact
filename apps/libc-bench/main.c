#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>

void print_stats(struct timespec tv0)
{
	FILE *f;
	char buf[256];
	struct timespec tv;
	int maj, min, in_heap=0;
	unsigned long l;
	size_t vm_size=0, vm_rss=0, vm_priv_dirty=0;

	clock_gettime(CLOCK_REALTIME, &tv);
	tv.tv_sec -= tv0.tv_sec;
	if ((tv.tv_nsec -= tv0.tv_nsec) < 0) {
		tv.tv_nsec += 1000000000;
		tv.tv_sec--;
	}

	f = fopen("/proc/self/smaps", "rb");
	if (f) while (fgets(buf, sizeof buf, f)) {
		if (sscanf(buf, "%*lx-%*lx %*s %*lx %x:%x %*lu %*s", &maj, &min)==2)
			in_heap = (!maj && !min && !strstr(buf, "---p") && (strstr(buf, "[heap]") || !strchr(buf, '[')));
		if (in_heap) {
			if (sscanf(buf, "Size: %lu", &l)==1) vm_size += l;
			else if (sscanf(buf, "Rss: %lu", &l)==1) vm_rss += l;
			else if (sscanf(buf, "Private_Dirty: %lu", &l)==1) vm_priv_dirty += l;
		}
	}
	if (f) fclose(f);
	printf("  time: %ld.%.9ld, virt: %zu, res: %zu, dirty: %zu\n\n",
		(long)tv.tv_sec, (long)tv.tv_nsec,
		vm_size, vm_rss, vm_priv_dirty);
}

struct bench_param {
	const char* label;
	size_t (*bench)(void*);
	void* params;
};

void* bench_thread (void* param_v) {
	struct bench_param* param = (struct bench_param*) param_v;
	
	puts(param->label);
	struct timespec tv0;
	clock_gettime(CLOCK_REALTIME, &tv0);
	param->bench(param->params);
	print_stats(tv0);
	return NULL;
}

void run_bench(const char *label, size_t (*bench)(void *), void *params)
{
	struct bench_param param = {
		.label = label,
		.bench = bench,
		.params = params
	};
	pthread_t benchthr;
	int s = pthread_create(&benchthr, NULL, bench_thread, &param);
	assert(s == 0);
	s = pthread_join(benchthr, NULL);
	assert(s == 0);
}

#define RUN(a, b) \
	extern size_t (a)(void *); \
	run_bench(#a " (" #b ")", (a), (b))

int main()
{
	
	RUN(b_malloc_sparse, 0);
	RUN(b_malloc_bubble, 0);
	RUN(b_malloc_tiny1, 0);
	RUN(b_malloc_tiny2, 0);
	RUN(b_malloc_big1, 0);
	RUN(b_malloc_big2, 0);
	RUN(b_malloc_thread_stress, 0);
	RUN(b_malloc_thread_local, 0);
/*
	RUN(b_string_strstr, "abcdefghijklmnopqrstuvwxyz");
	RUN(b_string_strstr, "azbycxdwevfugthsirjqkplomn");
	RUN(b_string_strstr, "aaaaaaaaaaaaaacccccccccccc");
	RUN(b_string_strstr, "aaaaaaaaaaaaaaaaaaaaaaaaac");
	RUN(b_string_strstr, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac");
	RUN(b_string_memset, 0);
	RUN(b_string_strchr, 0);
	RUN(b_string_strlen, 0);
*/
	RUN(b_pthread_createjoin_serial1, 0);
	RUN(b_pthread_createjoin_serial2, 0);
	RUN(b_pthread_create_serial1, 0);
	RUN(b_pthread_uselesslock, 0);
	RUN(b_pthread_createjoin_minimal1, 0);
	RUN(b_pthread_createjoin_minimal2, 0);

	RUN(b_mmap_anon_slab, 0);
	RUN(b_mmap_anon_chunks, 0);
	RUN(b_mmap_file_slab, 0);
	RUN(b_mmap_file_chunks, 0);
	RUN(b_mmap_files_chunks, 0);
/*
	RUN(b_mmap_anon_slab_nomun, 0);
	RUN(b_mmap_anon_chunks_nomun, 0);
	RUN(b_mmap_file_slab_nomun, 0);
	RUN(b_mmap_file_chunks_nomun, 0);
	RUN(b_mmap_files_chunks_nomun, 0);
*/
/*
	RUN(b_utf8_bigbuf, 0);
	RUN(b_utf8_onebyone, 0);

	RUN(b_stdio_putcgetc, 0);
	RUN(b_stdio_putcgetc_unlocked, 0);

	RUN(b_regex_compile, "(a|b|c)*d*b");
	RUN(b_regex_search, "(a|b|c)*d*b");
	RUN(b_regex_search, "a{25}b");
*/	
}

