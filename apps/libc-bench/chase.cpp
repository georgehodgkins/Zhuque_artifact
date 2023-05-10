#include <random>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <cassert>

#define RNG_SEED 42424242u
#define MIN_LEN 20000000ul // about 80 MB

using namespace std;

int main (int argc, char** argv) {
	unsigned long len;
	if (argc != 2 || !(len = strtoul(argv[1], NULL, 0))) {
		cout << "Usage: " << argv[0] << " <test array length>\n";
		return -1;
	}
	if (len < MIN_LEN) {
		cout << "The array length must be >= " << MIN_LEN << " so it doesn't fit in the cache.\n";
		return -1;
	}
	
	int* array = (int*) calloc(len, sizeof(int));
	assert(array);

	minstd_rand rng (RNG_SEED);
	uniform_int_distribution<> dist (0ul, len-1);

	cout << "Warming up cache...";
	for (unsigned i = 0; i < MIN_LEN; ++i) {
		++array[i];
	}
	cout << "done.\n";

	cout << "Running benchmark with array of size " << len << "..." << flush;
	struct timespec t_start, t_end;
	int s = clock_gettime(CLOCK_MONOTONIC, &t_start);
	assert(!s);
	for (unsigned c = 0; c < len; ++c) {
		unsigned long idx = dist(rng);
		++array[idx];
	}
	s = clock_gettime(CLOCK_MONOTONIC, &t_end);
	assert(!s);

	cout << "done.\n\nElapsed: " << (double) (t_end.tv_sec - t_start.tv_sec) +
		((double) (t_end.tv_nsec - t_start.tv_nsec)) / 1e9 << " s\n";
	
	return 0;
}


