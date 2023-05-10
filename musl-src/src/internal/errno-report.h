#pragma once
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/*
 * Macros for checking libc/kernel errors. Only useful for functions
 * with integral return values that set errno.
 *
 * First suffix is success condition:
 * - Z: return == zero
 * - GZ: return > 0
 * - GEZ: return >= 0 (file descriptors are the most common use case)
 *
 * If this condition is not met, prints errno and file/line # to stderr and
 * then calls abort().
 *
 * additional _NR means the return value is discarded after error checking.
 * If this suffix is not present, the variable to write the return value is the first
 * arg, and the function is the second arg. If the suffix is present, return value
 * arg is absent and function is the first arg.
 *
 * Add macros with more predicates as needed.
 */

// calls function func, and reports error and exit if return is not zero
#define ERRNO_REPORT_Z_NR(func, ...) \
	do { \
		int s = func(__VA_ARGS__); \
		if (s) { \
			fprintf(stderr, "%s:%d : %s [%s]", __FILE__, __LINE__, strerror(errno), #func); \
			abort(); \
		} \
	} while (0) // force semicolon; also makes scope for s easier

// calls function func and stores return value in ret; report error and exit if return is zero
#define ERRNO_REPORT_NZ(ret, func, ...) \
	do { \
		ret = func(__VA_ARGS__); \
		if (!ret) { \
			fprintf(stderr, "%s:%d : %s [%s]", __FILE__, __LINE__, strerror(errno), #func); \
			abort(); \
		} \
	} while (0)

// same as above, except the error condition is instead return value < 0
#define ERRNO_REPORT_GEZ(ret, func, ...) \
	do { \
		ret = func(__VA_ARGS__); \
		if (ret < 0) { \
			fprintf(stderr, "%s:%d : %s [%s]", __FILE__, __LINE__, strerror(errno), #func); \
			abort(); \
		} \
	} while (0)

#define ERRNO_REPORT_GEZ_NR(func, ...) \
	do { \
		int ret = func(__VA_ARGS__); \
		if (ret < 0) { \
			fprintf(stderr, "%s:%d : %s [%s]", __FILE__, __LINE__, strerror(errno), #func); \
			abort(); \
		} \
	} while (0)

#define ERRNO_REPORT_GZ_NR(func, ...) \
	do { \
		int ret = func(__VA_ARGS__); \
		if (ret <= 0) { \
			fprintf(stderr, "%s:%d : %s [%s]", __FILE__, __LINE__, strerror(errno), #func); \
			abort(); \
		} \
	} while (0)
