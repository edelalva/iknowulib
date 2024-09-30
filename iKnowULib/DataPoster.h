#pragma once
#ifndef DATAPOSTER_H
#define DATAPOSTER_H

#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

class DataPoster {
public:
    // Constructor and Destructor
    DataPoster();
    ~DataPoster();

    // Method to post data from another class
    void postData(const std::unordered_map<std::string, std::string>& data);

    // Method to start a thread that waits for the data
    void startWaitingThread();

    // Join the thread when done
    void joinThread();

private:
    std::unordered_map<std::string, std::string> jsonMap_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool hasData_;
    std::thread waitingThread_;

    // Thread function that waits for data to be posted
    void waitForData();
};

#endif // DATAPOSTER_H
