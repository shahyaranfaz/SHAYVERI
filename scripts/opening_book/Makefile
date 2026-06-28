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

dump_keys: $(DUMP_KEYS_BIN)

$(DUMP_KEYS_BIN): $(DUMP_KEYS_SRC) ../../include/*.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(DUMP_KEYS_SRC) $(LDFLAGS)

clean:
	rm -f $(DUMP_KEYS_BIN)

.PHONY: dump_keys clean
