#define _GNU_SOURCE
#include <limits.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#define CHUNK_FACTOR 32
#define SLAB_FACTOR CHUNK_FACTOR*1024 // must be a multiple of CHUNK_FACTOR

#define CHUNK_SIZE (PAGE_SIZE*CHUNK_FACTOR)
#define SLAB_SIZE (PAGE_SIZE*SLAB_FACTOR)
#define CHUNK_COUNT (SLAB_SIZE/CHUNK_SIZE)

size_t b_mmap_anon_slab (void* dummy) {
	void* slab = mmap(0, SLAB_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	assert(slab != MAP_FAILED);

	for (char* c = (char*) slab; c < slab + SLAB_SIZE; c += PAGE_SIZE) {
		*c = 'x';
	}
	
	int s = munmap(slab, SLAB_SIZE);
	assert(s == 0);
	return 0;
}

size_t b_mmap_anon_chunks (void* dummy) {
	for (size_t ch = 0; ch < CHUNK_COUNT; ++ch) {
		void* chunk = mmap(0, CHUNK_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
		assert(chunk != MAP_FAILED);
		for (char* c = (char*) chunk; c < chunk + CHUNK_SIZE; c += PAGE_SIZE) {
			*c = 'x';
		}
		munmap(chunk, CHUNK_SIZE);
	}

	return 0;
}

size_t b_mmap_file_slab (void* dummy) {
	int fd = open("/tmp", O_DIRECTORY|O_EXCL|O_RDWR|O_TMPFILE, 0600);
	assert(fd >= 0);
	int s = ftruncate(fd, SLAB_SIZE);
	assert(s == 0);

	void* slab = mmap(0, SLAB_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	assert(slab != MAP_FAILED);
	s = close(fd);
	assert(s == 0);

	for (char* c = (char*) slab; c < slab + SLAB_SIZE; c += PAGE_SIZE) {
		*c = 'x';
	}

	s = munmap(slab, SLAB_SIZE);
	assert(s == 0);
	
	return 0;
}

size_t b_mmap_file_chunks (void* dummy) {
	int fd = open("/tmp", O_DIRECTORY|O_EXCL|O_RDWR|O_TMPFILE, 0600);
	assert(fd >= 0);
	int s = ftruncate(fd, SLAB_SIZE);

	for (size_t ch = 0; ch < CHUNK_COUNT; ++ch) {
		void* chunk = mmap(0, CHUNK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
		assert(chunk != MAP_FAILED);
		for (char* c = (char*) chunk; c < chunk + CHUNK_SIZE; c += PAGE_SIZE) {
			*c = 'x';
		}
		munmap(chunk, CHUNK_SIZE);
	}

	s = close(fd);
	assert(s == 0);

	return 0;
}

size_t b_mmap_files_chunks (void* dummy) {
	for (size_t ch = 0; ch < CHUNK_COUNT; ++ch) {
		int fd = open("/tmp", O_DIRECTORY|O_EXCL|O_RDWR|O_TMPFILE, 0600);
		assert(fd >= 0);
		int s = ftruncate(fd, CHUNK_SIZE);
		assert(s == 0);
		void* chunk = mmap(0, CHUNK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
		assert(chunk != MAP_FAILED);
		s = close(fd);
		assert(s == 0);
		for (char* c = (char*) chunk; c < chunk + CHUNK_SIZE; c += PAGE_SIZE) {
			*c = 'x';
		}
		munmap(chunk, CHUNK_SIZE);
	}

	return 0;
}

size_t b_mmap_anon_slab_nomun (void* dummy) {
	void* slab = mmap(0, SLAB_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	assert(slab != MAP_FAILED);

	for (char* c = (char*) slab; c < slab + SLAB_SIZE; c += PAGE_SIZE) {
		*c = 'x';
	}
	
	return 0;
}

size_t b_mmap_anon_chunks_nomun (void* dummy) {
	for (size_t ch = 0; ch < CHUNK_COUNT; ++ch) {
		void* chunk = mmap(0, CHUNK_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
		assert(chunk != MAP_FAILED);
		for (char* c = (char*) chunk; c < chunk + CHUNK_SIZE; c += PAGE_SIZE) {
			*c = 'x';
		}
	}

	return 0;
}

size_t b_mmap_file_slab_nomun (void* dummy) {
	int fd = open("/tmp", O_DIRECTORY|O_EXCL|O_RDWR|O_TMPFILE, 0600);
	assert(fd >= 0);
	int s = ftruncate(fd, SLAB_SIZE);
	assert(s == 0);

	void* slab = mmap(0, SLAB_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	assert(slab != MAP_FAILED);

	for (char* c = (char*) slab; c < slab + SLAB_SIZE; c += PAGE_SIZE) {
		*c = 'x';
	}
	
	return 0;
}

size_t b_mmap_file_chunks_nomun (void* dummy) {
	int fd = open("/tmp", O_DIRECTORY|O_EXCL|O_RDWR|O_TMPFILE, 0600);
	assert(fd >= 0);
	int s = ftruncate(fd, SLAB_SIZE);

	for (size_t ch = 0; ch < CHUNK_COUNT; ++ch) {
		void* chunk = mmap(0, CHUNK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
		assert(chunk != MAP_FAILED);
		for (char* c = (char*) chunk; c < chunk + CHUNK_SIZE; c += PAGE_SIZE) {
			*c = 'x';
		}
	}

	return 0;
}

size_t b_mmap_files_chunks_nomun (void* dummy) {
	for (size_t ch = 0; ch < CHUNK_COUNT; ++ch) {
		int fd = open("/tmp", O_DIRECTORY|O_EXCL|O_RDWR|O_TMPFILE, 0600);
		assert(fd >= 0);
		int s = ftruncate(fd, CHUNK_SIZE);
		assert(s == 0);
		void* chunk = mmap(0, CHUNK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
		assert(chunk != MAP_FAILED);
		for (char* c = (char*) chunk; c < chunk + CHUNK_SIZE; c += PAGE_SIZE) {
			*c = 'x';
		}
	}

	return 0;
}
