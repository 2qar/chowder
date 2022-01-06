CC=cc
include_dirs=src/ $(wildcard libs/*/include) $(protocol_include_dir) \
	     $(packet_auto_gen_include)
CPPFLAGS=$(addprefix -I,$(include_dirs))
CFLAGS=-Wall -Wextra -Werror -pedantic
LDFLAGS=`pkg-config --libs openssl libcurl` -lm -lz
TARGET=$(bin_dir)/chowder

lib_dir=libs
obj_dir=build
bin_dir=$(obj_dir)/bin
packets_dir=protocol
protocol_include_dir=$(obj_dir)/include
packet_auto_gen_dir=utils/packet-auto-gen
packet_auto_gen_include=$(packet_auto_gen_dir)/include
build_scripts_dir=scripts

sources=$(wildcard src/*.c)
lib_sources=$(wildcard $(lib_dir)/*/*.c)
vpath %.c src/ $(wildcard $(lib_dir)/*)
vpath %.h $(include_dirs)

objects:=$(sources:src/%.c=$(obj_dir)/%.o) $(patsubst %.c,$(obj_dir)/%.o,$(notdir $(lib_sources)))
objects:=$(filter-out $(obj_dir)/test.o $(obj_dir)/tests.o,$(objects))
protocol_sources=$(wildcard $(packets_dir)/*.packet)
protocol_headers=$(protocol_sources:$(packets_dir)/%.packet=$(protocol_include_dir)/%.h)
protocol_objects=$(protocol_sources:$(packets_dir)/%.packet=$(obj_dir)/%.o)

$(TARGET): $(protocol_objects) $(objects) | $(bin_dir)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

debug: CFLAGS += -g
debug: $(TARGET)

$(objects): | $(protocol_objects)
$(objects): $(obj_dir)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $(obj_dir)/$*.o

$(protocol_objects): $(protocol_headers) $(protocol_include_dir)/protocol_autogen.h

$(protocol_include_dir)/%.h: $(packets_dir)/%.packet $(packet_auto_gen_dir)/pc \
	| $(protocol_include_dir)
	$(packet_auto_gen_dir)/pc -o $(protocol_include_dir)/ $(packets_dir)/$*.packet
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(@:.h=.c) -o $(obj_dir)/$*.o
	rm $(@:.h=.c)

$(protocol_include_dir)/protocol_autogen.h: $(protocol_headers)
	./$(build_scripts_dir)/make_protocol_header.sh "$(protocol_include_dir)"

$(packet_auto_gen_dir)/pc:
	BUILD_DIR=`pwd` cd $(packet_auto_gen_dir) && make && cd $$BUILD_DIR

$(obj_dir)/%.d: %.c | $(obj_dir) $(protocol_objects)
	@set -e; rm -f $@; \
	 $(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's/$*.o/$(obj_dir)\/$*.o $(obj_dir)\/$*.d/g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$
include $(patsubst %.c,$(obj_dir)/%.d,$(notdir $(sources) $(lib_sources)))

$(obj_dir): ; @mkdir -p $@
$(bin_dir): ; @mkdir -p $@
$(protocol_include_dir): ; @mkdir -p $@

.PHONY=clean
clean:
	rm -rf $(obj_dir)
