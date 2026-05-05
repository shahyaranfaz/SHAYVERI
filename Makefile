CXX := g++
CXXFLAGS := -std=c++20 -O3 -mbmi2 -march=native -Wall -Wextra -Wpedantic
LDFLAGS := -lpthread -flto
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

BIN := shaybot
BIN_WIN := shaybot.exe
CXX_WIN := x86_64-w64-mingw32-g++
CXXFLAGS_WIN := -std=c++20 -O3 -mbmi2 -march=native
LDFLAGS_WIN := -lpthread -static -flto

all: $(BIN)

windows: $(BIN_WIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS)

$(BIN_WIN): $(SRC)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_WIN)

clean:
	rm -f $(BIN) $(BIN_WIN)

dump_keys: opening_stuff/dump_zobrist_keys.cpp src/board.cpp src/fen.cpp src/attacks.cpp src/make.cpp src/move_gen.cpp src/evaluate.cpp src/search.cpp src/zobrist.cpp src/tt.cpp src/see.cpp src/time_manager.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

.PHONY: all windows clean