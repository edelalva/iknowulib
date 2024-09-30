#include "DataPoster.h"
#include <iostream>

// Constructor
DataPoster::DataPoster() : hasData_(false) {}

// Destructor
DataPoster::~DataPoster() {
    if (waitingThread_.joinable()) {
        waitingThread_.join(); // Ensure the thread is joined before destruction
    }
}

// Method to post data from another class
void DataPoster::postData(const std::unordered_map<std::string, std::string>& data) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jsonMap_ = data; // Update shared data
        hasData_ = true; // Signal that data has been posted
    }
    cv_.notify_one(); // Notify the waiting thread
}

// Method to start a thread that waits for the data
void DataPoster::startWaitingThread() {
    waitingThread_ = std::thread(&DataPoster::waitForData, this);
}

// Join the thread when done
void DataPoster::joinThread() {
    if (waitingThread_.joinable()) {
        waitingThread_.join();
    }
}

// Thread function that waits for data to be posted
void DataPoster::waitForData() {
    std::unique_lock<std::mutex> lock(mutex_);
    // Wait until data is posted
    cv_.wait(lock, [this] { return hasData_; });

    // Data has been posted, now process it
    std::cout << "Data received in thread: \n";
    for (const auto& pair : jsonMap_) {
        std::cout << pair.first << ": " << pair.second << std::endl;
    }

    // Reset data flag
    hasData_ = false;
}
