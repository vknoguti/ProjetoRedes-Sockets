all: server.cpp client.cpp
	g++ server.cpp -o server
	g++ client.cpp -o client

run_server: server
	./server

run_client: client
	./client