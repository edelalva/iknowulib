#include "Logger.h"

// Initialize static member
bool Logger::loggingEnabled = true; // Default to enabled

// Constructor
Logger::Logger(const std::string& prefix) : prefix_(prefix) {}

// Stream insertion operator
template<typename T>
Logger& Logger::operator<<(const T& message) {
    if (loggingEnabled) {
        stream_ << message;
    }
    return *this;
}

// Destructor
Logger::~Logger() {
    if (loggingEnabled) {
        std::cout << prefix_ << stream_.str() << std::endl;
    }
}

// Static method to enable/disable logging
void Logger::setLoggingEnabled(bool enabled) {
    loggingEnabled = enabled;
}

// Explicitly instantiate the template for commonly used types
template Logger& Logger::operator<<(const std::string&);
//template Logger& Logger::operator<<(const char*);
//template Logger& Logger::operator<<(int);
//template Logger& Logger::operator<<(double);
// Add other common types as needed
