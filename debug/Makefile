CXX := g++
CXXFLAGS ?= -std=c++20 -O3 -g -Wall -Wextra -Wpedantic -mbmi2 -march=native
INCLUDES := -I../include

DEFAULT_NNUE := ../SHAYVERI2_5_0.nnue
EMBED_NNUE := ../build/embed_nnue
EMBEDDED_NNUE_SRC := ../build/embedded_nnue.cpp

COMMON_SRC := \
	../src/board.cpp \
	../src/fen.cpp \
	../src/attacks.cpp \
	../src/make.cpp \
	../src/move_gen.cpp \
	../src/evaluate.cpp \
	../src/search.cpp \
	../src/nnue.cpp \
	$(EMBEDDED_NNUE_SRC) \
	../src/nnue_avx2.cpp \
	../src/nnue_update.cpp \
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
NNUE_TEST_SRC := \
	../src/attacks.cpp \
	../src/board.cpp \
	../src/fen.cpp \
	../src/make.cpp \
	../src/move_gen.cpp \
	../src/nnue.cpp \
	$(EMBEDDED_NNUE_SRC) \
	../src/nnue_avx2.cpp \
	../src/nnue_update.cpp \
	../src/zobrist.cpp \
	nnue_test.cpp

BIN := perft
LEGALITY_BIN := legality
FEN_STATE_BIN := fen_state
MAKE_UNMAKE_BIN := make_unmake
SEE_TESTS_BIN := see_tests
EVAL_SYM_BIN := eval_symmetry
TT_SAFETY_BIN := tt_safety
NNUE_TEST_BIN := nnue_test

all: $(BIN) $(FEN_STATE_BIN) $(MAKE_UNMAKE_BIN) $(LEGALITY_BIN) $(SEE_TESTS_BIN) $(EVAL_SYM_BIN) $(TT_SAFETY_BIN) $(NNUE_TEST_BIN)

$(EMBED_NNUE): ../src/tools/embed_nnue.cpp
	mkdir -p ../build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Wpedantic -o $@ ../src/tools/embed_nnue.cpp

$(EMBEDDED_NNUE_SRC): $(DEFAULT_NNUE) $(EMBED_NNUE)
	$(EMBED_NNUE) $(DEFAULT_NNUE) $@ SHAYVERI2_5_0.nnue

test: perft_suite \
	fen_state_suite \
	make_unmake_suite \
	legality_suite \
	see_suite \
	eval_symmetry_suite \
	tt_safety_suite \
	nnue_suite \
	uci_check \
	tactical_regression

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

$(NNUE_TEST_BIN): $(NNUE_TEST_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(NNUE_TEST_SRC)

nnue_classic_test.nnue nnue_kb8_test.nnue:
	$(PYTHON) -c "import struct; h=256; magic=0x4E4E5545; \
open('nnue_classic_test.nnue','wb').write(struct.pack('<II', magic, 2) + bytes(768*h*2) + bytes(h*2) + bytes(h*2*2) + bytes(4)); \
open('nnue_kb8_test.nnue','wb').write(struct.pack('<III', magic, 3, 8) + bytes(768*8*h*2) + bytes(h*2) + bytes(h*2*2) + bytes(4))"

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

nnue_suite: $(NNUE_TEST_BIN) nnue_classic_test.nnue nnue_kb8_test.nnue
	./$(NNUE_TEST_BIN) nnue_classic_test.nnue
	./$(NNUE_TEST_BIN) nnue_kb8_test.nnue

ensure_engine:
	@if [ ! -x ../SHAYVERI ]; then $(MAKE) -C ..; fi

uci_check: ensure_engine
	$(PYTHON) uci_check.py

tactical_regression: ensure_engine
	$(PYTHON) tactical_regression.py

clean:
	rm -f $(BIN) $(LEGALITY_BIN) $(FEN_STATE_BIN) $(MAKE_UNMAKE_BIN) $(SEE_TESTS_BIN) $(EVAL_SYM_BIN) $(TT_SAFETY_BIN) $(NNUE_TEST_BIN) nnue_classic_test.nnue nnue_kb8_test.nnue
	find . -type d -name __pycache__ -prune -exec rm -rf {} +

.PHONY: all test perft_suite fen_state_suite make_unmake_suite legality_suite see_suite eval_symmetry_suite tt_safety_suite nnue_suite ensure_engine uci_check tactical_regression clean
