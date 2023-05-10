#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h> 
#include <signal.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <limits.h>

#define IN_FILE "test_in.txt"
#define OUT_FILE "test_out.txt"
#define LCBRK 16

/*void handler (int signum) {
	assert(signum == SIGPWR);
	puts("In user handler!");
}*/

int main()
{
	printf("Signals:\n");
	sigset_t sigs;
	pthread_sigmask(0, NULL, &sigs);
	for (int i = 1; i <= SIGRTMAX; ++i) {
		if (sigismember(&sigs, i)) {
			printf("%s: BLOCKED\n", strsignal(i));
		} else {
			struct sigaction old;
			sigaction(i, NULL, &old);
			if (old.sa_handler == SIG_IGN) {
				printf("%s: IGNORED\n", strsignal(i));
			} else if (old.sa_handler != SIG_DFL) {
				if (old.sa_flags & SA_SIGINFO) {
					printf("%s: HANDLED by %p (siginfo)\n", strsignal(i), 
							old.sa_sigaction);
				} else {
					printf("%s: HANDLED by %p\n", strsignal(i), old.sa_handler);
				}
			}
		}
	}
	printf("\n\n");

	printf("A new execution. Opening files...");
	fflush(stdout);
	int infd = open(IN_FILE, O_RDONLY);
	if (infd < 0) {
		perror("\nOpen input file");
		abort();
	}
	int outfd = open(OUT_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0600);
	if (outfd < 0) {
		perror("\nOpen output file");
		abort();
	}
	printf("done.\n\n");
	fflush(stdout);

	char inbuf[64] = {0};
	char outbuf[64] = {0};
	unsigned lc = 0;
	while (inbuf[0] != '\n') {
		++lc;
		/*if (++lc == LCBRK) {
			//kill(0, SIGPWR);
			// delay loop
			//for (volatile size_t x = 0; x < UINT_MAX; ++x);
		}*/
		char* l = inbuf;
		size_t sz = 0;
 		do {
			read(infd, l, 1);
			++sz;
		} while (*(l++) != '\n');
		*l = '\0';
		if (l == &inbuf[1]) continue;
		printf("Got line %u (%zu c); ", lc, sz);
		fflush(stdout);
		double f;
		int s = sscanf(inbuf, "%lf\n", &f);
		if (s == EOF) {
			perror("\nScan input line");
			abort();
		}
		printf("Read input value %0.4f; ", f);
		fflush(stdout);
		f = sin(f);
		s = sprintf(outbuf, "%0.4f\n", f);
		if (s < 0) {
			perror("\nMake output line");
			abort();
		}
		char* o = outbuf;
		while (*o) {
			write(outfd, o++, 1);
		}
		printf("Wrote output value %0.4f\n", f);
		fflush(stdout);
	}

	int s = close(infd);
	assert(s == 0);
	s = close(outfd);
	assert(s == 0);
	return 0;
}

