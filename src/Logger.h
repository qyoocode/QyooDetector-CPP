// Logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>

class DetectorLogger
{
public:
    explicit DetectorLogger(bool enabled = false, std::ostream &output = std::cout)
        : enabled_(enabled), output_(&output) { }

    void log(const std::string &message) const
    {
        if (enabled_ && output_)
            *output_ << "Debug: " << message << std::endl;
    }

private:
    bool enabled_;
    std::ostream *output_;
};

inline void logVerbose(const DetectorLogger *logger, const std::string &message)
{
    if (logger)
        logger->log(message);
}

#endif // LOGGER_H
