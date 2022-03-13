include config.mk

CC=gcc
CPPFLAGS=-I. -I../include -I$(build_dir) $(addprefix -I$(chowder_dir)/,libs/nbt/include libs/list/include libs/mc/include src)
CFLAGS=-g -Wall -Werror -Wextra -pedantic
LDFLAGS=`pkg-config --libs openssl`

chowder_src_dirs=libs/nbt libs/list src libs/mc
vpath %.c $(addprefix $(chowder_dir)/,$(chowder_src_dirs))

build_dir=build
bin_dir=$(build_dir)/bin
pc=../pc
objects:=common.o conn.o packet.o nbt.o player.o list.o mc.o message.o
objects:=$(addprefix $(build_dir)/,$(objects))
test_names=$(basename $(filter-out common.c,$(wildcard *.c)))
tests=$(addprefix $(bin_dir)/,$(test_names))

.PHONY=clean tests
tests: $(tests)

$(tests): $(bin_dir)/%: $(objects) %.c | $(bin_dir) $(pc)
	$(pc) -o $(build_dir) ../examples/$*.packet
	$(CC) -DPACKET_FILE_PATH="\"/tmp/$*.bin\"" $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(build_dir)/$*.c $(LDFLAGS)

$(build_dir)/%.o: %.c | $(build_dir)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $^

$(build_dir): ; @mkdir -p $(build_dir)
$(bin_dir): ; @mkdir -p $(bin_dir)

$(pc): ; @cd WORKING_DIR=`pwd` cd ../ && make && cd $$WORKING_DIR

clean:
	rm -rf $(build_dir)
