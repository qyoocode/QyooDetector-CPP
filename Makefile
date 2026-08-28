# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INPUT_DIR = input
OUTPUT_DIR = output
SCRIPTS_DIR = scripts
TEST_DIR = tests
TEST_BIN_DIR = $(BIN_DIR)/tests

# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11 -I/opt/homebrew/include -I. # Compiler flags (for header files)
LDFLAGS = -L/opt/homebrew/lib -lgd  # Linker flags (for libraries)

# Files
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/qyoo_detector
CORE_OBJ_FILES = $(filter-out $(OBJ_DIR)/main.o,$(OBJ_FILES))
ASPECT_TEST = $(TEST_BIN_DIR)/test_feature_aspect_ratio
CONTRAST_TEST = $(TEST_BIN_DIR)/test_raw_image_contrast
SENTINEL_TEST = $(TEST_BIN_DIR)/test_background_average

# Target to build everything
all: $(EXECUTABLE)

# Create bin and obj directories if they don't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)
	mkdir -p $(INPUT_DIR)
	mkdir -p $(OUTPUT_DIR)

$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# Rule to build the executable
$(EXECUTABLE): $(OBJ_FILES) | $(BIN_DIR)
	$(CXX) $(OBJ_FILES) -o $@ $(LDFLAGS)

# Rule to compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ASPECT_TEST): $(TEST_DIR)/test_feature_aspect_ratio.cpp $(CORE_OBJ_FILES) | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJ_FILES) -o $@ $(LDFLAGS)

$(CONTRAST_TEST): $(TEST_DIR)/test_raw_image_contrast.cpp $(OBJ_DIR)/RawImage.o | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/RawImage.o -o $@ $(LDFLAGS)

$(SENTINEL_TEST): $(TEST_DIR)/test_background_average.cpp $(SRC_DIR)/FeatureDetector.cpp $(filter-out $(OBJ_DIR)/FeatureDetector.o $(OBJ_DIR)/main.o,$(OBJ_FILES)) | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(filter-out $(OBJ_DIR)/FeatureDetector.o $(OBJ_DIR)/main.o,$(OBJ_FILES)) -o $@ $(LDFLAGS)

.PHONY: test-aspect
test-aspect: $(ASPECT_TEST)
	$(ASPECT_TEST)

.PHONY: test-contrast
test-contrast: $(CONTRAST_TEST)
	$(CONTRAST_TEST)

.PHONY: test-background-average
test-background-average: $(SENTINEL_TEST)
	$(SENTINEL_TEST)

# Clean up generated files
clean:
	rm -rf $(OBJ_DIR)/*.o $(EXECUTABLE)
