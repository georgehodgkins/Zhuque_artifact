#!/bin/bash
set -e
TREE=$1
LIB=$2

if [[ $TREE == 'avltree' ]]; then
	sed -i -e 's:CFLAGS += -DMAP_USE_RBTREE:CFLAGS += -DMAP_USE_AVLTREE_LONG -DUSE_DUP_AND_REL:g' Makefile
fi
if [[ $TREE == 'rbtree' ]]; then
	sed -i -e 's:CFLAGS += -DMAP_USE_AVLTREE_LONG -DUSE_DUP_AND_REL:CFLAGS += -DMAP_USE_RBTREE:g' Makefile
fi
make ../lib/wrap/${LIB}_${TREE}.o
make ../lib/wrap/admin_pop.o
make ../lib/wrap/context.o
make vacation-${LIB}-${TREE}
