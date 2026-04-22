CXX := clang++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -Wextra -Wpedantic
INCLUDES := -I../include

SRC := \
	../src/board.cpp \
  	../src/fen.cpp \
  	../src/attacks.cpp \
  	../src/make.cpp \
  	../src/move_gen.cpp \
  	perft.cpp

BIN := perft

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC)

clean:
	rm -f $(BIN)

.PHONY: all clean
