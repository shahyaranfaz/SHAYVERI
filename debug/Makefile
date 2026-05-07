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

test: perft_suite uci_smoke

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC)

perft_suite: $(BIN)
	./$(BIN) suite

uci_smoke:
	@if [ ! -x ../SHAYVERI ]; then $(MAKE) -C ..; fi
	python3 uci_smoke.py

clean:
	rm -f $(BIN)

.PHONY: all test perft_suite uci_smoke clean
