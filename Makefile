# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INPUT_DIR = input
OUTPUT_DIR = output
SCRIPTS_DIR = scripts

# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11 -I/opt/homebrew/include -I. # Compiler flags (for header files)
LDFLAGS = -L/opt/homebrew/lib -lgd  # Linker flags (for libraries)

# Files
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/qyoo_detector

# Target to build everything
all: $(EXECUTABLE)

# Create bin and obj directories if they don't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)
	mkdir -p $(INPUT_DIR)
	mkdir -p $(OUTPUT_DIR)

# Rule to build the executable
$(EXECUTABLE): $(OBJ_FILES) | $(BIN_DIR)
	$(CXX) $(OBJ_FILES) -o $@ $(LDFLAGS)

# Rule to compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -rf $(OBJ_DIR)/*.o $(EXECUTABLE)