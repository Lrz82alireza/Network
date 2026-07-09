# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -O2

# Executable names
SENDER = sender
RECEIVER = receiver

# Test files and parameters
PORT = 8080
IP = 127.0.0.1
INPUT_FILE = sample_input.txt
OUTPUT_FILE = sample_output.txt

# Default target to compile both programs
all: $(SENDER) $(RECEIVER)

$(SENDER): sender.cpp packet.hpp
	$(CXX) $(CXXFLAGS) sender.cpp -o $(SENDER)

$(RECEIVER): receiver.cpp packet.hpp
	$(CXX) $(CXXFLAGS) receiver.cpp -o $(RECEIVER)

# Automation target using tmux
run: all
	@# Create a dummy input file if it doesn't exist for testing
	@test -f $(INPUT_FILE) || echo "This is a comprehensive test file for Mini-TCP implementation over UDP." > $(INPUT_FILE)
	
	@# Start a new tmux session in the background, named 'minitcp', and run the receiver on the left
	tmux new-session -d -s minitcp "./$(RECEIVER) $(PORT) $(OUTPUT_FILE); echo '--- Receiver finished ---'; exec bash"
	
	@# Wait a split second to ensure receiver socket is bound before sender starts
	sleep 0.5
	
	@# Split the window horizontally (creating a right pane) and run the sender
	tmux split-window -h -t minitcp "./$(SENDER) $(IP) $(PORT) $(INPUT_FILE); echo '--- Sender finished ---'; exec bash"
	
	@# Attach to the tmux session to bring it to the foreground
	tmux attach-session -t minitcp

# Clean target to remove binaries and output test files
clean:
	rm -f $(SENDER) $(RECEIVER) $(OUTPUT_FILE)
	@# Optionally kill the tmux session if it hangs
	-tmux kill-session -t minitcp 2>/dev/null || true