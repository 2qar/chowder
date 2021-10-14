#!/bin/bash

err_n_die() {
	echo "./test.sh: $1" 1>&2 && exit 1
}

get_makefile_var() {
	grep -m1 "$1" ../Makefile | sed -r "s/$1\s*=\s*//g"
}

test_filename="$1"
if [ -z $test_filename ]; then
	err_n_die "no filename given"
elif ! [ -r $test_filename ]; then
	err_n_die "$test_filename doesn't exist / isn't readable"
fi

cd ../
make 1>/dev/null || exit $?
cd test/

CC=$(get_makefile_var "CC")
CFLAGS=$(get_makefile_var "CFLAGS")
OBJECTS=($(get_makefile_var "OBJECTS"))
TARGET=$(echo $test_filename | sed 's/\.c//g')

objects=""
for i in $OBJECTS; do
	objects="$objects ../$i"
done

$CC -I../ $CFLAGS -o /tmp/$TARGET $test_filename $objects || exit $?
/tmp/$TARGET
