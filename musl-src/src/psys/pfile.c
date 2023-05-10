#include "psys-impl.h"
#include "syscall.h"
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>

/*
 * General management functions
 */

// this function generates a random filename not present in a given directory
const char namechars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
static char* tmpnamat(int dirfd, size_t len) {
	// TODO: there's a unlikely but possible race between name generation and file creation
	char* name = psys_alloc(len+1);
	// generate filenames until one is found that does not exist in the directory
	// given the namespace size (sizeof(namechars)^len), this should rarely take multiple tries
	do {
		for (unsigned i = 0; i < len; ++i) {
			name[i] = namechars[faai_get_random() % sizeof(namechars)];
		}
		name[len] = '\0';
	} while (faccessat(dirfd, name, F_OK, 0) == 0);

	return name;
}

// gets a fs path from a file descriptor
// TODO: this won't work for non-FS file descriptors. not sure what would happen for those
char* get_fd_path (int fd) {
	// get fd path from symlink in /proc, and open the containing directory
	char procfdpath[20]; // allows up to 5-digit (decimal) fds
	int s = snprintf(procfdpath, 20, "/proc/self/fd/%d", fd);
	assert(s < 20);
	struct stat stat;
	ERRNO_REPORT_Z_NR(lstat, procfdpath, &stat);
	char* fpath = psys_alloc(stat.st_size+1);
	ssize_t sz;
	ERRNO_REPORT_GEZ(sz, readlink, procfdpath, fpath, stat.st_size);	
	// readlink does not null-term
	fpath[stat.st_size] = '\0';
	return fpath;
}

// use this function to change file descriptor types in the table
// it is the serialization point for entries being taken/freed
static void fd_type_exchange(int fd, fd_type_t current, fd_type_t new) {
	assert(fd < FDTAB_SIZE && "Max file count exceeded!");
	if (!atomic_compare_exchange_strong(&ProcessCtx->fdtab[fd].type, &current, new)) {
		fputs("File descriptor collision! Aborting.", stderr);
		abort();
	}
}

static void match_fds(int fd, int re_fd) {
	if (re_fd != fd) {
		assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF && "Restoring fd already in use!");
		int rere_fd = _v_dup2(re_fd, fd);
		assert(rere_fd == fd);
		ERRNO_REPORT_Z_NR(_v_close, re_fd);
	}
}

static void copy_fdtab_entry (int src, int dest) {
	fd_type_t st = atomic_load(&ProcessCtx->fdtab[src].type);	
	assert(st != FD_TYPE_NONE);
	
	fd_type_exchange(dest, FD_TYPE_NONE, st);
	memcpy(&ProcessCtx->fdtab[dest], &ProcessCtx->fdtab[src], sizeof(fd_entry_t));	
}
	
// updates the upper FD bound, where fd was just opened (up == true)
// or about to be closed (up == false)
static void fd_update_hifd(int fd, bool up) {
	int hifd = atomic_load(&ProcessCtx->hifd);
	if (up) {
		while (fd >= hifd && !atomic_compare_exchange_strong(&ProcessCtx->hifd, &hifd, fd+1)) {}
	} else {
		if (hifd == fd+1) {
			--hifd;
			while (atomic_load(&ProcessCtx->fdtab[--hifd].type) == FD_TYPE_NONE) {}
			atomic_compare_exchange_strong(&ProcessCtx->hifd, &fd, hifd+1);
		}
	}
}
	
// This should be called *before* the fd is closed -- race on fd otherwise
static void clear_fd_entry(int fd, fd_type_t type) {
	// release entry
	fd_type_exchange(fd, type, FD_TYPE_NONE);
	// wipe everything except type designator
	//memset(&ProcessCtx->fdtab[fd] + sizeof(fd_type_t), 0, 
	//		sizeof(fd_entry_t) - sizeof(fd_type_t));
}

/*
 * Regular file management via hardlinks
 */
#ifdef __OFF_T_MATCHES_OFF64_T
# define EXTRA_OPEN_FLAGS 0
#else
# define EXTRA_OPEN_FLAGS O_LARGEFILE
#endif

// open and close files without recording them in the table, for internal use
int _v_openat(int dirfd, const char* file, int oflag, ...) {
	mode_t mode = 0;
  if ((oflag & O_CREAT) || (oflag & O_TMPFILE))
    {
      va_list arg;
      va_start (arg, oflag);
      mode = va_arg (arg, mode_t);
      va_end (arg);
    }

  return syscall_cp (SYS_openat, dirfd, file, oflag | EXTRA_OPEN_FLAGS, mode);
}	 

int _v_close(int fd) {
	return syscall_cp(SYS_close, fd);
}

