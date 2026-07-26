CXX := g++
TIDY ?= clang-tidy
# Optional platform-specific arguments for clang-tidy itself.  For example,
# a self-contained LLVM install can use --extra-arg=--gcc-toolchain=/usr.
TIDY_EXTRA_ARGS ?=
PYTHON ?= python3
ENGINE ?= ../SHAYVERI
CXXFLAGS ?= -std=c++20 -O3 -g -Wall -Wextra -Wpedantic -mbmi2 -march=native
INCLUDES := -I../include

STRICT_WARN_FLAGS := \
	-Wshadow=compatible-local \
	-Wconversion \
	-Wformat=2 \
	-Wnull-dereference \
	-Wduplicated-cond \
	-Wduplicated-branches \
	-Wlogical-op

STRICT_CXXFLAGS := $(CXXFLAGS) $(STRICT_WARN_FLAGS) -Werror

ASAN ?= 1
UBSAN ?= 1
LSAN ?= 1

ASAN_FLAG := $(if $(filter 1,$(ASAN)),-fsanitize=address)
UBSAN_FLAG := $(if $(filter 1,$(UBSAN)),-fsanitize=undefined)
# AddressSanitizer includes LeakSanitizer.  Link the standalone leak runtime
# only when leak checks are enabled without AddressSanitizer.
LSAN_FLAG := $(if $(and $(filter 1,$(LSAN)),$(filter-out 1,$(ASAN))),-fsanitize=leak)
SANITIZER_FLAGS := $(ASAN_FLAG) $(UBSAN_FLAG) $(LSAN_FLAG)

SANITIZE_CXXFLAGS := \
	-std=c++20 \
	-O1 \
	-g3 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-mbmi2 \
	-march=native \
	-fno-omit-frame-pointer \
	$(SANITIZER_FLAGS) \
	-fno-sanitize-recover=all
SANITIZE_LDFLAGS := -lpthread $(SANITIZER_FLAGS)
SANITIZE_BIN := SHAYVERI_sanitize
SANITIZE_DATAGEN_GAMES ?= 100
SANITIZE_DATAGEN_THREADS ?= 2
SANITIZE_CUTECHESS_GAMES ?= 200
SANITIZE_CUTECHESS_TC ?= 0.3+0.03
SANITIZE_CUTECHESS_CONCURRENCY ?= 20
CUTECHESS ?= cutechess-cli
SANITIZER_ENV := ASAN_OPTIONS=detect_leaks=$(LSAN):halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 SHAYVERI_ENGINE=../$(SANITIZE_BIN)

DEFAULT_NNUE := ../SHAYVERI2_5_0.nnue
DEBUG_EMBEDDED_NNUE_SRC := embedded_nnue_stub.cpp

COMMON_SRC := \
	../src/board.cpp \
	../src/fen.cpp \
	../src/attacks.cpp \
	../src/make.cpp \
	../src/move_gen.cpp \
	../src/move_io.cpp \
	../src/evaluate.cpp \
	../src/search.cpp \
	../src/nnue.cpp \
	$(DEBUG_EMBEDDED_NNUE_SRC) \
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
ATTACK_TABLE_SRC := ../src/attacks.cpp ../src/board.cpp ../src/zobrist.cpp attack_tables.cpp
TIME_MANAGER_SRC := ../src/time_manager.cpp time_manager_test.cpp
POSITION_RULES_SRC := \
	../src/attacks.cpp \
	../src/board.cpp \
	../src/fen.cpp \
	../src/zobrist.cpp \
	position_rules.cpp
SEARCH_REGRESSION_SRC := $(COMMON_SRC) search_regression_test.cpp
NNUE_TEST_SRC := \
	../src/attacks.cpp \
	../src/board.cpp \
	../src/fen.cpp \
	../src/make.cpp \
	../src/move_gen.cpp \
	../src/nnue.cpp \
	$(DEBUG_EMBEDDED_NNUE_SRC) \
	../src/nnue_avx2.cpp \
	../src/nnue_update.cpp \
	../src/zobrist.cpp \
	nnue_test.cpp

STRICT_SRC := \
	../src/attacks.cpp \
	../src/board.cpp \
	../src/datagen.cpp \
	../src/datagen_cli.cpp \
	../src/evaluate.cpp \
	../src/fen.cpp \
	../src/make.cpp \
	../src/move_gen.cpp \
	../src/move_io.cpp \
	../src/nnue.cpp \
	../src/nnue_avx2.cpp \
	../src/nnue_update.cpp \
	../src/opening_book.cpp \
	../src/search.cpp \
	../src/see.cpp \
	../src/time_manager.cpp \
	../src/tools/embed_nnue.cpp \
	../src/tt.cpp \
	../src/uci.cpp \
	../src/zobrist.cpp \
	$(DEBUG_EMBEDDED_NNUE_SRC)

