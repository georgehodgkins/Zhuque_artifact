# Makefile v1 for CSCI3753-F20 PA3

CC = gcc
CFLAGS = -Wextra -Wall -g -pthread -O0 
#CFLAGS = -Wall -Wextra -g -O3 -pthread -DNDEBUG
INCLUDES = 
LFLAGS = 
LIBS =

MAIN = multi-lookup

# Add any additional .c files to SRCS and .h files to HDRS
SRCS = multi-lookup.c util.c req_res.c shard.c
HDRS = util.h req_res.h shard.h

OBJS = $(SRCS:.c=.o) 

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean
clean: 
	$(RM) *.o *~ $(MAIN)