// opens or creates a hidden directory with name HLINK_DIRNAME
// at the root of the FS pointed by device, and stores its fd in
// ProcessCtx->dev2hlink
// undefined if device is not a filesystem
static int add_hlink_dirfd (int fd, dev_t device) {

	// get path to fd directory
	char* fpath = get_fd_path(fd);
	char* term = strrchr(fpath, '/');
	*term = '\0'; // now it's just the path to the directory

	// walk up the directory tree to find the root
	int old, new, vfs_root; // the root dir fd will eventually end up in old
	ERRNO_REPORT_GEZ(vfs_root, _v_openat, AT_FDCWD, "/", O_DIRECTORY | O_PATH);
	struct stat stat;
	ERRNO_REPORT_Z_NR(fstat, vfs_root, &stat);
	if (stat.st_dev == device) { // same FS as VFS root -- found the root!
		old = vfs_root;
	} else {
		ERRNO_REPORT_Z_NR(_v_close, vfs_root);
		ERRNO_REPORT_GEZ(old, _v_openat, AT_FDCWD, fpath, O_DIRECTORY | O_PATH);
		ERRNO_REPORT_GEZ(new, _v_openat, old, "..", O_DIRECTORY | O_PATH);
		ERRNO_REPORT_Z_NR(fstat, new, &stat);
		while (stat.st_dev == device) {
			ERRNO_REPORT_Z_NR(_v_close, old);
			old = new;
			ERRNO_REPORT_GEZ(new, _v_openat, old, "..", O_DIRECTORY | O_PATH);
			ERRNO_REPORT_Z_NR(fstat, new, &stat);
		}
		ERRNO_REPORT_Z_NR(_v_close, new);
	}

	// open or create hlink dir
	int hlink_dir_fd = _v_openat(old, HLINK_DIRNAME, O_DIRECTORY | O_RDONLY);
	if (hlink_dir_fd < 0) {
		ERRNO_REPORT_Z_NR(mkdirat, old, HLINK_DIRNAME, 0600);
		ERRNO_REPORT_GEZ(hlink_dir_fd, _v_openat, old,
				HLINK_DIRNAME, O_DIRECTORY | O_RDONLY);
	}

	// make fds contiguous
	int tmpfd;
	ERRNO_REPORT_GEZ(tmpfd, dup, old);
	ERRNO_REPORT_GEZ_NR(dup2, hlink_dir_fd, old);
	ERRNO_REPORT_Z_NR(_v_close, tmpfd);
	ERRNO_REPORT_Z_NR(_v_close, hlink_dir_fd);
	hlink_dir_fd = old;

	// record the dir fd in dev2hlink
	unsigned idx = atomic_fetch_add(&ProcessCtx->next_d2h, 1);
	assert(idx < D2HTAB_SIZE);
	ProcessCtx->dev2hlink[idx].device = device;
	ProcessCtx->dev2hlink[idx].hlink_dirfd = hlink_dir_fd;

	// record the dir fd in fdtab
	fd_type_exchange(hlink_dir_fd, FD_TYPE_NONE, FD_TYPE_LINKDIR);
	ProcessCtx->fdtab[hlink_dir_fd].E.hd.path = get_fd_path(hlink_dir_fd);

	return hlink_dir_fd;
}

// finds the hlink directory fd for a device, returns -1 if not found
static int find_hlink_dirfd (dev_t device) {
	unsigned ub = atomic_load(&ProcessCtx->next_d2h);
	for (unsigned i = 0; i < ub; ++i) {
		if (ProcessCtx->dev2hlink[i].device == device) {
			return ProcessCtx->dev2hlink[i].hlink_dirfd;
		}
	}

	return -1;
}

static void save_hlink_fd (int fd) {
	assert (atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_HLINK);
	ProcessCtx->fdtab[fd].E.h.off = lseek(fd, 0, SEEK_CUR);
	ProcessCtx->fdtab[fd].E.h.flags = fcntl(fd, F_GETFL);
	if (fcntl(fd, F_GETFD) & FD_CLOEXEC) ProcessCtx->fdtab[fd].E.h.flags |= O_CLOEXEC;
	if (ProcessCtx->fdtab[fd].E.h.flags & (O_WRONLY | O_RDWR)) fsync(fd);
}
	

// reopens an hlink directory recorded in dev2hlink at recovery
static void reinit_linkdir_fd(int fd) {
	// reopen file
	assert(atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_LINKDIR);
	int re_fd;
	ERRNO_REPORT_GEZ(re_fd, _v_openat, AT_FDCWD, ProcessCtx->fdtab[fd].E.hd.path,
			O_PATH | O_DIRECTORY, 0600);
	match_fds(fd, re_fd);

	// get new device number
	struct stat stat;
	ERRNO_REPORT_Z_NR(fstat, fd, &stat);
	int ofd = find_hlink_dirfd(stat.st_dev);
	if (ofd == -1) {
		unsigned ub = atomic_load(&ProcessCtx->next_d2h);
		for (unsigned i = 0; i < ub; ++i) {
			if (ProcessCtx->dev2hlink[i].hlink_dirfd == fd) {
				ProcessCtx->dev2hlink[i].device = stat.st_dev;
			}
		}
	} else assert(ofd == fd);
}

// creates a hardlink to the given fd in its device's hlink
// dir, creating it if it does not exist
static char* fd_make_hlink (int fd, dev_t device) {
	// find hlink dir, creating it if it does not exist
	int fs_hlink_dir = find_hlink_dirfd(device);
	if (fs_hlink_dir < 0) {
		fs_hlink_dir = add_hlink_dirfd(fd, device);
	}
	// create a hardlink with a random name to the file pointed by fd
	char* linkname = tmpnamat(fs_hlink_dir, 7);

	// NB: this can fail if the file currently has no links. unlikely if we just opened it
	char* path = get_fd_path(fd);
	int s = linkat(-1, path, fs_hlink_dir, linkname, 0);
	//free(path);
	if (s != 0) {
		//free(linkname);
		return NULL;
	}

	return linkname;
}

static void fd_delete_hlink (dev_t device, char* name) {	
	int dirfd;
	ERRNO_REPORT_GEZ(dirfd, find_hlink_dirfd, device);
	ERRNO_REPORT_Z_NR(unlinkat, dirfd, name, 0);
	//free(name);
}


#define EXCLUDED_FLAGS (O_CREAT|O_EXCL|O_TRUNC|O_NOCTTY)

// registers a regular file in the fdtab and creates a hardlink to it using fd_make_hlink
static bool register_hlink_fd (int fd, dev_t device, mode_t mode, int flags) {
	// reserve the fd slot
	fd_type_exchange(fd, FD_TYPE_NONE, FD_TYPE_HLINK);

	char* linkname = fd_make_hlink(fd, device);
	if (!linkname) { // switch to unpres
		return false;
	}

	// populate fdtab entry
	ProcessCtx->fdtab[fd].E.h.name = linkname;
	ProcessCtx->fdtab[fd].E.h.device = device;
	ProcessCtx->fdtab[fd].E.h.mode = mode;
	// mask flags that are not idempotent
	ProcessCtx->fdtab[fd].E.h.flags = flags & ~EXCLUDED_FLAGS;
	return true;
}

// reopens a regular file from the fdtab, using its hardlink
static void reinit_hlink_fd (int fd) {
	assert(atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_HLINK);

	int dirfd;
	ERRNO_REPORT_GEZ(dirfd, find_hlink_dirfd, ProcessCtx->fdtab[fd].E.h.device);
	int re_fd; // new file descriptor, should be same as the old one
	ERRNO_REPORT_GEZ(re_fd, _v_openat, dirfd, ProcessCtx->fdtab[fd].E.h.name,
			ProcessCtx->fdtab[fd].E.h.flags);
	match_fds(fd, re_fd);
	ERRNO_REPORT_GEZ_NR(lseek, fd, ProcessCtx->fdtab[fd].E.h.off, SEEK_SET);
}

