
# AegisOS Makefile
# Targets: all, test, workloads, legacy, clean, help

CC      ?= cc
CFLAGS  ?= -std=gnu11 -Wall -Wextra -Wshadow -O2 -g
CPPFLAGS += -Iinclude
LDLIBS  ?=

BUILD   := build
TARGET  := $(BUILD)/aegisos

SRCS      := $(wildcard src/*.c)
OBJS      := $(patsubst src/%.c,$(BUILD)/obj/%.o,$(SRCS))
LIB_OBJS  := $(filter-out $(BUILD)/obj/main.o,$(OBJS))

TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/tests/%,$(TEST_SRCS))
TEST_SH   := $(wildcard tests/test_*.sh)
API_SERVER := $(BUILD)/tests/run_api_server

WORK_SRCS := $(wildcard workloads/*.c)
WORK_BINS := $(patsubst workloads/%.c,$(BUILD)/workloads/%,$(WORK_SRCS))

LEGACY_SRCS := $(wildcard legacy/aegisos-core/src/*.c)
LEGACY_BIN  := $(BUILD)/legacy/aegis_legacy

.PHONY: all test workloads legacy clean help
.DEFAULT_GOAL := all

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(BUILD)/obj/%.o: src/%.c $(wildcard include/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ---- tests -------------------------------------------------------------
# Exit code 77 means skipped / not implemented yet.
$(BUILD)/tests/%: tests/%.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDLIBS)

# Helper used by tests/test_api.sh to start the HTTP server.
$(API_SERVER): tests/run_api_server.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDLIBS)


test: $(TEST_BINS) $(API_SERVER)
	@pass=0; skip=0; fail=0; \
	for t in $(TEST_BINS); do \
	  "$$t"; rc=$$?; \
	  if [ $$rc -eq 0 ]; then pass=$$((pass+1)); \
	  elif [ $$rc -eq 77 ]; then skip=$$((skip+1)); \
	  else fail=$$((fail+1)); echo "FAIL: $$t (exit $$rc)"; fi; \
	done; \
	for t in $(TEST_SH); do \
	  sh "$$t"; rc=$$?; \
	  if [ $$rc -eq 0 ]; then pass=$$((pass+1)); \
	  elif [ $$rc -eq 77 ]; then skip=$$((skip+1)); \
	  else fail=$$((fail+1)); echo "FAIL: $$t (exit $$rc)"; fi; \
	done; \
	echo "tests: $$pass passed, $$skip skipped (not implemented), $$fail failed"; \
	[ $$fail -eq 0 ]


# ---- workloads ---------------------------------------------------------
$(BUILD)/workloads/%: workloads/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDLIBS)

workloads: $(WORK_BINS)

# ---- legacy baseline (reference only) ----------------------------------
$(LEGACY_BIN): $(LEGACY_SRCS) $(wildcard legacy/aegisos-core/src/*.h)
	@mkdir -p $(dir $@)
	$(CC) -std=gnu11 -Wall -Wextra -Wno-unused-result -O2 -Ilegacy/aegisos-core/src $(LEGACY_SRCS) -o $@

legacy: $(LEGACY_BIN)

clean:
	rm -rf $(BUILD)

help:
	@echo "make            build $(TARGET)"
	@echo "make test       build and run tests (exit 77 = skipped)"
	@echo "make workloads  build test workloads into $(BUILD)/workloads"
	@echo "make legacy     build the original baseline shell (reference only)"
	@echo "make clean      remove $(BUILD)/"
