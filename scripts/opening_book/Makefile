CXX := g++

LTO := -flto=auto

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

INCLUDES := -I../../include
LDFLAGS := -lpthread $(LTO)

DUMP_KEYS_BIN := dump_keys
ARTIFACTS_DIR := artifacts
ZOBRIST_KEYS := $(ARTIFACTS_DIR)/zobrist_keys.json
TABLE_CSV := $(ARTIFACTS_DIR)/table.csv
BOOK_HEADER := ../../include/opening_book.h

DUMP_KEYS_SRC := \
	dump_zobrist_keys.cpp \
	../../src/board.cpp \
	../../src/fen.cpp \
	../../src/attacks.cpp \
	../../src/make.cpp \
	../../src/move_gen.cpp \
	../../src/evaluate.cpp \
	../../src/search.cpp \
	../../src/zobrist.cpp \
	../../src/tt.cpp \
	../../src/see.cpp \
	../../src/time_manager.cpp

all: book

run_all: book

book: $(BOOK_HEADER)

artifacts:
	mkdir -p $(ARTIFACTS_DIR)

fetch:
	bash fetch_twic.sh

table: $(TABLE_CSV)

keys: $(ZOBRIST_KEYS)

dump_keys: $(DUMP_KEYS_BIN)

$(DUMP_KEYS_BIN): $(DUMP_KEYS_SRC) ../../include/*.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(DUMP_KEYS_SRC) $(LDFLAGS)

$(ZOBRIST_KEYS): $(DUMP_KEYS_BIN) | artifacts
	./$(DUMP_KEYS_BIN) $(ZOBRIST_KEYS)

$(TABLE_CSV): build_table.py fetch_twic.sh | artifacts
	python3 build_table.py

$(BOOK_HEADER): build_header.py $(TABLE_CSV) $(ZOBRIST_KEYS) | artifacts
	python3 build_header.py

clean:
	rm -f $(DUMP_KEYS_BIN)

.PHONY: all run_all book artifacts fetch table keys dump_keys clean