// removes a regular file entry from the fdtab and deletes its corresponding hardlink
static void dereg_hlink_fd (int fd) {
	assert(atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_HLINK);

	dev_t device = ProcessCtx->fdtab[fd].E.h.device;
	char* name = ProcessCtx->fdtab[fd].E.h.name;

	// release the fdtab entry
	clear_fd_entry(fd, FD_TYPE_HLINK);

	// delete the hlink
	fd_delete_hlink(device, name);
}

// should be called by mmap wrapper when a fd is mapped.
// records the fd in ProcessCtx->sotab so that the hardlink
// will not be deleted when the fd is closed
unsigned set_fd_mapped(int fd) {
	assert(fd >= 0);
	if (atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_MAPPED) { // inc reference count
		atomic_fetch_add(&ProcessCtx->mrtab[ProcessCtx->fdtab[fd].E.m.refidx].refcnt, 1);
		return ProcessCtx->fdtab[fd].E.m.refidx;
	} else { // create mapping
		// copy over fdtab entry and then convert it into a ref
		unsigned idx = atomic_fetch_add(&ProcessCtx->next_mrtab, 1);
		assert(idx < VMTAB_SIZE);
		if (atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_HLINK) {
			ProcessCtx->mrtab[idx].ftype = FD_TYPE_HLINK;
			memcpy(&ProcessCtx->mrtab[idx].file.h, &ProcessCtx->fdtab[fd].E.h, sizeof(hlink_fd_entry));
		} else {
			assert(atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_UNPRES);
			ProcessCtx->mrtab[idx].ftype = FD_TYPE_UNPRES;
			memcpy(&ProcessCtx->mrtab[idx].file.u, &ProcessCtx->fdtab[fd].E.u, sizeof(unpres_fd_entry));
		}
		fd_type_exchange(fd, FD_TYPE_UNPRES, FD_TYPE_MAPPED);
		atomic_store(&ProcessCtx->mrtab[idx].refcnt, 2); // open file ref + mapper ref
		ProcessCtx->mrtab[idx].ofd = fd;
		ProcessCtx->fdtab[fd].E.m.refidx = idx;
		return idx;
	}
}

void add_mref(unsigned mref) {
	assert(mref < VMTAB_SIZE);
	unsigned cnt = atomic_fetch_add(&ProcessCtx->mrtab[mref].refcnt, 1);
	assert(cnt > 0);
}

void subtract_mref(unsigned mref) {
	assert(mref < VMTAB_SIZE);
	// decrement ref count, return if > 0
	if (atomic_fetch_sub(&ProcessCtx->mrtab[mref].refcnt, 1) > 1)
		return;
	
	int ofd = ProcessCtx->mrtab[mref].ofd;
	if (atomic_load(&ProcessCtx->fdtab[ofd].type) == FD_TYPE_MAPPED &&
			ProcessCtx->fdtab[ofd].E.m.refidx == mref) { // transform back to fdtab entry
		fd_type_exchange(ofd, FD_TYPE_MAPPED, FD_TYPE_HLINK);
		memcpy(&ProcessCtx->fdtab[ofd].E.h, &ProcessCtx->mrtab[mref].file.h, sizeof(hlink_fd_entry));
	} else if (ProcessCtx->mrtab[mref].ftype == FD_TYPE_HLINK) {
		fd_delete_hlink(ProcessCtx->mrtab[mref].file.h.device, ProcessCtx->mrtab[mref].file.h.name);
	} else {
		assert(ProcessCtx->mrtab[mref].ftype == FD_TYPE_UNPRES);
	}
}

int get_temp_mref (unsigned idx) {
	int ofd = ProcessCtx->mrtab[idx].ofd;
	if ((atomic_load(&ProcessCtx->fdtab[ofd].type) == FD_TYPE_MAPPED) &&
		(ProcessCtx->fdtab[ofd].E.m.refidx == idx) ) {
		return ofd;
	} else if (atomic_load(&ProcessCtx->mrtab[idx].ftype) == FD_TYPE_HLINK) {
		int dirfd;
		ERRNO_REPORT_GEZ(dirfd, find_hlink_dirfd, ProcessCtx->mrtab[idx].file.h.device);
		int fd;
		ERRNO_REPORT_GEZ(fd, _v_openat, dirfd, ProcessCtx->mrtab[idx].file.h.name,
				ProcessCtx->mrtab[idx].file.h.flags, 0);
		return fd;
	} else {
		assert(atomic_load(&ProcessCtx->mrtab[idx].ftype) == FD_TYPE_UNPRES);
		int fd;
		ERRNO_REPORT_GEZ(fd, _v_openat, ProcessCtx->mrtab[idx].file.u.dirfd,
			ProcessCtx->mrtab[idx].file.u.path, ProcessCtx->mrtab[idx].file.u.flags,
			ProcessCtx->mrtab[idx].file.u.mode);
		return fd;
	}		
}

void free_temp_mref(int fd) {
	if ((atomic_load(&ProcessCtx->fdtab[fd].type) != FD_TYPE_MAPPED) ||
		(ProcessCtx->mrtab[ProcessCtx->fdtab[fd].E.m.refidx].ofd != fd) ) {
		//_v_close(fd);
		syscall(SYS_close, fd); // avoid cancellation point during startup
	}
}

/*
 * Unpreserved file management
 */

// registers a file we do not attempt to preserve, just reopen at recovery
static void register_unpres_fd (int fd, int dirfd, const char* path, mode_t mode, int flags) {
	fd_type_exchange(fd, FD_TYPE_NONE, FD_TYPE_UNPRES);
	ProcessCtx->fdtab[fd].E.u.mode = mode;
	ProcessCtx->fdtab[fd].E.u.flags = flags & ~EXCLUDED_FLAGS;
	ProcessCtx->fdtab[fd].E.u.dirfd = dirfd;
	size_t l = strlen(path) + 1;
	char* cpath = psys_alloc(l);
	strncpy(cpath, path, l);
	ProcessCtx->fdtab[fd].E.u.path = cpath;
}

