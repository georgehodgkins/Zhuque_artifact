#include "psys-impl.h"
#include <sys/mman.h>
#include "syscall.h"
#include "pthread_impl.h"

// musl only has posix falloc flags, we need nix ones (INSERT_RANGE specifically)
#undef FALLOC_FL_KEEP_SIZE
#undef FALLOC_FL_PUNCH_HOLE
#include "/usr/include/linux/falloc.h"

#pragma GCC diagnostic ignored "-Wpointer-arith"

// this is due to a kernel bug making MAP_FIXED_NOREPLACE incompatible
// with MAP_SHARED_VALIDATE (which is required for DAX mappings)
#ifdef REAL_PMEM
#define FIXEDMAP MAP_FIXED
#else
#define FIXEDMAP MAP_FIXED_NOREPLACE
#endif

static int prot2fflags(int prot) {
	assert(prot != PROT_NONE);
	int flag = 0;
	if ( prot & (PROT_READ|PROT_EXEC)) {
		if (prot & PROT_WRITE) {
			flag = O_RDWR;
		} else {
			flag = O_RDONLY;
		}
	} else if (prot & PROT_WRITE) {
		flag = O_WRONLY;
	}
	return flag;
}	

__attribute__((target("sse2")))
void* ntmemset(void* dest, int32_t c, size_t n) {
	uint32_t src[4] = {c, c, c, c};

	while (n & ~7) {
		__builtin_ia32_movntq(dest, *(long long unsigned int*) src);
		n -= 8;
		dest += 8;
	}

	if (n & ~0x7) {
		__builtin_ia32_movnti(dest, *(long long unsigned int*) src);
		n -= 8;
		dest += 8;
	}

	return dest;
}


// returns the length of the name string for a given designator
static const char* descharset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
static size_t desnamelen(int des) {
	//_Static_assert(__builtin_object_size(descharset, 0)-1 == 64);
	assert(des >= 0);
	size_t len = 2;
	unsigned x = 0x40;
	unsigned udes = des;
	for (; (udes & x); ++len, x <<= 6);
	return len;
}

// writes the name corresponding to a designator into a output buffer
static void des2name(int des, char* name) {
	assert(des >= 0);
	unsigned udes = des;
	if (udes == 0) *(name++) = '0';
	else {
		for (; udes; udes >>= 6)
			*(name++) = descharset[udes & 0x3f];
	}

	*name = 0;
}

// increments designator reference count. *does not* create file for new entry
static unsigned add_desref (int des) {
	int x = atomic_fetch_add(&ProcessCtx->desref[des], 1);

	if (x == 0) { // new entry
		int hides = atomic_load(&ProcessCtx->hides);
		while (des > hides-1 && !atomic_compare_exchange_strong(&ProcessCtx->hides, &hides, des+1));

		int lodes = atomic_load(&ProcessCtx->lodes);
		if (des == lodes) {
			int y;
			for (y = lodes+1; y < hides; ++y) {
				if (!atomic_load(&ProcessCtx->desref[y]))
					break;
			}
			int plodes = lodes;
			while (!atomic_compare_exchange_strong(&ProcessCtx->lodes, &lodes, y) && lodes > plodes);	
		}
	}

	return x;
}

// decrements designator reference count. *does* delete file if count reaches zero
static unsigned sub_desref (int des) {
	if (atomic_load(&ProcessCtx->desref[des]) == 1) {
		char* name = alloca(desnamelen(des));
		des2name(des, name);
		ERRNO_REPORT_Z_NR(unlinkat, ProcessCtx->heap_dir, name, 0);

		int x = atomic_fetch_sub(&ProcessCtx->desref[des], 1);
		assert(x == 1);

		int hides = atomic_load(&ProcessCtx->hides);
		if (des == hides-1) {
			int y;
			for (y = hides; y > 0; --y) {
			   if (atomic_load(&ProcessCtx->desref[y-1]))
					break;
			}
			while (y < hides && !atomic_compare_exchange_strong(&ProcessCtx->hides, &hides, y));
		}

		//int lodes = atomic_load(&ProcessCtx->lodes);
		//while (des < lodes && !atomic_compare_exchange_strong(&ProcessCtx->lodes, &lodes, des));
		
		return x;
	} else {
		return atomic_fetch_sub(&ProcessCtx->desref[des], 1);
	}
}

// generate a new heap designator
static int get_new_desref (void) {
	int des = atomic_load(&ProcessCtx->lodes);
	assert(des < UINT_MAX);
	add_desref(des);
	return des;
}

// look up the vmtab entry corresponding to an address.
static int find_vmtab_entry(void* addr) {
	for (int i = 0; i < VMTAB_SIZE; ++i) {
		void* base = atomic_load(&ProcessCtx->vmtab[i].base);
		if (addr >= base && addr < base + ProcessCtx->vmtab[i].len)
			return i;
	}
	return -1;
}

