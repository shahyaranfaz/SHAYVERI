MAKEFLAGS += -j$(shell nproc)

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

HEADERS := $(wildcard include/*.h)

BIN := SHAYVERI
BIN_WIN := SHAYVERI.exe
BIN_MACOS := SHAYVERI_mac

all: $(BIN)

windows: $(BIN_WIN)

macos: $(BIN_MACOS)

$(BIN): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS)

$(BIN_WIN): $(SRC) $(HEADERS)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_WIN)

$(BIN_MACOS): $(SRC) $(HEADERS)
	$(CXX_MACOS) $(CXXFLAGS_MACOS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_MACOS)

clean:
	rm -f $(BIN) $(BIN_WIN) $(BIN_MACOS) dump_keys

dump_keys: \
	opening_book/dump_zobrist_keys.cpp \
	src/board.cpp \
	src/fen.cpp \
	src/attacks.cpp \
	src/make.cpp \
	src/move_gen.cpp \
	src/evaluate.cpp \
	src/search.cpp \
	src/zobrist.cpp \
	src/tt.cpp \
	src/see.cpp \
	src/time_manager.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

.PHONY: all windows macos clean
