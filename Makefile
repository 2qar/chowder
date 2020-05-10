make:
	cc main.c packet.c protocol.c -Wall -lssl -lcrypto -o chowder
