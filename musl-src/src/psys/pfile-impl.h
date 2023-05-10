#if !defined(_PSYS_IMPL_H_) || defined(_PFILE_IMPL_H_)
#error "This is not a standalone header! Include psys-impl.h instead."
#else
#define _PFILE_IMPL_H_
#endif

#define FDTAB_SIZE 1024
#define SOTAB_SIZE 32
#define D2HTAB_SIZE 32
#define HLINK_DIRNAME ".nvmhlinks"

#include <sys/uio.h>

typedef enum fd_type {
	FD_TYPE_NONE = 0, // nothing (that we know of) with this descriptor
	FD_TYPE_STD, // standard file descriptors; we expect these to be restored by default
	FD_TYPE_UNPRES, // we do not attempt to preserve this file, just reopen with same args
	FD_TYPE_HLINK, // file preserved via hardlink, in the relevant .nvmhlinks
	FD_TYPE_MAPPED, // mmaped file. like normal but we don't delete the hardlink when closed
	FD_TYPE_SOCK, // a socket; rebound or reconnected on restart
	FD_TYPE_LINKDIR, // a directory holding files preserved via hardlink; handled specially
	FD_TYPE_EPOLL, // epoll fd; recreated and repopulated on restart
	FD_TYPE_PIPE // pipe/fifo fd; unread contents restored on restart
} fd_type_t;

struct sockaddr_wrap {
	socklen_t addrlen;
	struct sockaddr_storage addr;
	int lbacklog;
};

typedef struct {
	char* name; // name within link dir
	dev_t device; // device number
	mode_t mode; // open mode
	int flags; // open flags
	off_t off; // saved offset
} hlink_fd_entry;

typedef struct {
	unsigned refidx;
} mapped_fd_entry;

typedef struct {
	char* path; // VFS path to dir
} hlink_dirfd_entry;

typedef struct {
	int dirfd; // dirfd for openat
	char* path; // path for openat
	mode_t mode; // mode for openat
	int flags; // flags for openat
} unpres_fd_entry;

struct sockopt {
	int level;
	int option;
	void* value;
	socklen_t len;
	bool post_assign;
	struct sockopt* next;
};

typedef struct {
	int domain; // socket domain; -1 if socket was connected to its address (vs bound)
	int type; // socket type
	int protocol; // socket protocol
	struct sockaddr_wrap* addr; // symbolic address (NULL until bound/connected)
	struct sockopt* opt;
} sock_fd_entry;

struct pollop {
	int fd;
	struct epoll_event event;
	struct pollop* next;
};

typedef struct {
	struct pollop* oplist;
	int flag;
} epoll_fd_entry;

#define SLURP_BUF_SIZE 0x10000
typedef struct {
	struct iovec slurp; 
		
	char* path;
	int buddy;

	int mode;
	int dirfd;
	int flags;
} pipe_fd_entry;

typedef struct fd_entry {
	_Atomic fd_type_t type;
	union {
		hlink_fd_entry h;
		mapped_fd_entry m;
		hlink_dirfd_entry hd;
		unpres_fd_entry u;
		sock_fd_entry s;
		epoll_fd_entry e;
		pipe_fd_entry p;
		void* v;
	} E;
} fd_entry_t;

typedef struct mapref {
	atomic_uint refcnt;
	int ofd;
	fd_type_t ftype;
	union { 
		hlink_fd_entry h;
		unpres_fd_entry u;
	} file;
} mapref_t;

typedef struct dev2hlink {
	dev_t device;
	int hlink_dirfd;
} dev2hlink_t;
 
void pfile_fresh_start(void);
void pfile_restore(void);
void cleanup_fd_tracking(void);
void save_fd_tracking(void);
char* get_fd_path(int fd);
unsigned set_fd_mapped(int);
void add_mref(unsigned);
void subtract_mref(unsigned);
int get_temp_mref(unsigned);
void free_temp_mref(int);

#define _v_dup2(a, b) syscall(SYS_dup2, a, b)
#define _v_pipe2(p, f) syscall(SYS_pipe2, p, f)
