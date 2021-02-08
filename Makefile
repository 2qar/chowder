CC=cc
CFLAGS=-Wall -Wextra -Werror -pedantic
LIBSSL=`pkg-config --libs openssl`
LIBS=$(LIBSSL) -lm -lz
TARGET=chowder

$(TARGET): main.o protocol.o server.o conn.o packet.o nbt.o region.o section.o blocks.o world.o include/linked_list.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

debug: CFLAGS += -g
debug: $(TARGET)

main.o: protocol.o server.o conn.o world.o

protocol.o: nbt.o packet.o conn.o region.o

server.o: protocol.o conn.o

conn.o: packet.o

packet.o: nbt.o

region.o: section.o nbt.o

world.o: region.o

clean:
	rm -f *.o include/*.o $(TARGET)
