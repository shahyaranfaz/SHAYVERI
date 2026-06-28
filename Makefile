MAKEFLAGS += -j$(shell nproc)

CXX := g++
CXX_WIN := x86_64-w64-mingw32-g++
CXX_MACOS := clang++
PYTHON ?= python3

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
EMBEDDED_NNUE_SRC := build/embedded_nnue.cpp

SRC += $(EMBEDDED_NNUE_SRC)

HEADERS := $(wildcard include/*.h)

BIN := SHAYVERI
BIN_WIN := SHAYVERI.exe
BIN_MACOS := SHAYVERI_mac

all: $(BIN)

windows: $(BIN_WIN)

macos: $(BIN_MACOS)

$(EMBEDDED_NNUE_SRC): $(DEFAULT_NNUE) scripts/embed_nnue.py
	$(PYTHON) scripts/embed_nnue.py $(DEFAULT_NNUE) $@ --name $(DEFAULT_NNUE)

$(BIN): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS)

$(BIN_WIN): $(SRC) $(HEADERS)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_WIN)

$(BIN_MACOS): $(SRC) $(HEADERS)
	$(CXX_MACOS) $(CXXFLAGS_MACOS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_MACOS)

clean:
	rm -f $(BIN) $(BIN_WIN) $(BIN_MACOS)
	rm -rf build
	$(MAKE) -C debug clean

test: $(BIN)
	$(MAKE) -C debug test

release_test: $(BIN)
	$(MAKE) -C debug release_test

.PHONY: all windows macos clean test release_test