// no dereg function, clearing the entry is sufficient

// reopens an unpreserved file at recovery
static void reinit_unpres_fd (int fd) {
	assert(atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_UNPRES);

	int re_fd;
	ERRNO_REPORT_GEZ(re_fd, _v_openat, ProcessCtx->fdtab[fd].E.u.dirfd,
			ProcessCtx->fdtab[fd].E.u.path, ProcessCtx->fdtab[fd].E.u.flags,
			ProcessCtx->fdtab[fd].E.u.mode);
	match_fds(fd, re_fd);
}

/*
 * Socket management
 */
static int _v_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);

// registers a socket fd in the fdtab
static void register_sock_fd (int fd, int domain, int type, int protocol) {
	fd_type_exchange(fd, FD_TYPE_NONE, FD_TYPE_SOCK);
	ProcessCtx->fdtab[fd].E.s.type = type;
	ProcessCtx->fdtab[fd].E.s.protocol = protocol;
	ProcessCtx->fdtab[fd].E.s.domain = domain;
	ProcessCtx->fdtab[fd].E.s.addr = NULL;
	ProcessCtx->fdtab[fd].E.s.opt = NULL;
}

// this is called when a sock is either connected or bound
static void assign_sock_fd (int fd, const struct sockaddr* addr, socklen_t len, bool conn) {
	struct sockaddr_wrap* a = psys_alloc(sizeof(struct sockaddr_wrap));
	a->addrlen = len;
	a->lbacklog = -1;
	memcpy(&a->addr, addr, len);
	if (conn) ProcessCtx->fdtab[fd].E.s.domain = -1;

	assert(atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_SOCK);
	ProcessCtx->fdtab[fd].E.s.addr = a;
}

// reopens/rebinds a socket at recovery
static void reinit_sock_fd(int fd) {
	assert(atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_SOCK);
	
	int af;
	if (ProcessCtx->fdtab[fd].E.s.addr) {
		af = ProcessCtx->fdtab[fd].E.s.addr->addr.ss_family; 
	} else {
		af = ProcessCtx->fdtab[fd].E.s.domain;
	}
	assert(af > 0);

	int re_fd = socketcall(socket, af, ProcessCtx->fdtab[fd].E.s.type,
			ProcessCtx->fdtab[fd].E.s.protocol, 0, 0, 0);
	match_fds(fd, re_fd);

	// do pre-assign options
	for (struct sockopt* opt = ProcessCtx->fdtab[fd].E.s.opt; opt; opt = opt->next) {
		if (!opt->post_assign)
			ERRNO_REPORT_Z_NR(_v_setsockopt, fd, opt->level, opt->option, opt->value, opt->len);
	}

	if (ProcessCtx->fdtab[fd].E.s.addr) { // socket was bound/connected
		if (ProcessCtx->fdtab[fd].E.s.domain > 0) { // socket was bound
			ERRNO_REPORT_Z_NR(socketcall, bind, fd, 
					(struct sockaddr*) &ProcessCtx->fdtab[fd].E.s.addr->addr,
					ProcessCtx->fdtab[fd].E.s.addr->addrlen, 0, 0, 0);
			if (ProcessCtx->fdtab[fd].E.s.addr->lbacklog >= 0) { // socket was listening
				ERRNO_REPORT_Z_NR(socketcall, listen, fd, ProcessCtx->fdtab[fd].E.s.addr->lbacklog, 0, 0, 0, 0);
			}
		} else { // socket was connected.
			ERRNO_REPORT_Z_NR(socketcall_cp, connect, fd, 
					(struct sockaddr*) &ProcessCtx->fdtab[fd].E.s.addr->addr,
					ProcessCtx->fdtab[fd].E.s.addr->addrlen, 0, 0, 0);
		}
		// do post-assign options
		for (struct sockopt* opt = ProcessCtx->fdtab[fd].E.s.opt; opt; opt = opt->next) {
			if (opt->post_assign)
				ERRNO_REPORT_Z_NR(_v_setsockopt, fd, opt->level, opt->option, opt->value, opt->len);
		}
	}
}

// removes a socket fd entry from the fdtab
static void dereg_sock_fd (int fd) {
	struct sockopt* opt = ProcessCtx->fdtab[fd].E.s.opt;
	
	while (opt) {
		struct sockopt* popt = opt->next;
		//free(opt);
		opt = popt;
	}
	struct sockaddr_wrap* a = ProcessCtx->fdtab[fd].E.s.addr;
	clear_fd_entry(fd, FD_TYPE_SOCK);

	//if (a) free(a);
}


/*
 * Epoll fd management
 */

static void reinit_epoll_fd (int fd) {
	epoll_fd_entry* ent = &ProcessCtx->fdtab[fd].E.e;
	int re_fd;	
	ERRNO_REPORT_GEZ(re_fd, syscall, SYS_epoll_create1, ent->flag);
	match_fds(fd, re_fd);
	
	for (struct pollop* op = ent->oplist; op; op = op->next)
		ERRNO_REPORT_Z_NR(syscall, SYS_epoll_ctl, fd, EPOLL_CTL_ADD, op->fd, &op->event);
}

static void dereg_epoll_fd (int fd) {
	struct pollop* op = ProcessCtx->fdtab[fd].E.e.oplist;
	while (op) {
		struct pollop* pop = op->next;
		//free(op);
		op = pop;
	}

	clear_fd_entry(fd, FD_TYPE_EPOLL);
}

/*
 * Pipe fd management
 */