// add a mapping to the vm table (or split an existing mapping, or modify its metadata)
static int add_vmtab_entry (void* addr, size_t len, int prot, int flags, int fd,
		off_t off, int des) {

	TIMER_START();
	assert(addr);
	// check alignment
	assert(!(len & (PAGE_SIZE-1)));
	assert(!((uint64_t) addr & (PAGE_SIZE-1)));
	assert(!(off & (PAGE_SIZE-1)));

	if (prot != PROT_NONE && (flags & MAP_ANONYMOUS))
		assert(flags & MAP_REAL_ANON);

	// modify existing mapping if necessary
	int existing = find_vmtab_entry(addr);
	vmtab_entry_t* ex_ent = NULL;
	if (existing >= 0) {
		ex_ent = &ProcessCtx->vmtab[existing];
		// if range is identical to existing, just update metadata and return
		if (addr == ex_ent->base && len == ex_ent->len) {
			ex_ent->prot = prot;
			ex_ent->flags = flags;
			assert(fd == -1 || ProcessCtx->mrtab[ex_ent->mref].ofd == fd);

			if (des >= 0 && ex_ent->des < 0) {
				ex_ent->des = des;
				ex_ent->mref = MREF_HEAPREF;
				ex_ent->off = off;
			} else {
				assert(des < 0 || des == ex_ent->des);
				assert(ex_ent->off == off);
			}
			TIMER_STOP("update vmtab entry");
			return existing;
		}
		assert(addr >= ex_ent->base);
		if (ex_ent->des >= 0) assert(des == ex_ent->des);
		size_t newlen = (addr - ex_ent->base);

		// do not take offset on new pmem mappings from old entry
		// but do take it if old entry was pmem backed
		if (des < 0 && ex_ent->des < 0) off = ex_ent->off + newlen;
		
		if (newlen == 0) { // at beginning of existing mapping
			assert(ex_ent->base == addr && ex_ent->len > len);
			ex_ent->len -= len;
			ex_ent->off += len;
			atomic_store(&ex_ent->base, ex_ent->base + len); 
		} else if ((addr + len) < ex_ent->base + ex_ent->len) { // in middle of existing mapping
			add_vmtab_entry((addr + len), ex_ent->len - newlen - len, ex_ent->prot, ex_ent->flags, 0,
					ex_ent->off + newlen + len, ex_ent->des);
			assert(ex_ent->len == newlen + len); // should have adjusted existing entry down
			ex_ent->len = newlen;
		} else { // at end of existing mapping
			ex_ent->len = addr - ex_ent->base;
		}
	}

	// find empty slot
	void* eslot = NULL;
	int i;
	for (i = 0; !atomic_compare_exchange_strong(&ProcessCtx->vmtab[i].base, &eslot, addr)
			&& i < VMTAB_SIZE; ++i) {
		eslot = NULL;
	}
	assert(i < VMTAB_SIZE && "Vmtab full!");

	ProcessCtx->vmtab[i].len = len;
	ProcessCtx->vmtab[i].prot = prot;
	ProcessCtx->vmtab[i].flags = flags;
	ProcessCtx->vmtab[i].off = off;
	
	if (des >= 0) { // always use designator if one was provided
		ProcessCtx->vmtab[i].des = des;	
	} else if (ex_ent) { // otherwise copy from existing entry
		ProcessCtx->vmtab[i].des = ex_ent->des;
		if (ex_ent->des >= 0) {
			unsigned x = add_desref(ex_ent->des);
			assert(x != 0);
		}
	} else { // or set to zero if no existing and no provided
		ProcessCtx->vmtab[i].des = -1;
	}

	if (ProcessCtx->vmtab[i].des >= 0) ProcessCtx->vmtab[i].mref = MREF_HEAPREF;
	else if (fd < 0 && !ex_ent) ProcessCtx->vmtab[i].mref = MREF_PRELOAD;
	else if (!ex_ent) ProcessCtx->vmtab[i].mref = set_fd_mapped(fd);
	else {
	   	if (ex_ent->mref < MREF_SPECIAL)
			add_mref(ex_ent->mref);
		ProcessCtx->vmtab[i].mref = ex_ent->mref;
	}
	TIMER_STOP("add vmtab entry");
	return i;
}

// removes a mapping from the vm table
// if it is pmem-backed, also deletes the pmem file 
static void remove_vmtab_entry(int i) {
	TIMER_START();
	assert(atomic_load(&ProcessCtx->vmtab[i].base));
	if (ProcessCtx->vmtab[i].des >= 0) {
		sub_desref(ProcessCtx->vmtab[i].des);
	} else if (ProcessCtx->vmtab[i].mref < MREF_SPECIAL) {
		subtract_mref(ProcessCtx->vmtab[i].mref);
	}
	atomic_store(&ProcessCtx->vmtab[i].base, NULL);
	TIMER_STOP("remove vmtab entry");
}

