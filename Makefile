make:
	cc main.c packet.c protocol.c server.c conn.c nbt.c -Wall -lssl -lcrypto -o chowder