static pipe_fd_entry* register_pipe_common(int fd, int flags, bool alloc_buf) {
	fd_type_exchange(fd, FD_TYPE_NONE, FD_TYPE_PIPE);
	pipe_fd_entry* ent = &ProcessCtx->fdtab[fd].E.p;

	if (alloc_buf) {
		ent->slurp.iov_base = mmap(NULL, SLURP_BUF_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		assert(ent->slurp.iov_base != MAP_FAILED);
		ent->slurp.iov_len = 0;
	} else {
		ent->slurp.iov_base = NULL;
		ent->slurp.iov_len = 0;
	}
	ent->flags = flags;

	return ent;
}

static void register_anon_pipe (int fd, int flags, int buddy) {
	pipe_fd_entry* ent = register_pipe_common(fd, flags, !(flags & O_WRONLY));
	
	ent->mode = 0;
	ent->path = NULL;
	assert(buddy >= 0);
	ent->buddy = buddy;
}

static void register_named_pipe(int fd, int flags, int dirfd, const char* path, int mode) {
	assert(!(flags & (O_CREAT|O_EXCL|O_DIRECTORY|O_PATH)));
	pipe_fd_entry* ent = register_pipe_common(fd, flags, true);
	
	ent->flags = flags;
	ent->mode = mode;
	ent->dirfd = dirfd;
	ent->path = psys_alloc(strlen(path)+1);
	memcpy(ent->path, path, strlen(path)+1);	 
	ent->buddy = -1;
}

static void save_pipe_fd (int fd) {
	pipe_fd_entry* ent = &ProcessCtx->fdtab[fd].E.p;
	if (ent->buddy >= 0 && ent->buddy < fd) return; // already dealt with for buddy

	int unread;
	ERRNO_REPORT_Z_NR(ioctl, fd, FIONREAD, &unread);
	assert(unread >= 0);
	if (unread == 0) return;
	else ent->slurp.iov_len = (size_t) unread;
	assert(unread <= SLURP_BUF_SIZE);

	int rdend = -1;
	struct iovec* vec = NULL;
	if (!(ent->flags & O_WRONLY)) { // named or anonymous reader
		rdend = fd;
		vec = &ent->slurp;
	} else if (ent->buddy >= 0) { // anonymous writer, buddy is reader
		rdend = ent->buddy;
		vec = &ProcessCtx->fdtab[rdend].E.p.slurp;
	} else if (ent->path) { // named writer
		vec = &ent->slurp;
		ERRNO_REPORT_GEZ(rdend, _v_openat, ent->dirfd, ent->path, (ent->flags & ~O_WRONLY) | O_RDWR, 0); 
	} else { // anonymous writer, abandoned
		return;
	}

	ssize_t s = vmsplice(rdend, vec, 1, 0);
	assert(s == unread); 
	
	if (ent->buddy < 0 && ent->flags & O_WRONLY && ent->path) _v_close(rdend);
}

static void reinit_pipe_fd (int fd) {
	pipe_fd_entry* ent = &ProcessCtx->fdtab[fd].E.p;
	if (ent->buddy >= 0 && ent->buddy < fd) return; // already dealt with for buddy
	
	int wrend = -1;
	struct iovec* vec = NULL;
	if (ent->path) { // named pipe
		int re_fd = _v_openat(ent->dirfd, ent->path, ent->flags, 0);
		if (re_fd < 0) // need to recreate
			ERRNO_REPORT_Z_NR(mkfifoat, ent->dirfd, ent->path, ent->mode);
		ERRNO_REPORT_GEZ(re_fd, _v_openat, ent->dirfd, ent->path, ent->flags, 0);
		match_fds(fd, re_fd);

		vec = &ent->slurp;
		if (ent->flags & O_WRONLY) wrend = fd;
		else ERRNO_REPORT_GEZ(wrend, _v_openat, ent->dirfd, ent->path, ent->flags | O_WRONLY, 0);
	} else { // anon pipe
		int re_fds[2];
		ERRNO_REPORT_Z_NR(_v_pipe2, re_fds, ent->flags & ~(O_RDWR|O_WRONLY));
		
		// have to move the larger of the two fds first, because it takes 
		// up the spot for the smaller one
		if (ent->flags & O_WRONLY) { // this is writer
			if (fd > ent->buddy) { 
				match_fds(ent->buddy, re_fds[0]);
				match_fds(fd, re_fds[1]);
			} else if (ent->buddy < 0) { // abandoned writer -- easy to simulate
				ERRNO_REPORT_Z_NR(_v_close, re_fds[0]);
				match_fds(fd, re_fds[1]);
				return;
			} else {
				match_fds(fd, re_fds[1]);
				match_fds(ent->buddy, re_fds[0]);
			}

			wrend = fd;
			vec = &ProcessCtx->fdtab[ent->buddy].E.p.slurp;
		} else { // buddy is writer
			if (ent->buddy > fd) {
				match_fds(ent->buddy, re_fds[1]);
				match_fds(fd, re_fds[0]);
			} else if (ent->buddy < 0) { // abandoned reader
				match_fds(fd, re_fds[0]);
				wrend = re_fds[1];
			} else {
				match_fds(fd, re_fds[0]);
				match_fds(ent->buddy, re_fds[1]);
				wrend = ent->buddy;
			}
			vec = &ent->slurp;
		 }
	}

	assert(vec->iov_base);
	if (vec->iov_len != 0) {
		ssize_t s = vmsplice(wrend, vec, 1, SPLICE_F_GIFT);
		assert(s == vec->iov_len);	
		ERRNO_REPORT_Z_NR(munmap, vec->iov_base, SLURP_BUF_SIZE);
		vec->iov_base = mmap(NULL, SLURP_BUF_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
		assert(vec->iov_base != MAP_FAILED);
		vec->iov_len = 0;
	}

	if (!(ent->flags & O_WRONLY) && ent->buddy < 0) ERRNO_REPORT_Z_NR(_v_close, wrend);
}
		
static void dereg_pipe_fd (int fd) {
	pipe_fd_entry* ent = &ProcessCtx->fdtab[fd].E.p;
	
	//if (ent->path) pfree(ent->path);
	if (ent->slurp.iov_base) munmap(ent->slurp.iov_base, SLURP_BUF_SIZE);

	if (ent->buddy != -1 && atomic_load(&ProcessCtx->fdtab[ent->buddy].type) == FD_TYPE_PIPE) {
		ProcessCtx->fdtab[ent->buddy].E.p.buddy = -1;
	}
	clear_fd_entry(fd, FD_TYPE_PIPE);
} 
	
/*
 * Ctors/Dtor/Top-level failure handler
 */

void pfile_fresh_start(void) {
	TIMER_START();
	assert (atomic_load(&pStat) & _FAAI_FRESH_);
	atomic_init(&ProcessCtx->next_mrtab, 0);
	atomic_init(&ProcessCtx->next_d2h, 0);
	for (int fd = 0; fd < 3; ++fd)
		fd_type_exchange(fd, FD_TYPE_NONE, FD_TYPE_STD);
	fd_type_exchange(ProcessCtx->heap_dir, FD_TYPE_NONE, FD_TYPE_STD);
#ifdef TIME_PSYS
	if (__timingact)
		fd_type_exchange(fileno(__timingdat), FD_TYPE_NONE, FD_TYPE_STD);
#endif
	TIMER_STOP("pfile init");
}

// restore saved file tables
void pfile_restore (void) {
	assert (!(atomic_load(&pStat) & _FAAI_FRESH_));
	TIMER_START();
	int fd_ub = atomic_load(&ProcessCtx->hifd);
	// first pass reinitializes link dirs only
	for (int fd = 3; fd < fd_ub; ++fd) {
		if (atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_LINKDIR) {
			reinit_linkdir_fd(fd);
		}
	}

	// second pass gets everything except epolls (because they depend on other fds)
	for (int fd = 3; fd < fd_ub; ++fd) {
		switch (atomic_load(&ProcessCtx->fdtab[fd].type)) {
		case FD_TYPE_UNPRES:
			reinit_unpres_fd(fd);
			break;
		case FD_TYPE_HLINK:
		case FD_TYPE_MAPPED:
			reinit_hlink_fd(fd);
			break;
		case FD_TYPE_SOCK:
			reinit_sock_fd(fd);
			break;
		case FD_TYPE_PIPE:
			reinit_pipe_fd(fd);
		case FD_TYPE_EPOLL:
		case FD_TYPE_LINKDIR:
		case FD_TYPE_STD:
		case FD_TYPE_NONE:
			break;
			// these are a no-op
		}
	}	
	
	// third pass gets epolls
	for (int fd = 3; fd < fd_ub; ++fd) {
		if (atomic_load(&ProcessCtx->fdtab[fd].type) == FD_TYPE_EPOLL)
			reinit_epoll_fd(fd);
	}
	TIMER_STOP("pfile restore");
}

// deregisters all fds and deletes corresponding hardlinks
void cleanup_fd_tracking(void) {
	TIMER_START();
	int fd_ub = atomic_load(&ProcessCtx->hifd);
	for (int fd = 0; fd < fd_ub; ++fd) {
		switch(atomic_load(&ProcessCtx->fdtab[fd].type)) {
		case FD_TYPE_HLINK:
			dereg_hlink_fd(fd);
			break;
		case FD_TYPE_SOCK:
			dereg_sock_fd(fd);
			break;	
		case FD_TYPE_MAPPED:
		case FD_TYPE_UNPRES:
			//free(ProcessCtx->fdtab[fd].E.u.path);
			break;
		case FD_TYPE_PIPE:
			dereg_pipe_fd(fd);
			break;
		case FD_TYPE_LINKDIR:
		case FD_TYPE_STD:
		case FD_TYPE_NONE:
			break;
			// no-op, assuming pmem file gets deleted
		}
	}
	TIMER_STOP("pfile cleanup");
}

void save_fd_tracking (void) {
	TIMER_START();
	int fd_ub = atomic_load(&ProcessCtx->hifd);
	for (int fd = 0; fd < fd_ub; ++fd) {
		fd_type_t type = atomic_load(&ProcessCtx->fdtab[fd].type); 
		switch (type) {
		case FD_TYPE_HLINK:
			save_hlink_fd(fd);
			break;
		case FD_TYPE_PIPE:
			save_pipe_fd(fd);
			break;
		default:
			break;
		}
	}
	TIMER_STOP("pfile save");
}

// NB: this assumes we are the owner
static int mode2accflags (mode_t mode) {
	int flags = F_OK;
	if (mode & S_IRUSR) flags |= R_OK;
	if (mode & S_IWUSR) flags |= W_OK;
	if (mode & S_IXUSR) flags |= X_OK;
	return flags;
}


// validate fdtab. May fail spuriously if fds have not been sync'd (and i.e. flags have changed)
void fdtab_is_valid(void) {
	int fd;
	for (fd = 0; fd < atomic_load(&ProcessCtx->hifd)-1; ++fd) {
		fd_type_t type = atomic_load(&ProcessCtx->fdtab[fd].type);
		if (type != FD_TYPE_NONE) {
			assert(fcntl(fd, F_GETFD) != -1);
			//int flags = 0;

			switch (type) {
			case FD_TYPE_HLINK:
				assert(is_pmem(ProcessCtx->fdtab[fd].E.h.name));
				assert(faccessat(find_hlink_dirfd(ProcessCtx->fdtab[fd].E.h.device), ProcessCtx->fdtab[fd].E.h.name,
							mode2accflags(ProcessCtx->fdtab[fd].E.h.mode), 0) == 0);
			//	flags = ProcessCtx->fdtab[fd].E.h.flags;
				break;
			case FD_TYPE_LINKDIR:
				assert(is_pmem(ProcessCtx->fdtab[fd].E.hd.path));
				assert(access(ProcessCtx->fdtab[fd].E.hd.path, R_OK | W_OK) == 0);
			//	flags = O_DIRECTORY | O_RDONLY; 
				break;
			case FD_TYPE_UNPRES:
				assert(is_pmem(ProcessCtx->fdtab[fd].E.u.path));
				assert(faccessat(ProcessCtx->fdtab[fd].E.u.dirfd, ProcessCtx->fdtab[fd].E.u.path,
						mode2accflags(ProcessCtx->fdtab[fd].E.u.mode), 0) == 0);
			//	flags = ProcessCtx->fdtab[fd].E.u.flags;
				break;
			default:
				break;
				// TODO: sockets
			}

			//if (flags) assert((fcntl(fd, F_GETFL) & ~EXCLUDED_FLAGS) == (flags & ~O_CLOEXEC));
			//if (flags & O_CLOEXEC) assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
		}
	}
}


/*
 * Interceptor functions
 */

// creat/open are redirected here
int _p_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
	FAAI_CHECK_INIT();
	TIMER_START();

	faai_startcrit(); // OS-level opening and recording in fdtab must be atomic wrt failure

	int fd = syscall_cp(SYS_openat, dirfd, pathname, flags, mode);

	if (fd >= 0 && atomic_load(&pStat) & _FAAI_ACTIVE_) {
		fd_update_hifd(fd, true);
		struct stat stat;
		ERRNO_REPORT_Z_NR(fstat, fd, &stat);

		/*if (S_ISREG(stat.st_mode) && ((flags & O_WRONLY) || (flags & O_RDWR))) {
			bool success = register_hlink_fd(fd, stat.st_dev, mode, flags);
			if (!success) {
				fd_type_exchange(fd, FD_TYPE_HLINK, FD_TYPE_NONE);
				register_unpres_fd(fd, dirfd, pathname, mode, flags);
			}	
		} else*/ if (S_ISFIFO(stat.st_mode)) {
			register_named_pipe(fd, flags, dirfd, pathname, stat.st_mode);
		} else {
			register_unpres_fd(fd, dirfd, pathname, mode, flags);
		}
	}

	faai_endcrit();
	TIMER_STOP("openat");
	return fd;
}

int _p_close (int fd) {
	FAAI_CHECK_INIT();
	TIMER_START();

	faai_startcrit();
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		fd_update_hifd(fd, false);
		switch(atomic_load(&ProcessCtx->fdtab[fd].type)) {
		// for these, just clear the entry
		case FD_TYPE_STD:
			clear_fd_entry(fd, FD_TYPE_STD);
			break;
		case FD_TYPE_UNPRES:
			clear_fd_entry(fd, FD_TYPE_UNPRES);
			break;
		case FD_TYPE_MAPPED: ;
			unsigned refidx = ProcessCtx->fdtab[fd].E.m.refidx;
			clear_fd_entry(fd, FD_TYPE_MAPPED);
			subtract_mref(refidx); // decrement reference count
			break;
		case FD_TYPE_LINKDIR:
			clear_fd_entry(fd, FD_TYPE_LINKDIR);
			break;
		case FD_TYPE_HLINK:
			dereg_hlink_fd(fd);
			break;
		case FD_TYPE_SOCK:
			dereg_sock_fd(fd);
			break;
		case FD_TYPE_EPOLL:
			dereg_epoll_fd(fd);
			break;
		case FD_TYPE_PIPE:
			dereg_pipe_fd(fd);
			break;
		case FD_TYPE_NONE:
			fputs("Closing unknown fd!", stderr);
			abort();
		}
	}

	
	int r = syscall_cp(SYS_close, fd);
	faai_endcrit();
	TIMER_STOP("close()");
	return r;
}

