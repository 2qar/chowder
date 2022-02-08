CC=gcc
CPPFLAGS=-Iinclude/ -I../hashmap/include
CFLAGS=-g -Wall -Wextra -Werror -pedantic
TARGET=tests

test_srcs=tests.c region.c chunk.c section.c
test_objs=$(test_srcs:.c=.o)

tests: $(test_objs)
