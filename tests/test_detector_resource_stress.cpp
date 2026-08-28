#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <gd.h>

#include "src/FeatureDetector.h"

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#endif


bool verbose = false;


static unsigned long long residentBytes()
{
#if defined(__APPLE__)
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return 0;
    return info.resident_size;
#elif defined(__linux__)
    std::ifstream status("/proc/self/statm");
    unsigned long long totalPages = 0;
    unsigned long long residentPages = 0;
    status >> totalPages >> residentPages;
    return residentPages * static_cast<unsigned long long>(sysconf(_SC_PAGESIZE));
#else
    return 0;
#endif
}


static gdImagePtr loadPng(const char *path)
{
    FILE *input = fopen(path, "rb");
    if (input == nullptr)
        return nullptr;
    gdImagePtr image = gdImageCreateFromPng(input);
    fclose(input);
    return image;
}


int main(int argc, char **argv)
{
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " image.png iterations resident-growth-limit-bytes" << std::endl;
        return 2;
    }
    int iterations = std::atoi(argv[2]);
    unsigned long long limit = std::strtoull(argv[3], nullptr, 10);
    gdImagePtr image = loadPng(argv[1]);
    if (image == nullptr) {
        std::cerr << "FAIL: could not load stress image " << argv[1] << std::endl;
        return 2;
    }

    unsigned long long initialResident = residentBytes();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        FeatureProcessor processor(image, gdImageSX(image), gdImageSY(image));
        processor.processImage();
        int accepted = processor.findQyoo();
        if (accepted <= 0) {
            std::cerr << "FAIL: known-answer Qyoo rejected at iteration " << iteration << std::endl;
            gdImageDestroy(image);
            return 1;
        }
        processor.findDots(image);
    }
    unsigned long long finalResident = residentBytes();
    gdImageDestroy(image);
    unsigned long long growth = finalResident > initialResident ? finalResident - initialResident : 0;
    std::cout << "iterations=" << iterations << " initial_resident_bytes=" << initialResident
              << " final_resident_bytes=" << finalResident << " growth_bytes=" << growth
              << " limit_bytes=" << limit << std::endl;
    if (initialResident != 0 && finalResident != 0 && growth > limit) {
        std::cerr << "FAIL: detector resident growth exceeded the bounded-resource limit" << std::endl;
        return 1;
    }
    std::cout << "PASS: repeated accepted-Qyoo processing remains within the resource bound" << std::endl;
    return 0;
}
