all: server.cpp client.cpp
	g++ server.cpp -o server
	g++ client.cpp -o client

server: server
	./server

client: client
	./client