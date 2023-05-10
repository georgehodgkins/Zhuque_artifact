#!/bin/bash

set -x
delay=$1

printf "Restart test for %s:\n" $2
trap '' PWR

mkdir goob
printf "Starting..." "${@:2:$#}"
setarch -R env LD_RELOAD="goob;${delay}" "${@:2:$#}" > /dev/null 2>&1
xit=$?

# if test exits cleanly before signal, keep trying
while [[ $xit -eq 0 ]]; do
	setarch -R env LD_RELOAD="goob;${delay}" "${@:2:$#}" > /dev/null 2>&1
	xit=$?
done

if [[ $xit -ne 85 ]]; then
	printf "\nError: %s exited with code %d, should have been 85\n" "${*:2:$#}" $xit
	exit 1
fi

printf "restarting..."
setarch -R env LD_RELOAD="goob;${delay}" "${@:2:$#}" > /dev/null 2>&1

xit=$?

if [[ $xit -ne 0 ]]; then
	printf "\nError: %s exited with code %d, should have been 0\n" "${*:2:$#}" $xit
	exit 1
fi

if [[ -e goob/* ]]; then
	printf "\nError: Heap directory not empty after clean exit!"
	exit 1
fi
rm -r goob

printf "success!\n"

