# QyooDetector (Command-Line Version)

This is a **command-line only** version of the QyooDetector project, designed for server-side image processing. The package has been stripped of all non-command-line code, with a focus on C++ implementations of grayscale image processing and feature detection.

## Overview

This version of **QyooDetector** is optimized for running on Linux or other command-line environments where server-side image processing is needed. It leverages the **GD** and **CML** libraries to perform various operations such as image transformation, contrast adjustments, and feature detection on grayscale images.

### Key Features

- Grayscale image processing using raw 8-bit and 32-bit image data.
- Contrast enhancement for images.
- Image flipping, saving, and matrix-based transformations.
- C++ based, making it suitable for integration in server environments.
  
This version uses **GD** to handle image input and output (PNG format), and **CML** for matrix transformations.

### Structure

The project now contains all C++ files in the `cpp/` folder. All dependencies on UIKit and Objective-C have been removed.

## Installation and Setup

### Dependencies

This version requires the following libraries:
- **GD**: For image manipulation (reading, saving, flipping).
- **CML**: For matrix-based transformations used in image processing.

On a typical Linux system, you can install these libraries via your package manager:

```bash
sudo apt-get install libgd-dev
sudo apt-get install libcml-dev
```

### Building the Project

To compile the project, navigate to the root of the repository and use a **Makefile** or another build system of your choice (e.g., `cmake`).

If you are using a `Makefile`, you can set it up as follows:

```makefile
CC = g++
CFLAGS = -Wall -std=c++11
LDFLAGS = -lgd -lcml

SOURCES = cpp/RawImageGray8.cpp cpp/RawImageGray32.cpp cpp/main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = qyoo_detector

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
```

To compile the project:

```bash
make
```

### Running the Application

Once compiled, the application can be run from the command line. Example:

```bash
./qyoo_detector input_image.png output_image.png
```

This example runs the Qyoo image processing system, loading an input PNG image, performing operations such as grayscale conversion and contrast adjustment, and saving the result as a PNG.

## Usage

### Key Classes and Methods

#### `RawImageGray8`

This class handles 8-bit grayscale image data.

- **Initialization**:
  ```cpp
  RawImageGray8 image(100, 100);
  ```
  
- **Rendering from GD Image**:
  ```cpp
  image.copyFromGDImage(gdImagePtr image);
  ```

- **Image Contrast**:
  Adjusts the contrast of the image:
  ```cpp
  image.runContrast();
  ```

- **Saving the Image**:
  The processed image can be saved using GD library functions:
  ```cpp
  gdImagePtr outputImage = image.makeGDImage();
  gdSaveAsPng(outputImage, "output_image.png");
  ```

#### `RawImageGray32`

This class handles 32-bit grayscale image data.

- **Initialization**:
  ```cpp
  RawImageGray32 image(100, 100);
  ```

- **Creating GD Image**:
  ```cpp
  gdImagePtr outputImage = image.makeImage();
  ```

## Legacy Server-Side Usage

This project, in its original form, was used for server-side image processing on Linux environments. This command-line only version preserves that legacy, removing all dependencies on Objective-C or UIKit and making it fully compatible with C++.

It is capable of handling tasks like feature detection, contrast adjustment, and image manipulation using the **GD** library for image input/output and the **CML** library for matrix operations.

## Contributing

We welcome contributions to the QyooDetector project. Please open an issue or submit a pull request if you'd like to contribute.

## License

This project is licensed under the BSD-3-Clause License. See the `LICENSE` file for details.