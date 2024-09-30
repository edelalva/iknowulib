#include <iostream>
#include <string>
#include <regex>
#include "iKnowUHelper.h"

// Function to validate IP address
bool isValidIPAddress(const std::string& ip) {
    // Define a regular expression for a valid IP address
    const std::regex ip_pattern(
        R"(^((25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)$)");
    return std::regex_match(ip, ip_pattern);
}

// Function to validate port
bool isValidPort(const std::string& port) {
    // Define a regular expression for a valid port number (1-65535)
    const std::regex port_pattern(R"(^(6553[0-5]|655[0-2]\d|65[0-4]\d{2}|6[0-4]\d{3}|[1-5]\d{4}|\d{1,4})$)");
    return std::regex_match(port, port_pattern);
}


bool isValidString(const std::string& input) {
    // Check if the string is empty
    if (input.empty()) {
        return false;
    }

    // Check if the string contains printable characters only
    for (const char& ch : input) {
        if (!std::isprint(static_cast<unsigned char>(ch))) {
            return false;
        }
    }

    // Additional validation rules can go here if needed

    return true;
}

// Validation function
bool validateInputReg(const std::string& ipAddress, const std::string& port, const std::string& appId, const std::string& returnId, std::string& minutiae, std::string& cause) {
    // Check if any string is empty
    if (ipAddress.empty()) {
        cause = "IP address is empty";
        return false;
    }
    if (port.empty()) {
        cause = "Port is empty";
        return false;
    }
    if (appId.empty()) {
        cause = "App ID is empty";
        return false;
    }
    if (returnId.empty()) {
        cause = "Return ID is empty";
        return false;
    }
    if (minutiae.empty()) {
        cause = "Scan finger not present. Please call startScan first.";
        return false;
    }

    if (!isValidString(returnId)) {
        cause = "Return ID contain invalid chracter";
        return false;
    }

    // Validate IP address format
    if (!isValidIPAddress(ipAddress)) {
        cause = "Invalid IP address format";
        return false;
    }

    // Validate port format
    if (!isValidPort(port)) {
        cause = "Invalid port format";
        return false;
    }

    // If all validations pass
    cause.clear();
    return true;
}

bool validateInputVal(const std::string& ipAddress, const std::string& port, const std::string& appId, std::string& cause) {
    // Check if any string is empty
    if (ipAddress.empty()) {
        cause = "IP address is empty";
        return false;
    }
    if (port.empty()) {
        cause = "Port is empty";
        return false;
    }
    if (appId.empty()) {
        cause = "App ID is empty";
        return false;
    }

    // Validate IP address format
    if (!isValidIPAddress(ipAddress)) {
        cause = "Invalid IP address format";
        return false;
    }

    // Validate port format
    if (!isValidPort(port)) {
        cause = "Invalid port format";
        return false;
    }

    // If all validations pass
    cause.clear();
    return true;
}