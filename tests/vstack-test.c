#include <psys.h>
#include <stdio.h>
#include <string.h>

void f1 (const char* str) {
	char* mem = _p_vpush(9);
	memcpy(mem, str, 8);
	mem[8] = 0;
	printf("Stort sting: %s\n", mem);
	_p_vpop();
}

void f0 (const char* arg, size_t arglen) {
	char* mem = _p_vpush(arglen*3);
	memcpy(mem, arg, arglen);
	for (unsigned i = 0; i < arglen; ++i) {
		mem[i] = toupper(mem[i]);
	}
	f1(mem);
	memcpy(mem + arglen, arg, arglen);
	for (unsigned i = 0; i < arglen; ++i) {
		mem[arglen + i] = tolower(mem[arglen +i]);
	}
	f1(mem + arglen);
	mem[arglen*2] = 0;
	printf("Bigsmallstring: %s", mem);
	_p_vpop();
}

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("Gib exactly one string pls\n");
		return -1;
	}

	size_t memlen = strlen(argv[1]);
	void* vmem = _p_vpush(memlen+1);
	memcpy(vmem, argv[1], memlen+1);
	f0((char*) vmem, memlen);
	
	printf("UR STRING WAS: %s\n", (char*) vmem);
	_p_vpop();
	return 0;
}