static int orig_socket(int domain, int type, int protocol) {
	int s = __socketcall(socket, domain, type, protocol, 0, 0, 0);
	if ((s==-EINVAL || s==-EPROTONOSUPPORT)
	    && (type&(SOCK_CLOEXEC|SOCK_NONBLOCK))) {
		s = __socketcall(socket, domain,
			type & ~(SOCK_CLOEXEC|SOCK_NONBLOCK),
			protocol, 0, 0, 0);
		if (s < 0) return __syscall_ret(s);
		if (type & SOCK_CLOEXEC)
			__syscall(SYS_fcntl, s, F_SETFD, FD_CLOEXEC);
		if (type & SOCK_NONBLOCK)
			__syscall(SYS_fcntl, s, F_SETFL, O_NONBLOCK);
	}
	return __syscall_ret(s);
}
		
int _p_socket(int domain, int type, int protocol) {
	FAAI_CHECK_INIT();
	TIMER_START();

	faai_startcrit();
	int fd = orig_socket(domain, type, protocol);
	if (fd >= 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		fd_update_hifd(fd, true);
		register_sock_fd(fd, domain, type, protocol);
	}	
	faai_endcrit();
	TIMER_STOP("socket()");
	return fd;
}

int _p_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
	FAAI_CHECK_INIT();
	TIMER_START();

	faai_startcrit();
	int s = socketcall(bind, sockfd, addr, addrlen, 0, 0, 0);
	if (s == 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) assign_sock_fd(sockfd, addr, addrlen, false);
	faai_endcrit();
	TIMER_STOP("bind()");
	return s;
}

