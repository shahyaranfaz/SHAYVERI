CXX := g++
CXX_WIN := x86_64-w64-mingw32-g++
CXX_MACOS := clang++

LTO := -flto=auto
LTO_MACOS := -flto

OPT_FLAGS := \
	-Ofast \
	-DNDEBUG \
	-march=native \
	-mbmi \
	-mbmi2 \
	-mlzcnt \
	-mpopcnt \
	-fomit-frame-pointer \
	-funroll-loops \
	-fno-rtti \
	$(LTO)

WARN_FLAGS := \
	-Wall \
	-Wextra \
	-Wpedantic

CXXFLAGS := \
	-std=c++20 \
	$(OPT_FLAGS) \
	$(WARN_FLAGS)

LDFLAGS := -lpthread $(LTO)

CXXFLAGS_WIN := \
	-std=c++20 \
	$(OPT_FLAGS)

LDFLAGS_WIN := -lpthread -static $(LTO)

CXXFLAGS_MACOS := \
	-std=c++20 \
	-Ofast \
	-DNDEBUG \
	-march=native \
	-mbmi \
	-mbmi2 \
	-mlzcnt \
	-mpopcnt \
	-fomit-frame-pointer \
	-funroll-loops \
	-fno-rtti \
	$(LTO_MACOS) \
	$(WARN_FLAGS)

LDFLAGS_MACOS := $(LTO_MACOS)

INCLUDES := -Iinclude

SRC := \
	src/attacks.cpp \
	src/board.cpp \
	src/datagen.cpp \
	src/datagen_cli.cpp \
	src/evaluate.cpp \
	src/fen.cpp \
	src/make.cpp \
	src/move_gen.cpp \
	src/move_io.cpp \
	src/nnue.cpp \
	src/nnue_avx2.cpp \
	src/nnue_update.cpp \
	src/opening_book.cpp \
	src/search.cpp \
	src/see.cpp \
	src/uci.cpp \
	src/time_manager.cpp \
	src/tt.cpp \
	src/zobrist.cpp

DEFAULT_NNUE := SHAYVERI2_5_0.nnue
EMBED_NNUE := build/embed_nnue
EMBEDDED_NNUE_SRC := build/embedded_nnue.cpp

SRC += $(EMBEDDED_NNUE_SRC)

