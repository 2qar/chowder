make:
	cc main.c packet.c protocol.c server.c conn.c -Wall -lssl -lcrypto -o chowder
