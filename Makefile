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
	src/evaluate.cpp \
	src/fen.cpp \
	src/make.cpp \
	src/move_gen.cpp \
	src/nnue.cpp \
	src/nnue_avx2.cpp \
	src/nnue_update.cpp \
	src/search.cpp \
	src/see.cpp \
	src/uci.cpp \
	src/texel.cpp \
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
PGO_GENERATE_FLAGS := -fprofile-generate=$(abspath $(PGO_DATA_DIR)) -fprofile-update=atomic
PGO_USE_FLAGS := -fprofile-use=$(abspath $(PGO_DATA_DIR)) -fprofile-correction

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
	rm -rf $(PGO_DATA_DIR) $(PGO_TRAIN_DIR)
	mkdir -p $(PGO_DATA_DIR) $(PGO_TRAIN_DIR)
	$(CXX) $(CXXFLAGS) $(PGO_GENERATE_FLAGS) $(INCLUDES) -o $(PGO_BIN) $(SRC) $(LDFLAGS) $(PGO_GENERATE_FLAGS)

pgo-train: pgo-generate
	python3 scripts/profiling/baseline.py \
		--engine ./$(PGO_BIN) \
		--output-dir $(PGO_TRAIN_DIR) \
		--run-name representative \
		--runs 1 \
		--threads 1,2,4,8

pgo: pgo-train
	find $(PGO_DATA_DIR) -type f -name '*.gcda' -print -quit | grep -q .
	$(CXX) $(CXXFLAGS) $(PGO_USE_FLAGS) $(INCLUDES) -o $(PGO_BIN) $(SRC) $(LDFLAGS) $(PGO_USE_FLAGS)

clean:
	rm -f $(BIN) $(BIN_WIN) $(BIN_MACOS) $(PGO_BIN)
	rm -rf build
	$(MAKE) -C debug clean

test: $(BIN)
	$(MAKE) -C debug test

.PHONY: all windows macos pgo pgo-generate pgo-train clean test
