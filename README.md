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

All project files are located in the src/ folder. This version removes all dependencies on Objective-C or UIKit, making it compatible for C++-based server environments.

## Installation and Setup

### Dependencies

This version requires the following libraries:
- **GD**: For image manipulation (reading, saving, flipping).

On a typical Linux system, you can install these libraries via your package manager:

```bash
# For GD
sudo apt-get install libgd-dev
```

### Building the Project

To compile the project, navigate to the root of the repository and use the existing `Makefile` or another build system of your choice (e.g., `cmake`).

```bash
make
```

This will compile the project and output the qyoo_detector binary to the bin/ folder.

### Typical Usage

1. Copy a Qyoo image (such as one created with the [QyooGenerate-C](https://github.com/qyoocode/QyooGenerate-C) project) into the `/input` folder.
2. From the root of the project, run the following commands:
   - If you haven't already compiled the project, run `make`.
   - Then, execute the following command:

```bash
bin/qyoo_detector input/45427039637.png
```

This should output something like:

```
Binary = 101010010011101010011001110110010101
Qyoo value = 45427039637
```

After running, an image file will be saved to the `/output` folder with the name matching the detected decimal Qyoo value (if a Qyoo was detected). It will draw green circles around detected dots and red Xs where a dot was not detected.

### Verbose Mode

You can add the `--v` flag to enable verbose logging. This will display debugging information such as feature detection progress and pixel data.

```bash
bin/qyoo_detector input/45427039637.png --v
```

Verbose mode output example:

```
Debug: Loaded image with size: 512x512
Debug: Starting Qyoo detection...
Debug: Starting new feature at (246, 127), feature ID: 1
Debug: First pass completed for feature ID: 1 with points: 798
Debug: Second pass completed for feature ID: 1 with total points: 798
...
Binary = 101010010011101010011001110110010101
Qyoo value = 45427039637
Debug: Feature processing completed successfully.
```

## Legacy Server-Side Usage

This project, in its original form, was used for server-side image processing on Linux environments. The command-line only version preserves that legacy, removing all dependencies on Objective-C or UIKit, making it fully compatible with C++.

It is capable of handling tasks like feature detection, contrast adjustment, and image manipulation using the GD library for image input/output and the CML library for matrix operations.

## Contributing

We welcome contributions to the QyooDetector project. Please open an issue or submit a pull request if you'd like to contribute.

## License

This project is licensed under the BSD-3-Clause License. See the [LICENSE](LICENSE) file for details.