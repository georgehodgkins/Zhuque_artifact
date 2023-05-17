#if !defined(_PSYS_IMPL_H_) || defined(_PHEAP_IMPL_H_)
#error "This is not a standalone header! Include psys-impl.h instead."
#else
#define _PHEAP_IMPL_H_
#endif

#include "dynlink.h"
#include "elf.h"
#include "libc.h"
#include "link.h"
#include <limits.h>

#define _v_munmap(addr, len) syscall(SYS_munmap, addr, len)
#define _v_mprotect(addr, len, prot) syscall(SYS_mprotect, addr, len, prot)

typedef struct {
	_Atomic(void*) base;
	atomic_ullong len;
	int prot;
	int flags;
	unsigned mref;
	off_t off;
	int des;
} vmtab_entry_t;

#define VMTAB_PAGES 512
#define VMTAB_SIZE ((PAGE_SIZE*VMTAB_PAGES)/sizeof(vmtab_entry_t))
#define MREF_HEAPREF UINT_MAX
#define MREF_PRELOAD (UINT_MAX-1)
#define MREF_STACK (UINT_MAX-2)
#define MREF_SPECIAL (UINT_MAX-2) // lower bound of special mrefs
#define PBRK_CHUNK_LEN (32ul*PAGE_SIZE*PAGE_SIZE) // .5 GB if pages are 4 KB
#if (PBRK_CHUNK_LEN < PAGE_SIZE)
#error "Heap chunk length set incorrectly"
#endif

// local struct defs from ldso/dynlink.c (not src/ldso)
// need these to handle kernel mapped stuff
struct td_index {
	size_t args[2];
	struct td_index *next;
};

struct dso {
#if DL_FDPIC
	struct fdpic_loadmap *loadmap;
#else
	unsigned char *base;
#endif
	char *name;
	size_t *dynv;
	struct dso *next, *prev;

	Phdr *phdr;
	int phnum;
	size_t phentsize;
	Sym *syms;
	Elf_Symndx *hashtab;
	uint32_t *ghashtab;
	int16_t *versym;
	char *strings;
	struct dso *syms_next, *lazy_next;
	size_t *lazy, lazy_cnt;
	unsigned char *map;
	size_t map_len;
	dev_t dev;
	ino_t ino;
	char relocated;
	char constructed;
	char kernel_mapped;
	char mark;
	char bfs_built;
	char runtime_loaded;
	struct dso **deps, *needed_by;
	size_t ndeps_direct;
	size_t next_dep;
	pthread_t ctor_visitor;
	char *rpath_orig, *rpath;
	struct tls_module tls;
	size_t tls_id;
	size_t relro_start, relro_end;
	uintptr_t *new_dtv;
	unsigned char *new_tls;
	struct td_index *td_index;
	struct dso *fini_next;
	char *shortname;
#if DL_FDPIC
	unsigned char *base;
#else
	struct fdpic_loadmap *loadmap;
#endif
	struct funcdesc {
		void *addr;
		size_t *got;
	} *funcdescs;
	size_t *got;
	char buf[];
};

void pmap_fresh_start(struct dso*, struct dso*, void*);
void pmap_restore_heaps(void);
void pmap_restore_file_backed(void);
void pmap_cleanup(void);
void setup_alt_stack(void);
bool is_pmem(void*);

void* psys_alloc(size_t);
void* psys_alloc_aligned(size_t, uint64_t);

#ifndef REAL_PMEM
void pmap_sync_heaps (void);
#endif

