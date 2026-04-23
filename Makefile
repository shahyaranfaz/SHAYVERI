CXX := clang++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra -Wpedantic
LDFLAGS := -lpthread
INCLUDES := -Iinclude

SRC := \
	src/board.cpp \
	src/fen.cpp \
	src/attacks.cpp \
	src/make.cpp \
	src/move_gen.cpp \
	src/evaluate.cpp \
	src/search.cpp \
	src/uci.cpp

BIN := shaybot
BIN_WIN := shaybot.exe
CXX_WIN := x86_64-w64-mingw32-g++
CXXFLAGS_WIN := -std=c++20 -O3
LDFLAGS_WIN := -lpthread -static

all: $(BIN)

windows: $(BIN_WIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS)

$(BIN_WIN): $(SRC)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(INCLUDES) -o $@ $(SRC) $(LDFLAGS_WIN)

clean:
	rm -f $(BIN) $(BIN_WIN)

.PHONY: all windows clean