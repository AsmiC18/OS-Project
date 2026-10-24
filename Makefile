CC ?= cc
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Wshadow
CPPFLAGS += -Iinclude
LDLIBS += -lm

BUILD := build
TARGET := $(BUILD)/aegisos-sim
OBJ := $(BUILD)/aegisos_sim.o $(BUILD)/main.o
TEST := $(BUILD)/test_sim

.PHONY: all test experiment clean help live-monitor live-report live-help live doctor
.DEFAULT_GOAL := all

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/aegisos_sim.o: src/aegisos_sim.c include/aegisos_sim.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/main.o: src/main.c include/aegisos_sim.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDLIBS)

$(TEST): tests/test_sim.c $(BUILD)/aegisos_sim.o include/aegisos_sim.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sim.c $(BUILD)/aegisos_sim.o -o $@ $(LDLIBS)

test: $(TEST) $(TARGET)
	python3 -m py_compile scripts/*.py tests/*.py
	./$(TEST)
	python3 tests/test_live_monitor.py
	python3 tests/test_dashboard.py
	./$(TARGET) run --policy adaptive --scenario bursty --seed 7 --duration 1000 >/dev/null

experiment: $(TARGET)
	mkdir -p results
	./$(TARGET) experiment --runs 20 --duration 10000 --out results/raw_results.csv
	python3 scripts/analyze_results.py results/raw_results.csv --out results

clean:
	rm -rf $(BUILD)

help:
	@echo 'make            Build the AegisOS simulator'
	@echo 'make test       Run simulator tests'
	@echo 'make experiment Run 400 default simulations and summarize results'
	@echo 'make live             Run monitor and local browser dashboard together'
	@echo 'make doctor           Check live-control prerequisites'
	@echo 'make live-monitor Run continuous real /proc monitoring'
	@echo 'make live-report Summarize saved live_data logs'
	@echo 'make clean      Remove compiled files'

.PHONY: live-monitor live-report live-help
live-monitor:
	python3 scripts/aegisos_live.py monitor

live:
	bash scripts/start_aegisos.sh

doctor:
	python3 scripts/aegisos_live.py doctor

live-report:
	python3 scripts/summarize_live.py --log-dir live_data

live-help:
	python3 scripts/aegisos_live.py --help
	@echo 'Monitor only: make live-monitor'
	@echo 'Live controller example: python3 scripts/aegisos_live.py run --foreground "sleep 300" --background "python3 -c \\\"while True: sum(range(100000))\\\""'