int _p_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
	FAAI_CHECK_INIT();
	TIMER_START();

	faai_startcrit();
	int s = socketcall(connect, sockfd, addr, addrlen, 0, 0, 0);
	if (s == 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) assign_sock_fd(sockfd, addr, addrlen, true);
	faai_endcrit();
	TIMER_STOP("connect()");
	return s;
}

int _p_dup(int src) {
	TIMER_START();
	faai_startcrit();
	int dest = syscall(SYS_dup, src);
	if (dest >= 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_))
		copy_fdtab_entry(src, dest);
	faai_endcrit();
	TIMER_STOP("dup()");
	return dest;
}

int _p_dup2(int src, int dest) {
	TIMER_START();
	faai_startcrit();
	int s = syscall(SYS_dup2, src, dest);
	if (s >= 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		assert(s == dest);
		copy_fdtab_entry(src, dest);
	}
	faai_endcrit();
	TIMER_STOP("dup2()");
	return s;
}

int _p_accept(int ofd, struct sockaddr* addr, socklen_t* addrlen) {
	TIMER_START();
	faai_startcrit();
	int nfd = socketcall_cp(accept, ofd, addr, addrlen, 0, 0, 0);
	fd_entry_t* oldent = &ProcessCtx->fdtab[ofd];
	if (nfd >= 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		register_sock_fd(nfd, oldent->E.s.domain, oldent->E.s.type, oldent->E.s.protocol);
		if (!addr) {
			addr = (struct sockaddr*) &oldent->E.s.addr->addr;
			addrlen = &oldent->E.s.addr->addrlen;
		}
		assign_sock_fd(nfd, addr, *addrlen, true);
	}
	faai_endcrit();
	TIMER_STOP("accept()");
	return nfd;
}

