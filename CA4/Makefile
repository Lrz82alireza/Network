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

clean:
	rm -rf $(JUNK_DIR)
	-tmux kill-session -t minitcp 2>/dev/null || true