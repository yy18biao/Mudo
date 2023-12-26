all: test
client:client.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17 -pthread
server:server.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17 -pthread
test:TestClient.cc
	g++ -o $@ $^ -std=c++17 -lpthread
	
.PHONY:
clean:
	rm -rf server client test