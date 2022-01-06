#!/bin/bash

err() {
	echo "$0: $1" >&2 && exit 1
}

include_dir="$1"
if [ -z "$include_dir" ]; then
	err "include_dir not given"
elif [ ! -d $include_dir ]; then
	err "\"$include_dir\" isn't a directory"
fi

header_guard="PROTOCOL_AUTOGEN_H"
includes=""
for i in $(ls -1 $include_dir | grep "h$"); do
	includes="$includes#include \"$i\"\n"
done

echo -e "#ifndef $header_guard\n#define $header_guard\n$includes#endif // $header_guard" \
	> $include_dir/protocol_autogen.h
