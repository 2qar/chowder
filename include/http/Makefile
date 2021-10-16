CC=cc
CFLAGS=-g -Wall -Wextra -Werror -pedantic -I../
LDFLAGS=`pkg-config --libs openssl`

SOURCES=http.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=http.o

TEST_SOURCES=http.c tests.c ../linked_list.c ../hashmap.c
TEST_OBJECTS=$(TEST_SOURCES:.c=.o)
TEST_TARGET=tests

$(TARGET): $(OBJECTS)

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY=clean clean-tests

clean:
	@rm $(OBJECTS)

clean-tests:
	@rm $(TEST_OBJECTS) $(TEST_TARGET)