HEADERS := $(wildcard include/*.h)

BIN := SHAYVERI
BIN_WIN := SHAYVERI.exe
BIN_MACOS := SHAYVERI_mac
PGO_BIN ?= SHAYVERI_pgo
PGO_DATA_DIR ?= build/pgo-data
PGO_TRAIN_DIR ?= build/pgo-training
PGO_CXXFLAGS ?= $(CXXFLAGS)
PGO_LDFLAGS ?= $(LDFLAGS)
PGO_TRAIN_RUNS ?= 1
PGO_TRAIN_DEPTH ?= 12
PGO_TRAIN_NODES ?= 250000
PGO_TRAIN_MOVETIME ?= 500
PGO_TRAIN_THREADS ?= 1,2,4,8
PGO_TRAIN_TIMEOUT ?= 300
PGO_GENERATE_FLAGS := -fprofile-generate=$(abspath $(PGO_DATA_DIR)) -fprofile-update=atomic
PGO_USE_FLAGS := \
	-fprofile-use=$(abspath $(PGO_DATA_DIR)) \
	-fprofile-correction \
	-Werror=coverage-mismatch \
	-Werror=missing-profile
PGO_REPORT_FLAGS := -fopt-info-all
PGO_V3_CXXFLAGS := $(subst -march=native,-march=x86-64-v3,$(CXXFLAGS))

all: $(BIN)

windows: $(BIN_WIN)

macos: $(BIN_MACOS)

$(EMBED_NNUE): src/tools/embed_nnue.cpp
	mkdir -p build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Wpedantic -o $@ src/tools/embed_nnue.cpp

$(EMBEDDED_NNUE_SRC): $(DEFAULT_NNUE) $(EMBED_NNUE)
	$(EMBED_NNUE) $(DEFAULT_NNUE) $@ $(DEFAULT_NNUE)

$(BIN): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS)

$(BIN_WIN): $(SRC) $(HEADERS)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_WIN)

$(BIN_MACOS): $(SRC) $(HEADERS)
	$(CXX_MACOS) $(CXXFLAGS_MACOS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_MACOS)

pgo-generate: $(SRC) $(HEADERS)
	test "$$(uname -s)" = Linux || { echo "PGO is supported only on Linux" >&2; exit 2; }
	rm -rf $(PGO_DATA_DIR) $(PGO_TRAIN_DIR)
	mkdir -p $(PGO_DATA_DIR) $(PGO_TRAIN_DIR)
	$(CXX) $(PGO_CXXFLAGS) $(PGO_GENERATE_FLAGS) $(INCLUDES) -o $(PGO_BIN) $(SRC) $(PGO_LDFLAGS) $(PGO_GENERATE_FLAGS)
	{ \
		echo "compiler=$$($(CXX) --version | head -n 1)"; \
		echo "cxxflags=$(PGO_CXXFLAGS)"; \
		echo "ldflags=$(PGO_LDFLAGS)"; \
		echo "generate_flags=$(PGO_GENERATE_FLAGS)"; \
	} > $(PGO_TRAIN_DIR)/build-manifest.txt
	sha256sum $(PGO_BIN) > $(PGO_TRAIN_DIR)/instrumented.sha256

pgo-train:
	test -x ./$(PGO_BIN)
	python3 scripts/profiling/baseline.py \
		--engine ./$(PGO_BIN) \
		--output-dir $(PGO_TRAIN_DIR) \
		--run-name representative \
		--runs $(PGO_TRAIN_RUNS) \
		--depth $(PGO_TRAIN_DEPTH) \
		--nodes $(PGO_TRAIN_NODES) \
		--movetime $(PGO_TRAIN_MOVETIME) \
		--threads $(PGO_TRAIN_THREADS) \
		--fixed-depth-threads 1 \
		--fixed-node-threads 1 \
		--all-timed-cases \
		--pgo-cases \
		--timeout $(PGO_TRAIN_TIMEOUT) \
		--build-label pgo-instrumented \
		--build-flags '$(PGO_CXXFLAGS) $(PGO_LDFLAGS) $(PGO_GENERATE_FLAGS)'
	find $(PGO_DATA_DIR) -type f -name '*.gcda' -printf '%P %s bytes\n' | sort > $(PGO_TRAIN_DIR)/profile-files.txt
	test -s $(PGO_TRAIN_DIR)/profile-files.txt

pgo-use: $(SRC) $(HEADERS)
	test -s $(PGO_TRAIN_DIR)/profile-files.txt
	rm -f $(PGO_TRAIN_DIR)/compiler-opt.txt
	$(CXX) $(PGO_CXXFLAGS) $(PGO_USE_FLAGS) $(PGO_REPORT_FLAGS) $(INCLUDES) -o $(PGO_BIN) $(SRC) $(PGO_LDFLAGS) $(PGO_USE_FLAGS) 2> $(PGO_TRAIN_DIR)/compiler-opt.txt || { tail -n 80 $(PGO_TRAIN_DIR)/compiler-opt.txt; exit 1; }
	sha256sum $(PGO_BIN) > $(PGO_TRAIN_DIR)/optimized.sha256
	printf 'uci\nisready\nbench 16 1 3 default depth\nquit\n' | ./$(PGO_BIN) > $(PGO_TRAIN_DIR)/optimized-check.txt
	grep -q '^uciok$$' $(PGO_TRAIN_DIR)/optimized-check.txt
	grep -q '^readyok$$' $(PGO_TRAIN_DIR)/optimized-check.txt
	grep -q '^Nodes: 94602$$' $(PGO_TRAIN_DIR)/optimized-check.txt

pgo-report:
	test -s $(PGO_TRAIN_DIR)/build-manifest.txt
	test -s $(PGO_TRAIN_DIR)/profile-files.txt
	test -s $(PGO_TRAIN_DIR)/compiler-opt.txt
	cat $(PGO_TRAIN_DIR)/build-manifest.txt
	printf 'profile_files='; wc -l < $(PGO_TRAIN_DIR)/profile-files.txt
	printf 'optimization_notes='; wc -l < $(PGO_TRAIN_DIR)/compiler-opt.txt
	cat $(PGO_TRAIN_DIR)/instrumented.sha256 $(PGO_TRAIN_DIR)/optimized.sha256

pgo:
	$(MAKE) pgo-generate \
		PGO_BIN='$(PGO_BIN)' \
		PGO_DATA_DIR='$(PGO_DATA_DIR)' \
		PGO_TRAIN_DIR='$(PGO_TRAIN_DIR)' \
		PGO_CXXFLAGS='$(PGO_CXXFLAGS)' \
		PGO_LDFLAGS='$(PGO_LDFLAGS)'
	$(MAKE) pgo-train \
		PGO_BIN='$(PGO_BIN)' \
		PGO_DATA_DIR='$(PGO_DATA_DIR)' \
		PGO_TRAIN_DIR='$(PGO_TRAIN_DIR)' \
		PGO_CXXFLAGS='$(PGO_CXXFLAGS)' \
		PGO_LDFLAGS='$(PGO_LDFLAGS)'
	$(MAKE) pgo-use \
		PGO_BIN='$(PGO_BIN)' \
		PGO_DATA_DIR='$(PGO_DATA_DIR)' \
		PGO_TRAIN_DIR='$(PGO_TRAIN_DIR)' \
		PGO_CXXFLAGS='$(PGO_CXXFLAGS)' \
		PGO_LDFLAGS='$(PGO_LDFLAGS)'

pgo-v3:
	$(MAKE) pgo \
		PGO_BIN='$(PGO_BIN)' \
		PGO_DATA_DIR='$(PGO_DATA_DIR)' \
		PGO_TRAIN_DIR='$(PGO_TRAIN_DIR)' \
		PGO_CXXFLAGS='$(PGO_V3_CXXFLAGS)' \
		PGO_LDFLAGS='$(LDFLAGS)'

pgo-v3-pair:
	$(MAKE) pgo-v3 PGO_BIN=SHAYVERI_pgo_a \
		PGO_DATA_DIR=build/pgo-a-data PGO_TRAIN_DIR=build/pgo-a-training
	$(MAKE) pgo-v3 PGO_BIN=SHAYVERI_pgo_b \
		PGO_DATA_DIR=build/pgo-b-data PGO_TRAIN_DIR=build/pgo-b-training

clean:
	rm -f $(BIN) $(BIN_WIN) $(BIN_MACOS) $(PGO_BIN)
	rm -rf build
	$(MAKE) -C debug clean

test: $(BIN)
	$(MAKE) -C debug test

.PHONY: all windows macos pgo pgo-v3 pgo-v3-pair pgo-generate pgo-train pgo-use pgo-report clean test
