
SRCS = $(sort $(wildcard *.c))
OBJS = $(SRCS:.c=.o)

CFLAGS = -g -O2 -D_XOPEN_SOURCE=700
LDFLAGS = 
LIBS = -lpthread -lrt -lpthread


all: libc-bench chase

clean:
	rm -f $(OBJS) libc-bench

test: all
	./libc-bench

relink:
	rm -f libc-bench
	make all

retest: relink test

libc-bench: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

chase: chase.cpp
	$(CXX) -O2 -g -o $@ $^
