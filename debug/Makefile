CXX := clang++
CXXFLAGS ?= -std=c++20 -O3 -g -Wall -Wextra -Wpedantic -mbmi2 -march=native
INCLUDES := -I../include

SRC := \
	../src/board.cpp \
	../src/fen.cpp \
	../src/attacks.cpp \
	../src/make.cpp \
	../src/move_gen.cpp \
	../src/evaluate.cpp \
	../src/search.cpp \
	../src/zobrist.cpp \
	../src/tt.cpp \
	../src/see.cpp \
  	perft.cpp

BIN := perft

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC)

clean:
	rm -f $(BIN)

.PHONY: all clean
