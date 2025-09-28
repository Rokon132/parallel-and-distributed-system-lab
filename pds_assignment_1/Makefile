CXX = g++
CXXFLAGS = -O2 -std=c++17 -pthread
LDFLAGS = 

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

client: client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f server client