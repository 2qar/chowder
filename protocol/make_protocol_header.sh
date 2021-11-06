#!/bin/bash

header_guard="PROTOCOL_AUTOGEN_H"
includes=""
for i in $(ls -1 include/ | grep "h$"); do
	includes="$includes#include \"$i\"\n"
done

echo -e "#ifndef $header_guard\n#define $header_guard\n$includes#endif // $header_guard" \
	> include/protocol_autogen.h
