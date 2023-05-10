#!/bin/sh

objects=$(ls *.o)

for obj in ${objects} ; do
    objcopy --redefine-syms=redefine_syms.txt ${obj} tmp.o
    mv tmp.o ${obj}
    objcopy --redefine-syms=redefine_syms.txt ${obj%.o}.lo tmp.o
    mv tmp.o ${obj%.o}.lo
done
