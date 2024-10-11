// Logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>

// Global verbose flag declaration
extern bool verbose;

// Function for logging verbose output
inline void logVerbose(const std::string& message) {
    if (verbose) {
        std::cout << "Debug: " << message << std::endl;
    }
}

#endif // LOGGER_H
