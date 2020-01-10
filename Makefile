CXX 		:= g++
CXXFLAGS	:= -Wall -O2 -std=c++11

SRC	:= $(wildcard *.cc)
OBJ	:= $(patsubst %.cc, %.o, $(SRC))
BIN	:= a.out
LIB :=

RM	:= rm -rf


all: $(BIN)

run: all
	./$(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB)
	
clean:
	$(RM) $(BIN) $(OBJ)