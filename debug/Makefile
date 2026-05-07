CXX := g++
PYTHON ?= python
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
SEE_TESTS_SRC := $(COMMON_SRC) see_tests.cpp
EVAL_SYM_SRC := $(COMMON_SRC) eval_symmetry.cpp
TT_SAFETY_SRC := ../src/tt.cpp tt_safety.cpp

BIN := perft
LEGALITY_BIN := legality
FEN_STATE_BIN := fen_state
MAKE_UNMAKE_BIN := make_unmake
SEE_TESTS_BIN := see_tests
EVAL_SYM_BIN := eval_symmetry
TT_SAFETY_BIN := tt_safety

all: $(BIN) $(FEN_STATE_BIN) $(MAKE_UNMAKE_BIN) $(LEGALITY_BIN) $(SEE_TESTS_BIN) $(EVAL_SYM_BIN) $(TT_SAFETY_BIN)

test: perft_suite \
	fen_state_suite \
	make_unmake_suite \
	legality_suite \
	see_suite \
	eval_symmetry_suite \
	tt_safety_suite \
	uci_smoke \
	tactical_regression

release_test: test selfplay_smoke

$(BIN): $(PERFT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(PERFT_SRC)

$(LEGALITY_BIN): $(LEGALITY_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(LEGALITY_SRC)

$(FEN_STATE_BIN): $(FEN_STATE_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(FEN_STATE_SRC)

$(MAKE_UNMAKE_BIN): $(MAKE_UNMAKE_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(MAKE_UNMAKE_SRC)

$(SEE_TESTS_BIN): $(SEE_TESTS_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SEE_TESTS_SRC)

$(EVAL_SYM_BIN): $(EVAL_SYM_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(EVAL_SYM_SRC)

$(TT_SAFETY_BIN): $(TT_SAFETY_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(TT_SAFETY_SRC)

perft_suite: $(BIN)
	./$(BIN) suite

fen_state_suite: $(FEN_STATE_BIN)
	./$(FEN_STATE_BIN)

make_unmake_suite: $(MAKE_UNMAKE_BIN)
	./$(MAKE_UNMAKE_BIN)

legality_suite: $(LEGALITY_BIN)
	./$(LEGALITY_BIN)

see_suite: $(SEE_TESTS_BIN)
	./$(SEE_TESTS_BIN)

eval_symmetry_suite: $(EVAL_SYM_BIN)
	./$(EVAL_SYM_BIN)

tt_safety_suite: $(TT_SAFETY_BIN)
	./$(TT_SAFETY_BIN)

ensure_engine:
	@if [ ! -x ../SHAYVERI ]; then $(MAKE) -C ..; fi

uci_smoke: ensure_engine
	$(PYTHON) uci_smoke.py

tactical_regression: ensure_engine
	$(PYTHON) tactical_regression.py

selfplay_smoke: ensure_engine
	$(PYTHON) selfplay_smoke.py

clean:
	rm -f $(BIN) $(LEGALITY_BIN) $(FEN_STATE_BIN) $(MAKE_UNMAKE_BIN) $(SEE_TESTS_BIN) $(EVAL_SYM_BIN) $(TT_SAFETY_BIN) --rec __pycache__

.PHONY: all test release_test perft_suite fen_state_suite make_unmake_suite legality_suite see_suite eval_symmetry_suite tt_safety_suite ensure_engine uci_smoke tactical_regression selfplay_smoke clean