// remove or adjust vmtab entries in the given range
static void remove_vmtab_entries(void* addr, size_t len) {
	size_t remlen = 0;
	for (int i = 0; i < VMTAB_SIZE && remlen < len; ++i) {
		void* base = atomic_load(&ProcessCtx->vmtab[i].base);
		if (base) {
			size_t tlen = ProcessCtx->vmtab[i].len;
			if (base == addr) {
				if (len >= tlen) {
					remove_vmtab_entry(i);
					remlen += tlen;
				} else { // len < tlen
					ProcessCtx->vmtab[i].len -= len;
					atomic_store(&ProcessCtx->vmtab[i].base, base + len);
					remlen += len;
				}
			} else if (base < addr && (base + tlen) > addr) {
				size_t shrink = (base + tlen) - addr;
				ProcessCtx->vmtab[i].len -= shrink;
				remlen += shrink;
			} else if (base > addr) { 
				if (base + tlen <= addr + len) {
					remove_vmtab_entry(i);
					remlen += tlen;
				} else if (base < addr + len) {
					size_t shrink = tlen - len;
					ProcessCtx->vmtab[i].len -= shrink;
					atomic_store(&ProcessCtx->vmtab[i].base, addr + len);
					remlen += shrink;
				}
			}
		}
	}
	//assert(remlen == len);
}

// transform a memory region to pmem backing
// contents will be copied if MAP_POPULATE is in flags, or zeroed if MAP_ANONYMOUS
static void* make_pmem_backed(volatile ProcessContext* ctx, void* addr, size_t len, int prot, int flags) {

	TIMER_START();
	flags &= ~MAP_PRIVATE;
#ifdef REAL_PMEM
	flags |= (MAP_SYNC | MAP_SHARED_VALIDATE);
#else
	flags |= MAP_SHARED;
#endif
	
	// align args, to avoid silent discrepancies between table and actual mappings
	addr = (void*) ((uint64_t) addr & ~(PAGE_SIZE-1));
	if (len % PAGE_SIZE) len += PAGE_SIZE - (len % PAGE_SIZE);

	// check if this is already a pmem-backed region
	if (addr) {
		int existing = find_vmtab_entry(addr);
		if (existing >= 0 && ProcessCtx->vmtab[existing].des >= 0) {
			assert(!(flags & MAP_POPULATE)
				&& "Need to add MAP_POPULATE support in this branch");
			vmtab_entry_t *ex_ent = &ProcessCtx->vmtab[existing];
			void* estop = ex_ent->base + ex_ent->len;
			void* nstop = addr + len;
			if (nstop > estop) { // need to extend existing file
				ex_ent->len += (nstop - estop);
				assert(!(ex_ent->len & (PAGE_SIZE-1)));
				char* name = alloca(desnamelen(ex_ent->des));
				des2name(ex_ent->des, name);
				int fd;
				ERRNO_REPORT_GEZ(fd, _v_openat, ctx->heap_dir, name, O_RDWR, 0700);
				ERRNO_REPORT_Z_NR(fallocate, fd, 0, 0, ex_ent->len);
				void* naddr = _v_mmap(ex_ent->base, ex_ent->len, ex_ent->prot,
					ex_ent->flags|FIXEDMAP, fd, 0);
				assert(naddr == ex_ent->base);
				ERRNO_REPORT_Z_NR(_v_close, fd);
			}
			if (ex_ent->prot != prot) {
				_v_mprotect(addr, len, prot);
				add_vmtab_entry(addr, len, prot, flags, -1,
					(off_t) (addr - ex_ent->base), ex_ent->des);
			}
			if (flags & MAP_ANONYMOUS) {
				ntmemset(addr, 0, len);
				flags &= ~MAP_ANONYMOUS;
			}
			TIMER_STOP("modify pmem region");
			return addr;
		}
	}

	
	// assign designator
	unsigned des = get_new_desref();
	char* name = alloca(desnamelen(des));
	des2name(des, name);
	// open new heap file
	int fd;
	ERRNO_REPORT_GEZ(fd, _v_openat, ctx->heap_dir, name,
			O_RDWR|O_CREAT|O_EXCL, 0700);
	//ERRNO_REPORT_Z_NR(ftruncate, fd, len);
	ERRNO_REPORT_Z_NR(fallocate, fd, 0, 0, len);
	//ERRNO_REPORT_Z_NR(fsync, ctx->heap_dir);

	if (flags & MAP_POPULATE) { // copy contents of previous mapping
		assert(!(flags & MAP_ANONYMOUS));
		ssize_t s = 0;
		size_t rlen = len;
		void* raddr = addr;
		do {
			s = write(fd, raddr, rlen);
			if (s < 0) {
				// handle the case where we estimated too much space after a segment
				assert(errno == EFAULT);
				len -= rlen;
				break;
			}
			rlen -= s;
			raddr += s;
		} while (rlen > 0);
		// round mapping size back to page in case it got truncated
		size_t len_mod = len & (PAGE_SIZE-1);
		if (len_mod) {
			len += (PAGE_SIZE - len_mod);
		}
		ERRNO_REPORT_Z_NR(fsync, fd);
		//ERRNO_REPORT_Z_NR(_v_munmap, addr, len);
	} 

	if (addr) {
		//flags |= FIXEDMAP;
		flags |= MAP_FIXED;
	}
	void* nmap = _v_mmap(addr, len, prot, flags & ~MAP_ANONYMOUS, fd, 0);
	assert(nmap != MAP_FAILED);
	if (addr) assert(nmap == addr);
	addr = nmap;

	if (flags & MAP_ANONYMOUS) {
		//ntmemset(addr, 0, len);
		flags &= ~MAP_ANONYMOUS;
	}

	// done with fd after mapping
	ERRNO_REPORT_Z_NR(_v_close, fd);

	add_vmtab_entry(addr, len, prot, flags, -1, 0, des); 

	TIMER_STOP("create pmem region");
#ifndef DNDEBUG
	int nt = find_vmtab_entry(addr);
	assert(nt >= 0);
	assert(ProcessCtx->vmtab[nt].base == addr);
	assert(ProcessCtx->vmtab[nt].len == len);
	assert(ProcessCtx->vmtab[nt].des == des);
#endif

	return addr;
}

