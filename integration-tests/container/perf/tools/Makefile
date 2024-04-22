BIN_DIR = bin

all: $(BIN_DIR)/syscall-latency

clean:
	rm -rf $(BIN_DIR)

 $(BIN_DIR):
	mkdir -p $(BIN_DIR)

 $(BIN_DIR)/syscall-latency: $(BIN_DIR) syscall_latency.cpp
	g++ -o $(BIN_DIR)/syscall-latency syscall_latency.cpp

