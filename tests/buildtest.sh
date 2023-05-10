#!/bin/bash

set -euo pipefail

make -C fd-test-st
make -C fd-test-mt
#gcc -o vstack-test-v -O0 -g vstack-test.c
#gcc -o vstack-test-p -O0 -DDOPT_VSTACK -g vstack-test.c

