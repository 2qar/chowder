#!/bin/bash

make || exit 1
ls -1 *.c | grep -v common | while read -r line; do
	test_name=$(echo $line | sed s'/\.c//g')
	# FIXME: assumes binaries get spit out in build/bin/
	packet_file=$(build/bin/$test_name || exit 1)
	echo $?
	rm $packet_file
	echo "ran $test_name"
done
