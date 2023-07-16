all: server.cpp client.cpp
	g++ server.cpp -o server -lpthread
	g++ client.cpp -o client -lpthread

run_server: server
	./server

run_client: client
	./client