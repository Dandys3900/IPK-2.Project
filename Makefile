CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
SRCS := main.cpp ServerClass.cpp UDPHelper.cpp TCPHelper.cpp
OBJS := $(SRCS:.cpp=.o)
EXE := ipk24chat-server

$(EXE): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(EXE)

clean:
	rm -f $(EXE)

.PHONY: all clean