// restores vmtab entry after a failure
static void restore_vmtab_entry(volatile ProcessContext* ctx, int idx) {
	TIMER_START();
	assert(atomic_load(&ctx->vmtab[idx].base) != NULL);

	// cast discards volatility
	vmtab_entry_t* entry = (vmtab_entry_t*) &ctx->vmtab[idx];
	int fd = -1;
	assert(entry->prot == PROT_NONE || 
			!!(entry->flags & MAP_ANONYMOUS) == !!(entry->flags & MAP_REAL_ANON)); 
	if (entry->mref == MREF_PRELOAD) {
		return; // assume already loaded by kernel...hopefully in the same place?
	} else if (entry->mref != MREF_HEAPREF) {
		fd = get_temp_mref(entry->mref);
	} else if (entry->prot != PROT_NONE) { // special exception for guard pages
		assert(entry->des >= 0);
		char* name = alloca(desnamelen(entry->des));
		des2name(entry->des, name);
		ERRNO_REPORT_GEZ(fd, _v_openat, ctx->heap_dir, name, prot2fflags(entry->prot), 0); 
		// clear the range in case we are clobbering a section of a kernload file
		// no error if nothing mapped
		ERRNO_REPORT_Z_NR(_v_munmap, entry->base, entry->len);
	}
	void* nmap = (void*) syscall(SYS_mmap, entry->base, entry->len, entry->prot,
			entry->flags | FIXEDMAP, fd, entry->off);
	assert(nmap != MAP_FAILED);
	free_temp_mref(fd);
	TIMER_STOP("restore mapping");
}

// add vmtab entries and pmem-backed writable sections for objects mapped by the kernel
// takes a context arg because one of these objects (ldso) contains the global ProcessCtx pointer
static void persist_kernel_mapped(volatile ProcessContext* ctx, struct dso* obj) {
	assert(obj->kernel_mapped);
	TIMER_START();
	Phdr* phdr = obj->phdr;
	int phnum = obj->phnum;
	char* path = obj->name;
	void* base = obj->map;
	size_t len = obj->map_len;
	// make one RO entry covering the whole file, which will be split later for W/X mappings
	int ofd;
	ERRNO_REPORT_GEZ(ofd, open, path, O_RDONLY, 0);
	add_vmtab_entry(base, len, PROT_READ, MAP_PRIVATE, -1, 0, -1);
	for (int i = 0; i < phnum; ++i) {
		// ignore non-mapped segments
		if (phdr[i].p_type != PT_LOAD) continue;
		// map addr and len must be page-aligned, but the actual contents still 
		// start at unaligned p_vaddr -- so the total length of the mapping is
		// memsz + down-align start to nearest page + up-align length to nearest page
		void* seg_addr = base + (phdr[i].p_vaddr & ~(PAGE_SIZE-1));
		size_t seg_len = phdr[i].p_memsz + (phdr[i].p_vaddr & (PAGE_SIZE-1));
		size_t len_mod = seg_len & (PAGE_SIZE-1);
		if (len_mod) seg_len += (PAGE_SIZE - len_mod);
		if (phdr[i].p_flags & PF_W) { // writable segment
			// to be safe, we assume even BSS segments could have contents already
			void* naddr = make_pmem_backed(ctx, seg_addr, seg_len, PROT_READ|PROT_WRITE,
					MAP_SHARED|MAP_POPULATE);
			assert(naddr == seg_addr);
		}
		if (phdr[i].p_flags & PF_X) { // executable segment (W/X are not mutually exclusive, although rare)
			int prot = PROT_READ|PROT_EXEC;
			if (phdr[i].p_flags & PF_W) prot |= PROT_WRITE;
			add_vmtab_entry(seg_addr, seg_len, prot, MAP_PRIVATE, -1, 0, -1);
		}
	}
	ERRNO_REPORT_Z_NR(close, ofd);
	TIMER_STOP("transform kernel mapped");
}

