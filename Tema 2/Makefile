all: build-server build-client

build-server:
	gcc -Wall -g server.c -o server

build-client:
	gcc -Wall -g client.c -o client

clean:
	rm -rf *.o client server