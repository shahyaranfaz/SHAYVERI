MAKEFLAGS += -j$(shell nproc)

CXX := g++
CXX_WIN := x86_64-w64-mingw32-g++

LTO := -flto=auto

CXXFLAGS := \
	-std=c++20 \
	-O3 \
	-mbmi2 \
	-march=native \
	-Wall \
	-Wextra \
	-Wpedantic \
	$(LTO)

LDFLAGS := -lpthread $(LTO)

CXXFLAGS_WIN := \
	-std=c++20 \
	-O3 \
	-mbmi2 \
	-march=native \
	$(LTO)

LDFLAGS_WIN := -lpthread -static $(LTO)

INCLUDES := -Iinclude

SRC := \
	src/board.cpp \
	src/fen.cpp \
	src/attacks.cpp \
	src/make.cpp \
	src/move_gen.cpp \
	src/evaluate.cpp \
	src/search.cpp \
	src/uci.cpp \
	src/zobrist.cpp \
	src/tt.cpp \
	src/see.cpp \
	src/time_manager.cpp \
	src/texel.cpp

BIN := SHAYVERI
BIN_WIN := SHAYVERI.exe

all: $(BIN)

windows: $(BIN_WIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS)

$(BIN_WIN): $(SRC)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_WIN)

clean:
	rm -f $(BIN) $(BIN_WIN) dump_keys

dump_keys: \
	opening_stuff/dump_zobrist_keys.cpp \
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

.PHONY: all windows clean