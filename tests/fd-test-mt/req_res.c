#include "req_res.h"

void* requester_thread(void* arg_v) {
	// counter for number of requests made (used for error checking)
	int *service_count = calloc(1, sizeof(int));
	
	// get args
	requester_args *args = (requester_args*) arg_v;
	shard *s = args->requests;
	FILE* namefile = args->namefile;
	int file_count = 0;

	while (namefile) {
		// do the requesto
		++file_count;
		char* hostname_pt;
		hostname_pt = NULL; // getline handles allocation
		size_t hostname_len; // size of allocation (not necessarily actual strlen)
		hostname_len = 0;

		while (getline(&hostname_pt, &hostname_len, namefile) >= 0) {
			hostname_pt[strlen(hostname_pt)-1] = '\0'; // remove newline
			shard_put(s, hostname_pt);
#ifndef NDEBUG
			fprintf(stderr, "Rq 0x%lx: Requested %s\n", (size_t) pthread_self(), hostname_pt);
#endif
			hostname_pt = NULL;
			hostname_len = 0;
			++(*service_count);
		}
		
		// this was an annoying leak that took a long time to find
		// memory alloc'd by getline() needs to be freed even if the call failed
		free(hostname_pt);	

		// call back to the master for another file
		fclose(namefile);
#ifndef NDEBUG
		fprintf(stderr, "Rq 0x%lx: Requesting new file from master\n", (size_t) pthread_self());
#endif
		// notify master and wait for response
		sem_post(&args->req_sem);
		sem_wait(&args->resp_sem);

		namefile = args->namefile;
	}
	// notify master you got the termination
	sem_post(&args->req_sem);

	// log number of files serviced
	pthread_mutex_lock(args->serviced_lk);
	fprintf(args->serviced, "Thread 0x%lx serviced %d files\n", 
			(size_t) pthread_self(), file_count);

	pthread_mutex_unlock(args->serviced_lk);
#ifndef NDEBUG
	fprintf(stderr, "Rq 0x%lx: Terminating\n", (size_t) pthread_self());
#endif
	return service_count;
}

void* resolver_thread(void* arg_v) {

	// get args
	resolver_args *args = (resolver_args*) arg_v;
	shard *requests = args->requests;
	shard *resolved = args->resolved;

	// do the resolvo
	int *service_count = calloc(1, sizeof(int));
	char ipbuf[MAX_IP_LENGTH];
	char *name = NULL;
	while (1) { // terminates when shard_get returns null, meaning no more names to resolve
		name = shard_get(requests);
		if (!name) break;
		int e = dnslookup(name, ipbuf, MAX_IP_LENGTH);
		if (e) strcpy(ipbuf, "");
#ifndef NDEBUG
		fprintf(stderr, "Rs 0x%lx: Resolved %s to %s\n", (size_t) pthread_self(), name, ipbuf);
#endif
		size_t namelen = strlen(name);
		size_t iplen = strlen(ipbuf);
		name = realloc(name, namelen + iplen + 3); //+ comma, newline, null
		stpcpy(stpcpy(stpcpy(&name[namelen], ","), ipbuf), "\n"); // perfectly readable!
		assert(name[namelen + iplen + 2] == '\0' && "You can't count!");
		shard_put(resolved, name);
		++(*service_count);
	}
	
	// tell master thread is done
	shard_terminate(resolved);
	
#ifndef NDEBUG
	fprintf(stderr, "Rs 0x%lx: Terminating\n", (size_t) pthread_self());
#endif
	return service_count;
}