TIDY_SRC := \
	$(STRICT_SRC) \
	perft.cpp \
	legality.cpp \
	fen_state.cpp \
	make_unmake.cpp \
	see_tests.cpp \
	eval_symmetry.cpp \
	attack_tables.cpp \
	tt_safety.cpp \
	time_manager_test.cpp \
	position_rules.cpp \
	search_regression_test.cpp \
	nnue_test.cpp

BIN := perft
LEGALITY_BIN := legality
FEN_STATE_BIN := fen_state
MAKE_UNMAKE_BIN := make_unmake
SEE_TESTS_BIN := see_tests
EVAL_SYM_BIN := eval_symmetry
TT_SAFETY_BIN := tt_safety
ATTACK_TABLE_BIN := attack_tables
TIME_MANAGER_BIN := time_manager
POSITION_RULES_BIN := position_rules
SEARCH_REGRESSION_BIN := search_regression
NNUE_TEST_BIN := nnue_test

all: $(BIN) $(FEN_STATE_BIN) $(MAKE_UNMAKE_BIN) $(LEGALITY_BIN) $(SEE_TESTS_BIN) $(EVAL_SYM_BIN) $(TT_SAFETY_BIN) $(ATTACK_TABLE_BIN) $(TIME_MANAGER_BIN) $(POSITION_RULES_BIN) $(SEARCH_REGRESSION_BIN) $(NNUE_TEST_BIN)

TEST_TARGETS := \
	perft_suite \
	fen_state_suite \
	make_unmake_suite \
	legality_suite \
	see_suite \
	eval_symmetry_suite \
	attack_table_suite \
	tt_safety_suite \
	time_manager_suite \
	position_rules_suite \
	search_regression_suite \
	nnue_suite \
	datagen_check \
	uci_check \
	time_control_integration \
	fixed_search_integration \
	tactical_regression

test:
	+$(MAKE) strict
	+$(MAKE) tidy
	+$(MAKE) runtime_tests

runtime_tests:
	+$(MAKE) $(TEST_TARGETS)

# Full runtime-memory/undefined-behavior gate.  The instrumented engine has a
# separate name, so the normal release binary is never replaced or deleted.
sanitize:
	@test "$(ASAN)" = 0 -o "$(ASAN)" = 1 || { echo "ASAN must be 0 or 1" >&2; exit 1; }
	@test "$(UBSAN)" = 0 -o "$(UBSAN)" = 1 || { echo "UBSAN must be 0 or 1" >&2; exit 1; }
	@test "$(LSAN)" = 0 -o "$(LSAN)" = 1 || { echo "LSAN must be 0 or 1" >&2; exit 1; }
	@test "$(ASAN)$(UBSAN)$(LSAN)" != 000 || { echo "enable at least one sanitizer" >&2; exit 1; }
	+$(MAKE) clean
	+$(MAKE) -B -C .. BIN="$(SANITIZE_BIN)" CXXFLAGS="$(SANITIZE_CXXFLAGS)" LDFLAGS="$(SANITIZE_LDFLAGS)" all
	+$(SANITIZER_ENV) $(MAKE) ENGINE="../$(SANITIZE_BIN)" CXXFLAGS="$(SANITIZE_CXXFLAGS)" runtime_tests
	+$(SANITIZER_ENV) SHAYVERI_DATAGEN_MATRIX=1 SHAYVERI_DATAGEN_GAMES="$(SANITIZE_DATAGEN_GAMES)" SHAYVERI_DATAGEN_THREADS="$(SANITIZE_DATAGEN_THREADS)" SHAYVERI_DATAGEN_TIMEOUT_SEC=180 $(PYTHON) datagen_check.py
	+$(MAKE) sanitize_cutechess
	+$(MAKE) clean

