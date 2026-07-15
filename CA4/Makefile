# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -O2

# Directories
JUNK_DIR = Junk
SENDER_DIR = $(JUNK_DIR)/Sender
RECEIVER_DIR = $(JUNK_DIR)/Receiver
LOG_DIR = $(JUNK_DIR)/Logs

# Executable names
SENDER = $(JUNK_DIR)/sender
RECEIVER = $(JUNK_DIR)/receiver

# Paths and Parameters
PORT = 8080
IP = 127.0.0.1
INPUT_FILE = $(SENDER_DIR)/sample_input.txt
OUTPUT_FILE = $(RECEIVER_DIR)/sample_output.txt

# Simulation Defaults
LOSS ?= 0.1
DELAY ?= 50

all: directories $(SENDER) $(RECEIVER)

directories:
	mkdir -p $(JUNK_DIR) $(SENDER_DIR) $(RECEIVER_DIR) $(LOG_DIR)

$(SENDER): sender.cpp packet.hpp logger.hpp
	$(CXX) $(CXXFLAGS) sender.cpp -o $(SENDER)

$(RECEIVER): receiver.cpp packet.hpp logger.hpp
	$(CXX) $(CXXFLAGS) receiver.cpp -o $(RECEIVER)

run: all
	@echo "Generating large test file in $(INPUT_FILE)..."
	@rm -f $(INPUT_FILE)
	@for i in `seq 1 200`; do \
		echo "Line $$i: This is a robust test file for Mini-TCP to guarantee multiple packets." >> $(INPUT_FILE); \
	done
	
	@# Start tmux session for split-screen testing
	tmux new-session -d -s minitcp "./$(RECEIVER) $(PORT) $(OUTPUT_FILE) --loss $(LOSS) --delay $(DELAY); echo '--- Receiver finished ---'; exec bash"
	sleep 0.5
	tmux split-window -h -t minitcp "./$(SENDER) $(IP) $(PORT) $(INPUT_FILE) $(LOG_DIR); echo '--- Sender finished ---'; exec bash"
	tmux attach-session -t minitcp

test: all directories
	@echo "========================================"
	@echo "Running Automated Tests for Phase 5"
	@echo "========================================"
	@echo "Cleaning up zombie processes..."
	@-killall -9 receiver sender 2>/dev/null || true
	@mkdir -p Junk/Plots
	@# Generate a test file if it doesn't exist
	@rm -f $(INPUT_FILE)
	@for i in `seq 1 1000`; do echo "Line $$i: Automated test data for plotting phase." >> $(INPUT_FILE); done

	@echo "\n>>> RUNNING TEST 1: Loss = 0%, Delay = 0ms <<<"
	./$(RECEIVER) $(PORT) $(OUTPUT_FILE) --loss 0.0 --delay 0 > /dev/null 2>&1 & PID=$$!; \
	sleep 0.5; \
	./$(SENDER) $(IP) $(PORT) $(INPUT_FILE) > /dev/null; \
	wait $$PID; \
	mv $(LOG_DIR)/cwnd.csv $(LOG_DIR)/cwnd_test1.csv; \
	mv $(LOG_DIR)/events.log $(LOG_DIR)/events_test1.log

	@echo "\n>>> RUNNING TEST 2: Loss = 10%, Delay = 50ms <<<"
	./$(RECEIVER) $(PORT) $(OUTPUT_FILE) --loss 0.1 --delay 50 > /dev/null 2>&1 & PID=$$!; \
	sleep 0.5; \
	./$(SENDER) $(IP) $(PORT) $(INPUT_FILE) > /dev/null; \
	wait $$PID; \
	mv $(LOG_DIR)/cwnd.csv $(LOG_DIR)/cwnd_test2.csv; \
	mv $(LOG_DIR)/events.log $(LOG_DIR)/events_test2.log

	@echo "\n>>> RUNNING TEST 3: Loss = 20%, Delay = 100ms <<<"
	./$(RECEIVER) $(PORT) $(OUTPUT_FILE) --loss 0.2 --delay 100 > /dev/null 2>&1 & PID=$$!; \
	sleep 0.5; \
	./$(SENDER) $(IP) $(PORT) $(INPUT_FILE) > /dev/null; \
	wait $$PID; \
	mv $(LOG_DIR)/cwnd.csv $(LOG_DIR)/cwnd_test3.csv; \
	mv $(LOG_DIR)/events.log $(LOG_DIR)/events_test3.log

	@echo "\n>>> RUNNING TEST 4 (BONUS): Loss = 10%, Delay = 50ms with FAST RECOVERY <<<"
	./$(RECEIVER) $(PORT) $(OUTPUT_FILE) --loss 0.1 --delay 50 > /dev/null 2>&1 & PID=$$!; \
	sleep 0.5; \
	./$(SENDER) $(IP) $(PORT) $(INPUT_FILE) --fast > /dev/null; \
	wait $$PID; \
	mv $(LOG_DIR)/cwnd.csv $(LOG_DIR)/cwnd_test4.csv; \
	mv $(LOG_DIR)/events.log $(LOG_DIR)/events_test4.log

	@echo "\n========================================"
	@echo "Tests Completed. Generating Python Plots..."
	@echo "========================================"
	@python3 plot_results.py

clean:
	rm -rf $(JUNK_DIR)
	-tmux kill-session -t minitcp 2>/dev/null || true

