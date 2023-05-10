CC = gcc
#CFLAGS = -O0 -g
AR = ar
RANLIB = ranlib

SOURCE_OBJ = malloc.o free.o donate.o aligned_alloc.o realloc.o __lock.o

all: libngpmalloc.so

ngpmalloc.a: $(SOURCE_OBJ)
	rm -f $@
	$(AR) rc $@ $^
	$(RANLIB) $@

libngpmalloc.so: $(SOURCE_OBJ)
	$(CC) -fPIC -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -DNGP_WRAP -c -o $@ $^ 

clean:
	rm -f *.o *.a *.so