sanitize_cutechess:
	@command -v "$(CUTECHESS)" >/dev/null || { echo "cutechess executable not found: $(CUTECHESS)" >&2; exit 1; }
	@test $$(( $(SANITIZE_CUTECHESS_GAMES) % 2 )) -eq 0 || { echo "SANITIZE_CUTECHESS_GAMES must be even" >&2; exit 1; }
	$(SANITIZER_ENV) $(CUTECHESS) \
		-engine cmd=../$(SANITIZE_BIN) name=SHAYVERI-sanitize-A option.Threads=1 option.OwnBook=false \
		-engine cmd=../$(SANITIZE_BIN) name=SHAYVERI-sanitize-B option.Threads=1 option.OwnBook=false \
		-each proto=uci tc=$(SANITIZE_CUTECHESS_TC) \
		-games 2 -rounds $$(( $(SANITIZE_CUTECHESS_GAMES) / 2 )) -repeat -concurrency $(SANITIZE_CUTECHESS_CONCURRENCY)

strict:
	$(CXX) $(STRICT_CXXFLAGS) $(INCLUDES) -fsyntax-only $(STRICT_SRC)

tidy:
	$(TIDY) $(TIDY_EXTRA_ARGS) $(TIDY_SRC) -- -std=c++20 $(INCLUDES) -march=native -mbmi2

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

$(ATTACK_TABLE_BIN): $(ATTACK_TABLE_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(ATTACK_TABLE_SRC)

$(TIME_MANAGER_BIN): $(TIME_MANAGER_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(TIME_MANAGER_SRC)

$(POSITION_RULES_BIN): $(POSITION_RULES_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(POSITION_RULES_SRC)

$(SEARCH_REGRESSION_BIN): $(SEARCH_REGRESSION_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SEARCH_REGRESSION_SRC)

$(NNUE_TEST_BIN): $(NNUE_TEST_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(NNUE_TEST_SRC)

nnue_classic_test.nnue:
	$(PYTHON) -c "import struct; h=256; magic=0x4E4E5545; open('nnue_classic_test.nnue','wb').write(struct.pack('<II', magic, 2) + bytes(768*h*2) + bytes(h*2) + bytes(h*2*2) + bytes(4))"

nnue_kb8_test.nnue:
	$(PYTHON) -c "import struct; h=256; magic=0x4E4E5545; open('nnue_kb8_test.nnue','wb').write(struct.pack('<III', magic, 3, 8) + bytes(768*8*h*2) + bytes(h*2) + bytes(h*2*2) + bytes(4))"

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

attack_table_suite: $(ATTACK_TABLE_BIN)
	./$(ATTACK_TABLE_BIN)

time_manager_suite: $(TIME_MANAGER_BIN)
	./$(TIME_MANAGER_BIN)

position_rules_suite: $(POSITION_RULES_BIN)
	./$(POSITION_RULES_BIN)

search_regression_suite: $(SEARCH_REGRESSION_BIN)
	./$(SEARCH_REGRESSION_BIN)

nnue_suite: $(NNUE_TEST_BIN) nnue_classic_test.nnue nnue_kb8_test.nnue $(DEFAULT_NNUE)
	./$(NNUE_TEST_BIN) nnue_classic_test.nnue
	./$(NNUE_TEST_BIN) nnue_kb8_test.nnue
	./$(NNUE_TEST_BIN) $(DEFAULT_NNUE)

datagen_check: ensure_engine
	$(PYTHON) datagen_check.py

ensure_engine:
	@if [ ! -x "$(ENGINE)" ]; then echo "engine binary not found: $(ENGINE)" >&2; exit 1; fi

uci_check: ensure_engine
	$(PYTHON) uci_check.py

time_control_integration: ensure_engine
	$(PYTHON) time_control_integration.py

fixed_search_integration: ensure_engine
	$(PYTHON) fixed_search_integration.py

tactical_regression: ensure_engine
	$(PYTHON) tactical_regression.py

clean:
	rm -f $(BIN) $(LEGALITY_BIN) $(FEN_STATE_BIN) $(MAKE_UNMAKE_BIN) $(SEE_TESTS_BIN) $(EVAL_SYM_BIN) $(TT_SAFETY_BIN) $(ATTACK_TABLE_BIN) $(TIME_MANAGER_BIN) $(POSITION_RULES_BIN) $(SEARCH_REGRESSION_BIN) $(NNUE_TEST_BIN) nnue_classic_test.nnue nnue_kb8_test.nnue
	find . -type d -name __pycache__ -prune -exec rm -rf {} +

.PHONY: all test runtime_tests strict tidy sanitize sanitize_cutechess perft_suite fen_state_suite make_unmake_suite legality_suite see_suite eval_symmetry_suite tt_safety_suite attack_table_suite time_manager_suite position_rules_suite search_regression_suite nnue_suite datagen_check ensure_engine uci_check time_control_integration fixed_search_integration tactical_regression clean
