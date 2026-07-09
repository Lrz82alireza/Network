# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -O2

# Executable names
SENDER = sender
RECEIVER = receiver

# Paths and Parameters
PORT = 8080
IP = 127.0.0.1
SENDER_DIR = Junk/Sender
RECEIVER_DIR = Junk/Receiver
INPUT_FILE = $(SENDER_DIR)/sample_input.txt
OUTPUT_FILE = $(RECEIVER_DIR)/sample_output.txt

all: $(SENDER) $(RECEIVER)

$(SENDER): sender.cpp packet.hpp logger.hpp
	$(CXX) $(CXXFLAGS) sender.cpp -o $(SENDER)

$(RECEIVER): receiver.cpp packet.hpp logger.hpp
	$(CXX) $(CXXFLAGS) receiver.cpp -o $(RECEIVER)

run: all
	@# Create required directories
	mkdir -p $(SENDER_DIR) $(RECEIVER_DIR)
	
	@# Generate a large test file (approx 200-300KB) to ensure multiple packets
	@echo "Generating large test file in $(INPUT_FILE)..."
	@rm -f $(INPUT_FILE)
	@for i in `seq 1 100`; do \
		echo "Line $$i: This is a robust test file for Mini-TCP to guarantee multiple packets." >> $(INPUT_FILE); \
	done
	
	@# Start tmux session for split-screen testing
	tmux new-session -d -s minitcp "./$(RECEIVER) $(PORT) $(OUTPUT_FILE); echo '--- Receiver finished ---'; exec bash"
	sleep 0.5
	tmux split-window -h -t minitcp "./$(SENDER) $(IP) $(PORT) $(INPUT_FILE); echo '--- Sender finished ---'; exec bash"
	tmux attach-session -t minitcp

clean:
	rm -f $(SENDER) $(RECEIVER)
	rm -rf Junk/
	-tmux kill-session -t minitcp 2>/dev/null || true