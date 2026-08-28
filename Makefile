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
HEADER_FILES = $(wildcard $(SRC_DIR)/*.h)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/qyoo_detector
CORE_OBJ_FILES = $(filter-out $(OBJ_DIR)/main.o,$(OBJ_FILES))
ASPECT_TEST = $(TEST_BIN_DIR)/test_feature_aspect_ratio
SCALE_GATE_TEST = $(TEST_BIN_DIR)/test_feature_scale_gate
CONTRAST_TEST = $(TEST_BIN_DIR)/test_raw_image_contrast
SENTINEL_TEST = $(TEST_BIN_DIR)/test_background_average
RESOURCE_TEST = $(TEST_BIN_DIR)/test_detector_resource_stress
IMAGE_INPUT_TEST = $(TEST_BIN_DIR)/test_image_input
PROJECTIVE_TRANSFORM_TEST = $(TEST_BIN_DIR)/test_projective_transform
PROJECTIVE_ESTIMATE_TEST = $(TEST_BIN_DIR)/test_feature_projective_estimate
PROJECTIVE_SAMPLING_TEST = $(TEST_BIN_DIR)/test_projective_sampling
RESOURCE_STRESS_IMAGE ?= ../recovery/task-02/corpus/images/expanded_raw_zero.png
RESOURCE_STRESS_ITERATIONS ?= 50
RESOURCE_STRESS_LIMIT_BYTES ?= 67108864
IMAGE_INPUT_PNG ?= ../recovery/task-02/corpus/images/expanded_raw_zero.png
IMAGE_INPUT_JPG ?= ../recovery/task-03/focused-tests/image-input/expanded_raw_zero.jpg
IMAGE_INPUT_JPEG ?= ../recovery/task-03/focused-tests/image-input/expanded_raw_zero.jpeg
PROJECTIVE_MANIFEST ?= ../recovery/task-02/corpus/corpus_manifest.json
PROJECTIVE_CONTROL_RESULTS ?= ../recovery/task-04/raw-evidence/00-control/task01_frozen_55/per_case_results.jsonl
PERSPECTIVE_SWEEP_MANIFEST ?= ../recovery/task-04/perspective-sweep/manifest.json
DIAGNOSTICS_ACCEPTED_IMAGE ?= ../recovery/task-02/corpus/images/expanded_raw_zero.png
DIAGNOSTICS_REJECTED_IMAGE ?= ../recovery/task-01/artifacts/detector-baseline/images/rotation_135deg.png
VISUAL_DIAGNOSTICS_TRASH ?= ../recovery/trash/detector-visuals/test-visual-diagnostics
CORNER_PAIR_IMAGE ?= ../recovery/task-01/artifacts/detector-baseline/images/blur_4sigma.png
CORNER_PAIR_EXPECTED_BITS ?= 101111101010011110111010111001101100
CORNER_PAIR_NEGATIVE ?= ../recovery/task-05/negative-corpus/images/negative_development_circle_square_20.png

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
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADER_FILES) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ASPECT_TEST): $(TEST_DIR)/test_feature_aspect_ratio.cpp $(CORE_OBJ_FILES) | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJ_FILES) -o $@ $(LDFLAGS)

$(SCALE_GATE_TEST): $(TEST_DIR)/test_feature_scale_gate.cpp $(CORE_OBJ_FILES) | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJ_FILES) -o $@ $(LDFLAGS)

$(CONTRAST_TEST): $(TEST_DIR)/test_raw_image_contrast.cpp $(OBJ_DIR)/RawImage.o $(OBJ_DIR)/Geometry.o | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/RawImage.o $(OBJ_DIR)/Geometry.o -o $@ $(LDFLAGS)

$(SENTINEL_TEST): $(TEST_DIR)/test_background_average.cpp $(SRC_DIR)/FeatureDetector.cpp $(filter-out $(OBJ_DIR)/FeatureDetector.o $(OBJ_DIR)/main.o,$(OBJ_FILES)) | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(filter-out $(OBJ_DIR)/FeatureDetector.o $(OBJ_DIR)/main.o,$(OBJ_FILES)) -o $@ $(LDFLAGS)

$(RESOURCE_TEST): $(TEST_DIR)/test_detector_resource_stress.cpp $(CORE_OBJ_FILES) | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(CORE_OBJ_FILES) -o $@ $(LDFLAGS)

$(IMAGE_INPUT_TEST): $(TEST_DIR)/test_image_input.cpp $(OBJ_DIR)/ImageLoader.o | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/ImageLoader.o -o $@ $(LDFLAGS)

$(PROJECTIVE_TRANSFORM_TEST): $(TEST_DIR)/test_projective_transform.cpp $(OBJ_DIR)/Geometry.o | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/Geometry.o -o $@ $(LDFLAGS)

$(PROJECTIVE_ESTIMATE_TEST): $(TEST_DIR)/test_feature_projective_estimate.cpp $(OBJ_DIR)/Feature.o $(OBJ_DIR)/Geometry.o $(OBJ_DIR)/RawImage.o | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/Feature.o $(OBJ_DIR)/Geometry.o $(OBJ_DIR)/RawImage.o -o $@ $(LDFLAGS)

$(PROJECTIVE_SAMPLING_TEST): $(TEST_DIR)/test_projective_sampling.cpp $(OBJ_DIR)/Geometry.o $(OBJ_DIR)/RawImage.o | $(TEST_BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJ_DIR)/Geometry.o $(OBJ_DIR)/RawImage.o -o $@ $(LDFLAGS)

.PHONY: test-aspect
test-aspect: $(ASPECT_TEST)
	$(ASPECT_TEST)

.PHONY: test-scale-gate
test-scale-gate: $(SCALE_GATE_TEST)
	$(SCALE_GATE_TEST)

.PHONY: test-contrast
test-contrast: $(CONTRAST_TEST)
	$(CONTRAST_TEST)

.PHONY: test-background-average
test-background-average: $(SENTINEL_TEST)
	$(SENTINEL_TEST)

.PHONY: test-resource-stress
test-resource-stress: $(RESOURCE_TEST)
	$(RESOURCE_TEST) $(RESOURCE_STRESS_IMAGE) $(RESOURCE_STRESS_ITERATIONS) $(RESOURCE_STRESS_LIMIT_BYTES)

.PHONY: test-image-input
test-image-input: $(IMAGE_INPUT_TEST) $(EXECUTABLE)
	$(IMAGE_INPUT_TEST) $(IMAGE_INPUT_PNG) $(IMAGE_INPUT_JPG) $(IMAGE_INPUT_JPEG)
	python3 $(TEST_DIR)/test_cli_image_input.py $(EXECUTABLE) $(IMAGE_INPUT_PNG) $(IMAGE_INPUT_JPG) $(IMAGE_INPUT_JPEG)

.PHONY: test-projective-transform
test-projective-transform: $(PROJECTIVE_TRANSFORM_TEST)
	$(PROJECTIVE_TRANSFORM_TEST)

.PHONY: test-projective-estimate
test-projective-estimate: $(PROJECTIVE_ESTIMATE_TEST)
	$(PROJECTIVE_ESTIMATE_TEST)

.PHONY: test-projective-sampling
test-projective-sampling: $(PROJECTIVE_SAMPLING_TEST)
	$(PROJECTIVE_SAMPLING_TEST)

.PHONY: test-projective-cli
test-projective-cli: $(EXECUTABLE)
	python3 $(TEST_DIR)/test_projective_cli.py $(EXECUTABLE) $(PROJECTIVE_MANIFEST) $(PROJECTIVE_CONTROL_RESULTS)

.PHONY: test-diagnostics
test-diagnostics: $(EXECUTABLE)
	python3 $(TEST_DIR)/test_rejection_diagnostics.py $(EXECUTABLE) $(DIAGNOSTICS_ACCEPTED_IMAGE) $(DIAGNOSTICS_REJECTED_IMAGE)

.PHONY: test-visual-diagnostics
test-visual-diagnostics: $(EXECUTABLE)
	python3 $(TEST_DIR)/test_visual_diagnostics.py $(EXECUTABLE) $(DIAGNOSTICS_ACCEPTED_IMAGE) $(DIAGNOSTICS_REJECTED_IMAGE) $(VISUAL_DIAGNOSTICS_TRASH)

.PHONY: test-fallback-policy
test-fallback-policy: $(EXECUTABLE)
	python3 $(TEST_DIR)/test_fallback_policy.py $(EXECUTABLE) $(PROJECTIVE_MANIFEST) $(PERSPECTIVE_SWEEP_MANIFEST)

.PHONY: test-corner-edge-pair
test-corner-edge-pair: $(EXECUTABLE)
	python3 $(TEST_DIR)/test_corner_edge_pair.py $(EXECUTABLE) $(CORNER_PAIR_IMAGE) $(CORNER_PAIR_EXPECTED_BITS) $(CORNER_PAIR_NEGATIVE)

# Clean up generated files
clean:
	rm -rf $(OBJ_DIR)/*.o $(EXECUTABLE)