int _p_accept4(int ofd, struct sockaddr* addr, socklen_t* addrlen, int flag) {
	
	TIMER_START();
	faai_startcrit();
	int nfd = socketcall_cp(accept4, ofd, addr, addrlen, flag, 0, 0);
	fd_entry_t* oldent = &ProcessCtx->fdtab[ofd];
	if (nfd >= 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		register_sock_fd(nfd, oldent->E.s.domain, oldent->E.s.type, oldent->E.s.protocol);
		if (!addr) {
			addr = (struct sockaddr*) &oldent->E.s.addr->addr;
			addrlen = &oldent->E.s.addr->addrlen;
		}
		assign_sock_fd(nfd, addr, *addrlen, true);
	}
	faai_endcrit();
	TIMER_STOP("accept4()");
	return nfd;
}

int _p_listen(int fd, int blog) {
	TIMER_START();
	faai_startcrit();
	int s = socketcall(listen, fd, blog, 0, 0, 0, 0);
	if (s == 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) 
		ProcessCtx->fdtab[fd].E.s.addr->lbacklog = blog;	
	faai_endcrit();
	TIMER_STOP("listen()");
	return s;
}

#define IS32BIT(x) !((x)+0x80000000ULL>>32)
#define CLAMP(x) (int)(IS32BIT(x) ? (x) : 0x7fffffffU+((0ULL+(x))>>63))
#include <sys/time.h>

static int _v_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	const struct timeval *tv;
	time_t s;
	suseconds_t us;

	int r = __socketcall(setsockopt, fd, level, optname, optval, optlen, 0);

	if (r==-ENOPROTOOPT) switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_RCVTIMEO:
		case SO_SNDTIMEO:
			if (SO_RCVTIMEO == SO_RCVTIMEO_OLD) break;
			if (optlen < sizeof *tv) return __syscall_ret(-EINVAL);
			tv = optval;
			s = tv->tv_sec;
			us = tv->tv_usec;
			if (!IS32BIT(s)) return __syscall_ret(-ENOTSUP);

			if (optname==SO_RCVTIMEO) optname=SO_RCVTIMEO_OLD;
			if (optname==SO_SNDTIMEO) optname=SO_SNDTIMEO_OLD;

			r = __socketcall(setsockopt, fd, level, optname,
				((long[]){s, CLAMP(us)}), 2*sizeof(long), 0);
			break;
		case SO_TIMESTAMP:
		case SO_TIMESTAMPNS:
			if (SO_TIMESTAMP == SO_TIMESTAMP_OLD) break;
			if (optname==SO_TIMESTAMP) optname=SO_TIMESTAMP_OLD;
			if (optname==SO_TIMESTAMPNS) optname=SO_TIMESTAMPNS_OLD;
			r = __socketcall(setsockopt, fd, level,
				optname, optval, optlen, 0);
			break;
		}
	}
	return __syscall_ret(r);
}

int _p_setsockopt(int fd, int level, int option, const void* value, socklen_t len) {
	
	TIMER_START();
	faai_startcrit();
	int s = _v_setsockopt(fd, level, option, value, len);

	if (s == 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		// buffer also holds a copy of the provided value
		struct sockopt* newopt = psys_alloc(sizeof(struct sockopt) + len);
		newopt->level = level;
		newopt->option = option;
		newopt->value = (void*) ((char*) newopt + sizeof(struct sockopt));
		memcpy(newopt->value, value, len);
		newopt->len = len;
		newopt->post_assign = (bool) (ProcessCtx->fdtab[fd].E.s.addr);
		newopt->next = ProcessCtx->fdtab[fd].E.s.opt;
		ProcessCtx->fdtab[fd].E.s.opt = newopt;	
	}
	faai_endcrit();
	TIMER_STOP("setsockopt");
	return s;
}
	
int _p_epoll_create1 (int flag) {
	TIMER_START();
	int fd = syscall(SYS_epoll_create1, flag);
	if (fd >= 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		fd_type_exchange(fd, FD_TYPE_NONE, FD_TYPE_EPOLL);
		ProcessCtx->fdtab[fd].E.e.flag = flag;
		ProcessCtx->fdtab[fd].E.e.oplist = NULL;
	}
	TIMER_STOP("epoll_create");
	return fd;
}

int _p_epoll_ctl (int fd, int op, int fd2, struct epoll_event* ev) {
	TIMER_START();
	int s = syscall(SYS_epoll_ctl, fd, op, fd2, ev);
	if (s == 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		epoll_fd_entry* ent = &ProcessCtx->fdtab[fd].E.e;
		if (op == EPOLL_CTL_ADD) {
			struct pollop* newop = psys_alloc(sizeof(struct pollop));
			newop->fd = fd2;
			newop->event = *ev;
			newop->next = ent->oplist;
			ent->oplist = newop;
		} else {
			// find existing op
			struct pollop* o, *prevop = NULL;
			for (o = ent->oplist; o; o = o->next) {
				if (o->fd == fd2) break;
				prevop = o;
			}
			assert(o);
			
			if (op == EPOLL_CTL_DEL) {
				if (prevop) prevop->next = o->next;
				else ent->oplist = o->next;
				//free(o);
			} else { // EPOLL_CTL_MOD
				o->event = *ev;
			}
		}
	}
	TIMER_STOP("epoll_ctl");
	return s;
}

int _p_pipe2 (int pipefd[2], int flags) {
	TIMER_START();
	int s = syscall(SYS_pipe2, pipefd, flags);
	if (s == 0 && (atomic_load(&pStat) & _FAAI_ACTIVE_)) {
		register_anon_pipe(pipefd[0], flags | O_RDWR, pipefd[1]);
		register_anon_pipe(pipefd[1], flags | O_WRONLY, pipefd[0]);
	}
	TIMER_STOP("pipe()");
	return s;
}

int _p_fcntl(int fd, int cmd, unsigned long arg) {
	TIMER_START();
	if (atomic_load(&pStat) & _FAAI_ACTIVE_) {
		int ret;
		switch (cmd) {
		case F_DUPFD:
			return _p_dup(fd);
		case F_DUPFD_CLOEXEC:
			ret = syscall(SYS_fcntl, fd, F_DUPFD_CLOEXEC, 0);
			if (ret < 0) return ret;
			
			copy_fdtab_entry(fd, ret);
			return ret;
		default:
			return syscall(SYS_fcntl, fd, cmd, arg);
		}
	} else return syscall(SYS_fcntl, fd, cmd, arg);
	TIMER_STOP("fcntl");
}