// Get anonymous mem from this thread's 'break'
static void* anon_from_pbrk (size_t len) {
	ThreadContext* ctx = myContext();
	TIMER_START();
	vmtab_entry_t** brkent_pt;
	void** curbrk_pt;
	if (ctx) {
		brkent_pt = &ctx->brkent;
		curbrk_pt = &ctx->curbrk;
	} else {
		brkent_pt = &ProcessCtx->brkent;
		curbrk_pt = &ProcessCtx->curbrk;
	}
	vmtab_entry_t* brkent = *brkent_pt;
	void* curbrk = *curbrk_pt;
		
	if (len % PAGE_SIZE) len += (PAGE_SIZE - len%PAGE_SIZE);
	void* mem = NULL;
	if (len >= PBRK_CHUNK_LEN) { // big mappings get their own entry
		return make_pmem_backed(ProcessCtx, NULL, len, PROT_READ|PROT_WRITE, 0);
	} else if (brkent == NULL || curbrk + len >= brkent->base + PBRK_CHUNK_LEN) { // new brk	
		mem = make_pmem_backed(ProcessCtx, NULL, PBRK_CHUNK_LEN, PROT_READ|PROT_WRITE, 0);
		*brkent_pt = &ProcessCtx->vmtab[find_vmtab_entry(mem)];
		*curbrk_pt = mem + len;
	} else {
		mem = curbrk;
		(*curbrk_pt) += len;
	}
	TIMER_STOP("anon pmem alloc");
	assert(*curbrk_pt <= (*brkent_pt)->base + PBRK_CHUNK_LEN);
	return mem;
}

void* psys_alloc_aligned(size_t sz, uint64_t aln) {
	uint64_t mem = (uint64_t) anon_from_pbrk(sz + aln);
	uint64_t aln_mask = ~(aln-1);
	if (aln && mem != (mem & aln_mask)) {
		assert(mem > (mem & aln_mask));
		mem = (mem & aln_mask) + aln;
	}

	return (void*) mem;
}

void* psys_alloc(size_t sz) {
	return psys_alloc_aligned(sz, 0);
}

// handle thread stack overflows
static struct sigaction fallback_segv_action, faai_segv_action;
void faai_sigsegv_handler (int sig, siginfo_t* info, void* ucontext) {
	assert(sig == SIGSEGV);
	ERRNO_REPORT_Z_NR(sigaction, sig, &fallback_segv_action, NULL);
	struct pthread* self = (struct pthread*) __pthread_self();
	if (self && info->si_addr > (self->stack - self->stack_size - PAGE_SIZE) && info->si_addr <= self->stack) {
		TIMER_START();
		// handle stack overflow
		int existing = find_vmtab_entry(self->stack);
		assert(existing != -1);
		vmtab_entry_t* ex_ent = &ProcessCtx->vmtab[existing];
		size_t nlen = ex_ent->len + __default_stacksize;
		void* naddr = ex_ent->base - __default_stacksize;
		assert(self->guard_size >= nlen && "No more space to extend stack!");

#ifdef USE_VSTACK
		assert(ex_ent->des == -1);
		assert(ex_ent->flags & MAP_ANON);
		void* nmap = _v_mmap(naddr, nlen, ex_ent->prot, ex_ent->flags | FIXEDMAP, -1, 0);
#else	
		// extend file
		assert(ex_ent->des >= 0);
		assert(atomic_load(&ProcessCtx->desref[ex_ent->des]) == 1);
		char* name = alloca(desnamelen(ex_ent->des));
		des2name(ex_ent->des, name);
		int fd;
		ERRNO_REPORT_GEZ(fd, _v_openat, ProcessCtx->heap_dir, name, O_RDWR, 0);
		ERRNO_REPORT_Z_NR(fallocate, fd, FALLOC_FL_INSERT_RANGE, ex_ent->off, __default_stacksize);

		// extend mapping
		void* nmap = _v_mmap(naddr, nlen, ex_ent->prot, ex_ent->flags | FIXEDMAP, fd, ex_ent->off);
#endif
		assert(nmap == naddr);
		ex_ent->base = naddr;
		ex_ent->len = nlen;
		self->stack_size += __default_stacksize;	
		self->guard_size -= __default_stacksize;
		TIMER_STOP("stack extend");
	} else if (fallback_segv_action.sa_handler != NULL 
			&& fallback_segv_action.sa_handler != SIG_IGN
			&& fallback_segv_action.sa_handler != SIG_DFL) 
	{
		// if this handler returns, we can continue execution
		if (fallback_segv_action.sa_flags & SA_SIGINFO) {
			fallback_segv_action.sa_sigaction(sig, info, ucontext);
		} else {
			fallback_segv_action.sa_handler(sig);
		}
	} else {
		abort();
	}	
	ERRNO_REPORT_Z_NR(sigaction, sig, &faai_segv_action, NULL);
}

