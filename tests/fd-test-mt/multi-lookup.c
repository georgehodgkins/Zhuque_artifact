#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <sched.h>
#include <string.h>
#include <semaphore.h>

#include "shard.h"
#include "req_res.h"

int main (int argc, char** argv) {
	// start the clock
	struct timeval tI;
	gettimeofday(&tI, NULL);
	
	// params
	int rqnum, rsnum; // requester/resolver count
	FILE *serviced, *results; // handles for those files
	char** namefiles;
	
	// parse args
	int err = 0;
	if (argc < 6) {
		fputs("Not enough arguments.\n", stderr);
		err = 1;
	} else {
		rqnum = atoi(argv[1]);
		if (rqnum == 0) {
			fputs("Number of requester threads is invalid.\n", stderr);
			err = 1;
		}
		rsnum = atoi(argv[2]);
		if (rsnum == 0) {
			fputs("Number of resolver threads is invalid.\n", stderr);
			err = 1;
		}
		serviced = fopen(argv[3], "w");
		if (!serviced) {
			perror("Could not open requester log");
			err = 1;
		}
		results = fopen(argv[4], "w");
		if (!results) {
			perror("Could not open resolver output file");
			err = 1;
		}
	}

	if (err) {
		fputs("Expected arguments: <# requesters> <# resolvers> <requester log> "
				"<resolver log> <data file> [<more data files>]\n", stderr);
		return 1;
	}

	const int nf_count = argc - 5; // number of input files to process
	namefiles = &argv[5]; // why waste this perfectly good memory?
	int nf_ind = 0; // index (in namefiles) for next input filename
	if (rqnum > nf_count) {
		fputs("Warning: truncating number of requester threads to number of input files.\n", stderr);
		rqnum = nf_count;
	}

	// initialize shared arrays
	shard requests_sh, resolved_sh;
	shard* requests = &requests_sh;
	shard *resolved = &resolved_sh;
	shard_init(requests);
	shard_init(resolved);
	
	// initialize locks for serviced and results files
	pthread_mutex_t serviced_lk = PTHREAD_MUTEX_INITIALIZER;

	// spawn requester threads
	requester_args *rqargs = (requester_args*) malloc(sizeof(requester_args)*rqnum);
	pthread_t *requesters = (pthread_t*) malloc(sizeof(pthread_t)*rqnum);
	for (int a = 0; a < rqnum; ++a) {
		rqargs[a].requests = requests;
	       	rqargs[a].serviced = serviced;
		rqargs[a].serviced_lk = &serviced_lk;
		sem_init(&rqargs[a].req_sem, 0, 0);
		sem_init(&rqargs[a].resp_sem, 0, 0);
		// open an input file
		do {
			rqargs[a].namefile = fopen(namefiles[nf_ind], "r");
			if (rqargs[a].namefile == NULL) {
				fprintf(stderr, "Could not open input file %s: %s\n",
						namefiles[nf_ind], strerror(errno));
			}
			++nf_ind;
		} while (rqargs[a].namefile == NULL && nf_ind < nf_count);
		if (nf_ind == nf_count &&
				(!rqargs[a].namefile || a < rqnum-1)) { // reached end of file list before spawning all requesters, truncate rqnum
			fputs("Warning: Fewer valid input files than requester threads, truncating requester thread count.\n", stderr);
			rqnum = a;
			// add one if current thread is valid
			if (rqargs[a].namefile) ++rqnum;
			else break; // do not try to spawn requester with empty file
		}
		// fork the thread
		err = pthread_create(&requesters[a], NULL, requester_thread, (void*) &rqargs[a]);
		assert(err == 0 && "Error spawning requester thread!");
	}

	// spawn resolver threads
	resolver_args *rsargs = malloc(sizeof(resolver_args)*rsnum);
	pthread_t *resolvers = malloc(sizeof(pthread_t)*rsnum);
	for (int a = 0; a < rsnum; ++a) {
		rsargs[a].requests = requests;
		rsargs[a].resolved = resolved;
		err = pthread_create(&resolvers[a], NULL, resolver_thread, (void*) &rsargs[a]);
		assert(err == 0 && "Error spawning resolver thread!");
	}

	// Main control loop:
	// peridically poll requester threads to see if they need a new file
	// until there are no more new files
	// and write items in resolved buffer out to results file
	// until there are no more items in resolved buffer
	int lines_written = 0; // debug counter of total output lines
	while (1) { // loop terminates when resolved buffer is empty and terminated
#ifndef NDEBUG
		static int iter_count = 0;
#endif
		static int subsequent_action = 0;
		if (nf_ind < nf_count) {
			for (int a = 0; a < rqnum; ++a) {
				if (sem_trywait(&rqargs[a].req_sem) == 0) { // thread is waiting for new file
					do {
						rqargs[a].namefile = fopen(namefiles[nf_ind], "r");
						if (rqargs[a].namefile == NULL) {
							fprintf(stderr, "Could not open input file %s: %s",
									namefiles[nf_ind], strerror(errno));
						}
						++nf_ind;
#ifndef NDEBUG
						fprintf(stderr, "Master: Gave Rq 0x%lx file %s, %d files remaining\n",
							(size_t) requesters[a], namefiles[nf_ind - 1], nf_count - nf_ind);
#endif
					} while (rqargs[a].namefile == NULL && nf_ind != nf_count);
					sem_post(&rqargs[a].resp_sem);
				}
				if (nf_ind == nf_count) { // no more files
					subsequent_action = 1;
					break;
				}
			}
		} else if (subsequent_action == 1) { 
			// make sure no more puts are pending on the request buffer and terminate it
			static int rqs_done = 0;
			for (int a = 0; a < rqnum; ++a) {
				if (rqargs[a].requests) {
					rqargs[a].namefile = NULL;
					sem_post(&rqargs[a].resp_sem); // in case they're currently blocking
					if (sem_trywait(&rqargs[a].req_sem) == 0) {
#ifndef NDEBUG
						fprintf(stderr, "Master: Rq %lx reports completion \n", (size_t) requesters[a]);
#endif
						++rqs_done;
						rqargs[a].requests = NULL; // indicate completion
					}
				}
			}
			// we do not join requesters here to give them time to write out results
			if (rqs_done == rqnum) {
				shard_terminate(requests);
				subsequent_action = 2;
#ifndef NDEBUG
				fputs("Master: All requesters report completion, request buffer terminated", stderr);
#endif
			}
		}

		// Print any strings in resolved buffer to output file (returns null if none available)
		char* str; 
		while ( (str = shard_tryget(resolved)) ) {
			fputs(str, results);
#ifndef NDEBUG
			fprintf(stderr, "Master: Put entry %s in output file\n", str);
#endif
			free(str);
			++lines_written;
		} 
		
		// Exit control loop if shard is empty and all resolvers are done putting
		if (shard_getterm(resolved) == rsnum) {
			// terminate gets incremented by exiting resolvers
			break;
		}

#ifndef NDEBUG
		++iter_count;
		if (iter_count%1000 == 0) {
			fputs("Master: 1000 control loop iterations\n", stderr);
		}
#endif
		// sleep for 5 us
		const struct timespec sleepy = {0, 5000};
		nanosleep(&sleepy, NULL);
	}

#ifndef NDEBUG
	fputs("Master: left control loop\n", stderr);
#endif

	//  wake and join all requester threads
	int requester_served = 0;
	for (int a = 0; a < rqnum; ++a) {
#ifndef NDEBUG
		fprintf(stderr, "Master: Joining requester %lx...", (size_t) requesters[a]);
#endif
		void* serv; // holds return value (number of requests made)
		pthread_join(requesters[a], &serv);
		requester_served += *(int*) serv;
		// clean up dynallocs
		free(serv);
		sem_destroy(&rqargs[a].req_sem);
		sem_destroy(&rqargs[a].resp_sem);
#ifndef NDEBUG
		fputs("joined.\n", stderr);
#endif
	}

	// join all resolver threads
	int resolver_served = 0;
	for (int a = 0; a < rsnum; ++a) {
#ifndef NDEBUG
		fprintf(stderr, "Master: Joining resolver %lx...", (size_t) resolvers[a]);
#endif
		void* serv;
		pthread_join(resolvers[a], &serv);
		resolver_served += *(int*) serv;
		free(serv);
#ifndef NDEBUG
		fputs("joined.\n", stderr);
#endif
	}

	assert(resolver_served == requester_served && "Requester/resolver mismatch!");
	assert(resolver_served == lines_written && "Resolver/global out mismatch!");
	for (int a = 0; a < ARRAY_SIZE; ++a) {
		assert(resolved->array[a] == NULL && "Entries remaining in resolved buffer!");
		assert(requests->array[a] == NULL && "Entries remaining in request buffer!");
	}

	// we're done now
	fclose(serviced);
	fclose(results);
	// no memory leeks pls
	free(rqargs);
	free(requesters);
	free(rsargs);
	free(resolvers);

	// stop and inspect clock
	struct timeval tF, dT;
	gettimeofday(&tF, NULL);
	assert(tF.tv_sec >= tI.tv_sec && "Time is not monotonic!");
	dT.tv_sec = tF.tv_sec - tI.tv_sec;
	if (tF.tv_usec < tI.tv_usec) {
		assert(dT.tv_sec > 0 && "Time is not monotonic!"); 
		--dT.tv_sec; // carry
		dT.tv_usec = tI.tv_usec - tF.tv_usec;
	} else {
		dT.tv_usec = tF.tv_usec - tI.tv_usec;
	}
	double elapsed = (double) dT.tv_sec + ((double) dT.tv_usec)/1000000;
	printf("%s: total time is %.6f seconds\n", argv[0], elapsed);
	
	return 0;
}	

