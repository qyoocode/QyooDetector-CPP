#include <iostream>
#include <sstream>
#include <string>
#include <gd.h>
#include "DetectorCore.h"
#include "ImageLoader.h"

static std::string diagnosticJsonString(const std::string &value)
{
    std::ostringstream output;
    output << '"';
    for (unsigned char character : value)
    {
        if (character == '"' || character == '\\') output << '\\' << character;
        else if (character == '\n') output << "\\n";
        else if (character == '\r') output << "\\r";
        else if (character == '\t') output << "\\t";
        else output << character;
    }
    output << '"';
    return output.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <image_file> [--v|--verbose] "
                  << "[--normalization affine|projective|carrier-template|shadow] "
                  << "[--fallback-policy legacy-affine|reject|qualified] [--diagnostics] "
                  << "[--visual-debug-dir DIRECTORY] "
                  << "[--localization-policy production|native]" << std::endl;
        return 1;
    }

    // Check if verbose flag is set
    bool verbose = false;
    NormalizationMode normalization = NormalizationCarrierTemplate;
    ProjectiveFallbackPolicy fallbackPolicy = QualifiedAffineFallback;
    bool diagnostics = false;
    bool nativeLocalization = false;
    std::string visualDebugDirectory;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--v" || arg == "--verbose") {
            verbose = true;  // Enable verbose logging
        } else if (arg == "--diagnostics") {
            diagnostics = true;
        } else if (arg == "--visual-debug-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --visual-debug-dir requires an existing directory." << std::endl;
                return 1;
            }
            visualDebugDirectory = argv[++i];
            diagnostics = true;
        } else if (arg == "--normalization") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --normalization requires affine, projective, carrier-template, or shadow." << std::endl;
                return 1;
            }
            std::string value = argv[++i];
            if (value == "affine") normalization = NormalizationAffine;
            else if (value == "projective") normalization = NormalizationProjective;
            else if (value == "carrier-template") normalization = NormalizationCarrierTemplate;
            else if (value == "shadow") normalization = NormalizationShadow;
            else {
                std::cerr << "Error: unknown normalization mode: " << value << std::endl;
                return 1;
            }
        } else if (arg == "--fallback-policy") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --fallback-policy requires legacy-affine, reject, or qualified." << std::endl;
                return 1;
            }
            std::string value = argv[++i];
            if (value == "legacy-affine") fallbackPolicy = LegacyAffineFallback;
            else if (value == "reject") fallbackPolicy = RejectUnsupportedProjective;
            else if (value == "qualified") fallbackPolicy = QualifiedAffineFallback;
            else {
                std::cerr << "Error: unknown fallback policy: " << value << std::endl;
                return 1;
            }
        } else if (arg == "--localization-policy") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --localization-policy requires production or native."
                          << std::endl;
                return 1;
            }
            std::string value = argv[++i];
            if (value == "production") nativeLocalization = false;
            else if (value == "native") nativeLocalization = true;
            else {
                std::cerr << "Error: unknown localization policy: " << value << std::endl;
                return 1;
            }
        }
    }

    std::string image_file = argv[1]; // The first argument should be the image file

    // Load the image (using gdImagePtr)
    gdImagePtr theImage = loadImage(image_file);
    if (!theImage) {
        if (diagnostics) {
            std::cout << "Diagnostics JSON = {\"schema\":\"org.qyoo.detector.rejection-diagnostics\","
                      << "\"schema_version\":1,\"image_id\":" << diagnosticJsonString(image_file)
                      << ",\"image_loaded\":false,\"failure_stage\":\"image_load_preparation\"}"
                      << std::endl;
        }
        return 1; // Exit if image loading fails
    }

    // Convert palette-based image to true color if necessary
    if (!gdImageTrueColor(theImage)) {
        gdImagePtr trueColorImg = gdImageCreateTrueColor(gdImageSX(theImage), gdImageSY(theImage));
        if (!trueColorImg) {
            std::cerr << "Error: Unable to create true color image." << std::endl;
            gdImageDestroy(theImage);
            return 1;
        }

        // Copy the palette-based image into the true color image
        gdImageCopy(trueColorImg, theImage, 0, 0, 0, 0, gdImageSX(theImage), gdImageSY(theImage));
        gdImageDestroy(theImage);  // Clean up the original palette-based image
        theImage = trueColorImg;   // Replace with the true-color image
    }

    DetectorCoreOptions options;
    options.normalization = normalization;
    options.fallbackPolicy = fallbackPolicy;
    options.nativeLocalization = nativeLocalization;
    options.diagnostics = diagnostics;
    options.verbose = verbose;
    options.emitLegacyResults = true;
    options.writeOutputImages = true;
    options.imageId = image_file;
    options.visualDebugDirectory = visualDebugDirectory;
    DetectorCoreResult result = runDetectorCore(theImage, options);
    if (result.status != DetectorCoreSuccess)
    {
        std::cerr << "No Qyoo found in the image." << std::endl;
        gdImageDestroy(theImage);
        return 1;
    }

    if ((result.multiscaleRun || normalization == NormalizationCarrierTemplate) &&
        !result.candidates.empty()) {
        for (const LocalizationCandidate &candidate : result.candidates) {
            std::cout << "Binary = " << candidate.payload << std::endl;
            std::cout << "Qyoo value = " << std::stoull(candidate.payload, nullptr, 2)
                      << std::endl;
        }
    }
    if (result.candidates.empty())
        std::cerr << "No Qyoo found in the image." << std::endl;
    else if (verbose)
        std::cout << "Debug: Feature processing completed successfully." << std::endl;

    if (diagnostics)
    {
        if (!visualDebugDirectory.empty() &&
            !result.visualDebugArtifactsWritten)
            std::cerr << "Error: one or more visual debug artifacts could not be written." << std::endl;
        std::cout << "Diagnostics JSON = " << result.diagnosticsJson << std::endl;
    }

    // Clean up
    gdImageDestroy(theImage); // Destroy the image to avoid memory leaks

    return 0;
}