// this is called every time a thread is created
void setup_alt_stack (void) {
	stack_t old;
	ERRNO_REPORT_Z_NR(sigaltstack, NULL, &old);
	if (!(old.ss_flags & SS_DISABLE)) return; // alt stack already in place
	
	size_t mapsz = (SIGSTKSZ > PAGE_SIZE) ? SIGSTKSZ : PAGE_SIZE;
	void* mem = _v_mmap(NULL, mapsz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
	assert(mem != MAP_FAILED);
	stack_t altstack;
	altstack.ss_sp = mem;
	altstack.ss_flags = 0;
	altstack.ss_size = mapsz;
	ERRNO_REPORT_Z_NR(sigaltstack, &altstack, NULL);
}

// this happens once, at startup
static void setup_segv_handler (void) {
	sigset_t allsigs;
	sigfillset(&allsigs);
	faai_segv_action.sa_flags = SA_SIGINFO | SA_ONSTACK;
	faai_segv_action.sa_mask = allsigs;
	faai_segv_action.sa_sigaction = &faai_sigsegv_handler;
	ERRNO_REPORT_Z_NR(sigaction, SIGSEGV, &faai_segv_action, &fallback_segv_action);
}

/*
 * Public interfaces
 */

// Original musl mmap and supporting code
static void dummy(void) { }
weak_alias(dummy, __vm_wait);
#define UNIT SYSCALL_MMAP2_UNIT
#define OFF_MASK ((-0x2000ULL << (8*sizeof(syscall_arg_t)-1)) | (UNIT-1))
void* _v_mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
	//assert(!ProcessCtx || start + len-1 < (void*) ProcessCtx || start > (void*) ProcessCtx + sizeof(ProcessContext)
	//		&& "Mapping over context object!");

	long ret;
	if (off & OFF_MASK) {
		errno = EINVAL;
		return MAP_FAILED;
	}
	if (len >= PTRDIFF_MAX) {
		errno = ENOMEM;
		return MAP_FAILED;
	}
	if (flags & MAP_FIXED) {
		__vm_wait();
	}
#ifdef SYS_mmap2
	ret = __syscall(SYS_mmap2, start, len, prot, flags, fd, off/UNIT);
#else
	ret = __syscall(SYS_mmap, start, len, prot, flags, fd, off);
#endif
	/* Fixup incorrect EPERM from kernel. */
	if (ret == -EPERM && !start && (flags&MAP_ANON) && !(flags&MAP_FIXED))
		ret = -ENOMEM;
	return (void *)__syscall_ret(ret);
}

void* _p_mmap (void* addr, size_t len, int prot, int flags, int fd, off_t off) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		if (flags & MAP_REAL_ANON) { // vmalloc
		    assert(flags & MAP_ANONYMOUS);
		    void* nmap = _v_mmap(NULL, len, prot, flags & ~MAP_REAL_ANON, -1, 0);
		    assert(nmap != MAP_FAILED);
		    add_vmtab_entry(nmap, len, prot, flags, -1, 0, -1);
		    return nmap;
		} else if (flags & MAP_ANONYMOUS && prot != PROT_NONE && !(flags & MAP_FIXED)) { // heap mapping
			assert(fd == -1);
			return anon_from_pbrk(len);
		} else if (((prot & PROT_WRITE) && (flags & MAP_PRIVATE))
					|| (flags & MAP_STACK)) { // static or thread stack mapping
			// map in original file so its contents can be copied over
			// this mapping is replaced internally by make_pmem_backed()
			if (fd >= 0) {
				void* naddr = _v_mmap(addr, len, prot, flags, fd, off);
				assert(naddr != MAP_FAILED);
				if (flags & MAP_FIXED) assert(naddr == addr);
				return make_pmem_backed(ProcessCtx, naddr, len, prot, flags | MAP_POPULATE);
			} else return make_pmem_backed(ProcessCtx, addr, len, prot, flags & ~MAP_POPULATE);
		} else { // unbacked mapping
			TIMER_START();
			if (len%PAGE_SIZE) len += (PAGE_SIZE - len%PAGE_SIZE);
			addr = (void*) ((uint64_t) addr & ~(PAGE_SIZE-1));
			void* nmap = _v_mmap(addr, len, prot, flags, fd, off);
			assert(nmap != MAP_FAILED);
			addr = nmap;
			add_vmtab_entry(addr, len, prot, flags, fd, off, -1);
			return addr;
			TIMER_STOP("unbacked mapping");
		}
	} else {
		return _v_mmap(addr, len, prot, flags, fd, off);
	}
}

int _p_munmap (void* addr, size_t len) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) remove_vmtab_entries(addr, len);
	return _v_munmap(addr, len);
}

