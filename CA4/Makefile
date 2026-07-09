CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -O2

all: sender receiver

sender: sender.cpp packet.hpp
	$(CXX) $(CXXFLAGS) sender.cpp -o sender

receiver: receiver.cpp packet.hpp
	$(CXX) $(CXXFLAGS) receiver.cpp -o receiver

clean:
	rm -f sender receiver