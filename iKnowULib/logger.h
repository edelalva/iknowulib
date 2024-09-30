#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>
#include <string>

class Logger {
public:
    // Constructor with prefix
    Logger(const std::string& prefix);

    // Stream insertion operator
    template<typename T>
    Logger& operator<<(const T& message);

    // Destructor
    ~Logger();

    // Static method to enable/disable logging
    static void setLoggingEnabled(bool enabled);

private:
    std::string prefix_;
    std::ostringstream stream_;
    static bool loggingEnabled; // Flag to control logging
};

// Macros for convenience
#define LOG_INFO Logger("[INFO] ")
#define LOG_ERROR Logger("[ERROR] ")
#define LOG_SYSERR Logger("[SYSERR] ")

#endif // LOGGER_H
#pragma once