void* _p_mremap (void* old_addr, size_t old_len, size_t new_len, int flags, void* new_addr) {

	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		int existing = find_vmtab_entry(old_addr);
		vmtab_entry_t *ex_ent = &ProcessCtx->vmtab[existing];
		assert(!(flags & MREMAP_FIXED) && "fixed mremap not implemented");
		assert(!(flags & MREMAP_DONTUNMAP) || (ex_ent->des == -1) && "DONTUNMAP not implemented for pmem-backed");
		ThreadContext* ctx = myContext();
		bool try_inplace = true;

		// half-assed check for overlapping regions
		if (find_vmtab_entry(old_addr + old_len) != -1 ||
				find_vmtab_entry(old_addr + new_len - 1) != -1) {
			try_inplace = false;
		}

		if (ex_ent->des >= 0) { // pmem-backed
			if (ex_ent == ctx->brkent) { // old_addr is from the pbrk
				// we can only resize pbrk mem in place if it was the most recent allocation
				if (old_addr + old_len != ctx->curbrk) { 
					try_inplace = false;
				}
			}

			if (try_inplace) {
				// expand file
				char* name = alloca(desnamelen(ex_ent->des));
				des2name(ex_ent->des, name);
				int fd;
				ERRNO_REPORT_GEZ(fd, _v_openat, ProcessCtx->heap_dir, name, O_RDWR);
				ERRNO_REPORT_Z_NR(fallocate, fd, FALLOC_FL_ZERO_RANGE, ex_ent->off + old_len, new_len - old_len);
				ERRNO_REPORT_Z_NR(_v_close, fd);

				// remap
				void* naddr = (void*) syscall(SYS_mremap, old_addr, old_len, new_len,
					flags & ~MREMAP_MAYMOVE, new_addr);
				if (naddr != MAP_FAILED) {
					atomic_store(&ex_ent->base, naddr);
					ex_ent->len = new_len;
					return naddr;
				}
			}
				
			if (!(flags & MREMAP_MAYMOVE)) {
				errno = ENOMEM;
				return MAP_FAILED;
			}

			// create a new pmem mapping and copy over the contents of the old one
			void* nmem = make_pmem_backed(ProcessCtx, NULL, new_len, ex_ent->prot, ex_ent->flags);
			assert(nmem != MAP_FAILED);
			memcpy(nmem, old_addr, old_len);
			return nmem;
		} else { // not pmem-backed, just call mremap and return result
			void* naddr = (void*) syscall(SYS_mremap, old_addr, old_len, new_len, flags, new_addr);
			assert(naddr != MAP_FAILED);
			if (naddr != MAP_FAILED) {
				atomic_store(&ex_ent->base, naddr);
				ex_ent->len = new_len;
			}
			return naddr;
		}
	} else return (void*) syscall(SYS_mremap, old_addr, old_len, new_len, flags, new_addr);
}

// handle detached threads correctly
__attribute__((noreturn))
void _p_unmapself(void* addr, size_t len) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) remove_vmtab_entries(addr, len);
	__unmapself(addr, len);
	while (1) ;
}
			

// TODO: does not support changes across multiple mappings. Probably not important.
int _p_mprotect (void* addr, size_t len, int prot) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
	
		// align args, since mprotect works at page granularity
		addr = (void*) ((uint64_t) addr & ~(PAGE_SIZE-1));
		if (len%PAGE_SIZE) len = len + (PAGE_SIZE - len%PAGE_SIZE);

		int existing = find_vmtab_entry(addr);
		if (existing == -1) {
			errno = ENOMEM;
			return -1;
		}
		vmtab_entry_t* ex_ent = &ProcessCtx->vmtab[existing];

		// replace writable, private mappings with pmem backing
#ifdef USE_VSTACK
		if (ex_ent->flags & MAP_STACK) {
			assert(!is_pmem(addr));
			assert(ex_ent->flags & MAP_ANONYMOUS);
			ERRNO_REPORT_Z_NR(_v_mprotect, addr, len, prot);
			add_vmtab_entry(addr, len, prot, ex_ent->flags, -1, 0, -1);
		} else
#endif
		if ((prot & PROT_WRITE) && (ex_ent->flags & MAP_PRIVATE)) {
			int flags = ex_ent->flags;
			if (ex_ent->prot & PROT_READ) flags |= MAP_POPULATE;
			void* naddr = make_pmem_backed(ProcessCtx, addr, len, prot, flags);
			assert (naddr == addr);
		} else {
			ERRNO_REPORT_Z_NR(_v_mprotect, addr, len, prot);
			add_vmtab_entry(addr, len, prot, ProcessCtx->vmtab[existing].flags, -1, 0,
				ProcessCtx->vmtab[existing].des);
		}
		return 0;
	} else {
		return _v_mprotect(addr, len, prot);
	}
}

#ifdef DOPT_VSTACK
#define DOPT_VSTACK_PAGES 16
void* _p_vpush(size_t len) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		ThreadContext* ctx = myContext();
		assert(ctx && "No valloca in the master thread!");

		len += sizeof(void*); // make room for frame pointer

		void* mem = ctx->vcurbrk;
		ctx->vcurbrk += len;
		*((void**)ctx->vcurbrk) = mem;

		return mem + sizeof(void*);
	} else assert(!"Valloca not supported for volatile programs!");
}

