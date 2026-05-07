CXX := clang++
CXXFLAGS ?= -std=c++20 -O3 -g -Wall -Wextra -Wpedantic -mbmi2 -march=native
INCLUDES := -I../include

COMMON_SRC := \
	../src/board.cpp \
	../src/fen.cpp \
	../src/attacks.cpp \
	../src/make.cpp \
	../src/move_gen.cpp \
	../src/evaluate.cpp \
	../src/search.cpp \
	../src/zobrist.cpp \
	../src/tt.cpp \
	../src/see.cpp

PERFT_SRC := $(COMMON_SRC) perft.cpp
LEGALITY_SRC := $(COMMON_SRC) legality.cpp

BIN := perft
LEGALITY_BIN := legality

all: $(BIN)

test: perft_suite legality_suite uci_smoke

$(BIN): $(PERFT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(PERFT_SRC)

$(LEGALITY_BIN): $(LEGALITY_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(LEGALITY_SRC)

perft_suite: $(BIN)
	./$(BIN) suite

legality_suite: $(LEGALITY_BIN)
	./$(LEGALITY_BIN)

uci_smoke:
	@if [ ! -x ../SHAYVERI ]; then $(MAKE) -C ..; fi
	python3 uci_smoke.py

clean:
	rm -f $(BIN) $(LEGALITY_BIN)

.PHONY: all test perft_suite legality_suite uci_smoke clean
