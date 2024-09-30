#include <iostream>
#include <string>
#include <regex>

// Function to validate IP address
bool isValidIPAddress(const std::string& ip);

// Function to validate port
bool isValidPort(const std::string& port);

// Validation function
bool validateInputReg(const std::string& ipAddress, const std::string& port, const std::string& appId, const std::string& returnId, std::string& minutiae, std::string& cause);

bool validateInputVal(const std::string& ipAddress, const std::string& port, const std::string& appId, std::string& cause);