void _p_vpop (void) {
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		ThreadContext* ctx = myContext();
		assert(ctx && "No valloca in the master thread!");

		void** frame = (void**) ctx->vcurbrk;
		ctx->vcurbrk = *frame;
	} else assert(!"Valloca not supported for volatile programs!");
}

void pmap_setup_vstack(ThreadContext* ctx) {
	ctx->vcurbrk = make_pmem_backed(ProcessCtx, NULL, PAGE_SIZE * DOPT_VSTACK_PAGES, PROT_READ | PROT_WRITE, 0);
}
#endif // DOPT_VSTACK
	
// tests if an address is backed by (our) pmem
bool is_pmem(void* addr) {
	int entry = find_vmtab_entry(addr);
	if (entry == -1) return false;

	return (ProcessCtx->vmtab[entry].des >= 0);
}

// restore all heaps -- called before files are reopened at restoration
void pmap_restore_heaps (void) {
	assert(!(atomic_load(&pStat) & _FAAI_FRESH_));
#ifdef DOPT_VSTACK
	assert(!"Enabling vstack disables restoration!");
#endif
	// we get a local copy of the context and pStat because
	// their old values will be restored when the ldso globals are reloaded
	// but we want to keep the new values
	volatile ProcessContext* ctx_local = ProcessCtx;
	volatile unsigned true_pstat = atomic_load(&pStat); 
	for (unsigned i = 0; i < VMTAB_SIZE; ++i) {
		if (atomic_load(&ProcessCtx->vmtab[i].base) && ProcessCtx->vmtab[i].des >= 0) {
			restore_vmtab_entry(ctx_local, i);
		}
	}
	ProcessCtx = (ProcessContext*) ctx_local;
	assert(ProcessCtx == ctx_local);
	atomic_store(&pStat, true_pstat);
	setup_segv_handler();
}

// restore file backed mappings -- called after files are reopened
void pmap_restore_file_backed (void) {
	assert(!(atomic_load(&pStat) & _FAAI_FRESH_));
	for (unsigned i = 0; i < VMTAB_SIZE; ++i) {
		if (atomic_load(&ProcessCtx->vmtab[i].base) && ProcessCtx->vmtab[i].des < 0) {
			restore_vmtab_entry(ProcessCtx, i);
		}
	}
}

// initializes library for a fresh process
void pmap_fresh_start (struct dso* libc_obj, struct dso* exec_obj) {
	TIMER_START();
	assert(fcntl(ProcessCtx->heap_dir, F_GETFD) != -1 && "Heap dir not set!"); 
	volatile ProcessContext* ctx_local = ProcessCtx; // can't use the global ptr while it's being remapped

	atomic_init(&ctx_local->hides, 0);
	atomic_init(&ctx_local->lodes, 0);
	// add entries for already-mapped objects: context, libc, and probably the executable
	size_t ctx_len_aln = sizeof(ProcessContext) +
		((sizeof(ProcessContext)%PAGE_SIZE) ? (PAGE_SIZE - sizeof(ProcessContext)% PAGE_SIZE) : 0); 
	int ci = add_vmtab_entry(ProcessCtx, ctx_len_aln, 0, 0, -1, 0, -1);
	assert(ProcessCtx->vmtab[ci].mref == MREF_PRELOAD);
	persist_kernel_mapped(ctx_local, libc_obj);
	assert(ProcessCtx == ctx_local);
	if (exec_obj) persist_kernel_mapped(ctx_local, exec_obj);
	setup_segv_handler();
	TIMER_STOP("pmap fresh start");
}

#ifndef REAL_PMEM
// ensure file contents reach disk at "failure". For testing on non-NVDIMM platforms
void pmap_sync_heaps (void) {
	for (int i = 0; i < VMTAB_SIZE; ++i) {
		if (atomic_load(&ProcessCtx->vmtab[i].base) && ProcessCtx->vmtab[i].des >= 0) {
			ERRNO_REPORT_Z_NR(msync, ProcessCtx->vmtab[i].base, ProcessCtx->vmtab[i].len, MS_SYNC);
		}
	}
}
#endif

// delete heap files at exit. Does not unmap them, in case they
// are used by something after our exit code
void pmap_cleanup (void) {
	TIMER_START();
	for (int i = 0; i < VMTAB_SIZE; ++i) {
		if (atomic_load(&ProcessCtx->vmtab[i].base))
			remove_vmtab_entry(i);
	}
	close(ProcessCtx->heap_dir);
	TIMER_STOP("pmap cleanup");
}

int _p_madvise (void* addr, size_t len, int adv) {
	if (adv == MADV_FREE && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		int i = find_vmtab_entry(addr);
		if (i >= 0 && ProcessCtx->vmtab[i].des >= 0) return 0;
	}
	return syscall(SYS_madvise, addr, len, adv);
}

// TODO: brk/sbrk
int _p_brk(void* addr) {
	return __syscall_ret(-ENOMEM);
}

void* _p_sbrk(intptr_t inc) {
	return (void*)__syscall_ret(-ENOMEM);
}

#pragma GCC diagnostic pop
