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
FEN_STATE_SRC := $(COMMON_SRC) fen_state.cpp
MAKE_UNMAKE_SRC := $(COMMON_SRC) make_unmake.cpp

BIN := perft
LEGALITY_BIN := legality
FEN_STATE_BIN := fen_state
MAKE_UNMAKE_BIN := make_unmake

all: $(BIN)

test: perft_suite fen_state_suite make_unmake_suite legality_suite uci_smoke

$(BIN): $(PERFT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(PERFT_SRC)

$(LEGALITY_BIN): $(LEGALITY_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(LEGALITY_SRC)

$(FEN_STATE_BIN): $(FEN_STATE_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(FEN_STATE_SRC)

$(MAKE_UNMAKE_BIN): $(MAKE_UNMAKE_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(MAKE_UNMAKE_SRC)

perft_suite: $(BIN)
	./$(BIN) suite

fen_state_suite: $(FEN_STATE_BIN)
	./$(FEN_STATE_BIN)

make_unmake_suite: $(MAKE_UNMAKE_BIN)
	./$(MAKE_UNMAKE_BIN)

legality_suite: $(LEGALITY_BIN)
	./$(LEGALITY_BIN)

uci_smoke:
	@if [ ! -x ../SHAYVERI ]; then $(MAKE) -C ..; fi
	python3 uci_smoke.py

clean:
	rm -f $(BIN) $(LEGALITY_BIN) $(FEN_STATE_BIN) $(MAKE_UNMAKE_BIN)

.PHONY: all test perft_suite fen_state_suite make_unmake_suite legality_suite uci_smoke clean
