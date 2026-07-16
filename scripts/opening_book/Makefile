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

DUMP_KEYS_BIN := outputs/dump_zobrist_keys
OUTPUTS_DIR := outputs
ZOBRIST_KEYS := $(OUTPUTS_DIR)/zobrist_keys.json
TABLE_CSV := $(OUTPUTS_DIR)/table.csv
BOOK_HEADER := ../../include/opening_book.h

DUMP_KEYS_SRC := \
	dump_zobrist_keys.cpp \
	../../src/zobrist.cpp

DUMP_KEYS_HEADERS := \
	../../include/zobrist.h \
	../../include/board.h \
	../../include/types.h

all: run_all

run_all: fetch table keys book

book: $(BOOK_HEADER)

outputs:
	mkdir -p $(OUTPUTS_DIR)

fetch:
	@if [ -f $(OUTPUTS_DIR)/twic.pgn ]; then \
		echo "Using existing corpus: $(OUTPUTS_DIR)/twic.pgn"; \
	else \
		bash fetch_twic.sh 920 2500 $(OUTPUTS_DIR)/twic.pgn; \
	fi

update:
	bash fetch_twic.sh

table: $(TABLE_CSV)

keys: $(ZOBRIST_KEYS)

dump_keys: $(DUMP_KEYS_BIN)

$(DUMP_KEYS_BIN): $(DUMP_KEYS_SRC) $(DUMP_KEYS_HEADERS) | outputs
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(DUMP_KEYS_SRC) $(LDFLAGS)

$(ZOBRIST_KEYS): $(DUMP_KEYS_BIN) | outputs
	./$(DUMP_KEYS_BIN) $(ZOBRIST_KEYS)

$(TABLE_CSV): build_table.py $(OUTPUTS_DIR)/twic.pgn | outputs
	python3 build_table.py

$(BOOK_HEADER): build_header.py $(TABLE_CSV) $(ZOBRIST_KEYS) | outputs
	python3 build_header.py

clean:
	rm -f $(DUMP_KEYS_BIN)

.PHONY: all run_all book outputs fetch update table keys dump_keys clean
