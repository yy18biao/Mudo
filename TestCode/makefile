all: client server
client:test_client.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17
server:test_server.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17
	
.PHONY:
clean:
	rm -rf server client