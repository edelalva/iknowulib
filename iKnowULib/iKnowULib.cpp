#define _CRT_SECURE_NO_DEPRECATE
//#include <drogon/drogon.h>
#include <iostream>
#include <Windows.h>
#include <dpfpdd.h>
#include <future>
#include <mutex> // For std::mutex

#include <fstream>
#include <sgfplib.h>

#include <opencv2/opencv.hpp>

#include "iKnowULib.h"
#include "iKnowUHelper.h"

#include <json/json.h>
#include <curl/curl.h>

#include <filesystem>
#include "Logger.h"
#include "JsonParcer.h"



#define CLIENT_VERSION "v1.1"
#define SIMILARITY_THRESHOLD 50
#define SIMILARITY_THRESHOLD_VERSION_ZERO 10


std::promise<void> cancelPromise;

#define SGFPM_STATUS DWORD

std::mutex g_mutex; // Global mutex for synchronization



// Define log levels
enum LogLevel { LOG_LEVEL_INFO, LOG_LEVEL_ERROR, LOG_LEVEL_SYSERR };

// Global variable to set the minimum log level
LogLevel currentLogLevel = LOG_LEVEL_INFO; // Default to INFO level

// Logging method that accepts an ostringstream and appends std::endl automatically
void logMessage(LogLevel level, const std::string& logLevelStr, const std::string& message) {
    if (level >= currentLogLevel) {
        std::cout << logLevelStr << message << std::endl;
    }
}

// Helper function to wrap log message construction and send it to logMessage
struct LogHelper {
    LogLevel level;
    std::ostringstream oss;
    std::string prefix;

    LogHelper(LogLevel logLevel, const std::string& logPrefix)
        : level(logLevel), prefix(logPrefix) {}

    ~LogHelper() {
        logMessage(level, prefix, oss.str());
    }

    template <typename T>
    LogHelper& operator<<(const T& value) {
        oss << value;
        return *this;
    }
};

// Macros to use << for constructing the log message and sending it to logMessage
#define LOG_INFO LogHelper(LOG_LEVEL_INFO, "[INFO] ")
#define LOG_ERROR LogHelper(LOG_LEVEL_ERROR, "[ERROR] ")
#define LOG_SYSERR LogHelper(LOG_LEVEL_SYSERR, "[SYSERR] ")

// Function to set the log level
void setLogLevel(LogLevel level) {
    currentLogLevel = level;
}




using ClusterRegCallback = std::function<void(bool success, const std::string& message)>;
std::string mMessageRet = "";

std::string mMinutiae = "";

constexpr auto STAGE_IDLE = 0;
constexpr auto STAGE_CAPTURE = 1;
constexpr auto STAGE_SENDING  = 2;

std::int16_t mStage = STAGE_IDLE;



std::string createJsonResponse(const std::string& status, const std::string& cause) {
    // Create a Json::Value object
    Json::Value jsonResult;

    // Populate the Json::Value object with status, cause, and client version
    jsonResult["status"] = status;
    jsonResult["cause"] = cause;
    jsonResult["returnId"] = cause;
    jsonResult["client version"] = std::string(CLIENT_VERSION);

    return jsonResult.toStyledString();
}

bool createDirectoryIfNotExists(const std::string& directoryPath) {
    namespace fs = std::filesystem;

    // Check if the directory already exists
    if (fs::exists(directoryPath) && fs::is_directory(directoryPath)) {
        //std::cout << "Directory '" << directoryPath << "' already exists." << std::endl;
        return true;
    }

    // Attempt to create the directory
    if (fs::create_directory(directoryPath)) {
        LOG_INFO << "Directory '" << directoryPath << "' created successfully.";
        return true;
    }
    else {
        LOG_ERROR << "Failed to create directory '" << directoryPath << "'.";
        return false;
    }
}

// Function to update the log level in the JSON file
void updateLogLevelInFile(const std::string& filename, const std::string& newLogLevel) {
    try {
        // Read JSON from file
        std::ifstream inputFile(filename, std::ifstream::binary);
        if (!inputFile.is_open()) {
            throw std::runtime_error("Error opening file for reading: " + filename);
        }

        Json::Value root;
        inputFile >> root;
        inputFile.close();

        // Update the log level
        root["app"]["log"]["log_level"] = newLogLevel;

        // Write updated JSON back to the file
        std::ofstream outputFile(filename, std::ofstream::binary);
        if (!outputFile.is_open()) {
            throw std::runtime_error("Error opening file for writing: " + filename);
        }

        Json::StreamWriterBuilder writerBuilder;
        std::string jsonString = Json::writeString(writerBuilder, root);
        outputFile << jsonString;
        outputFile.close();

      //  std::cout << "Updated JSON has been saved to " << filename << std::endl;
    }
    catch (const std::runtime_error& e) {
        //std::cerr << "Runtime error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        //std::cerr << "Exception: " << e.what() << std::endl;
    }
}

void initLog(bool isEnable)
{
    Logger::setLoggingEnabled(isEnable);
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    if (!isEnable)
    {
        setLogLevel(LOG_LEVEL_SYSERR);
    }

    //trantor::Logger::setLogLevel(trantor::Logger::kFatal);
    //if (isEnable)
    //    updateLogLevelInFile("./config.json", "DEBUG");
    //else
    //    updateLogLevelInFile("./config.json", "SILENT");

    //std::string directoryPath = "./logs";
    //if (createDirectoryIfNotExists(directoryPath)) {
    //   
    //}
    //else {
    //    // Handle failure to create directory
    //}
    ////drogon::app().loadConfigFile("./config.json");
    //cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);
    //trantor::Logger::setLogLevel(trantor::Logger::kFatal);
}


unsigned int GetFirstDPI(DPFPDD_DEV dev)
{
    int DPI = 0;
    //get required size for the capabilities structure
    unsigned int size = sizeof(unsigned int);
    int result = dpfpdd_get_device_capabilities(dev, (DPFPDD_DEV_CAPS*)&size);
    if (result != DPFPDD_E_MORE_DATA)
    {
        //ErrorMsg(_T("error when calling dpfpdd_get_device_capabilities()"), result);//
        return DPI;
    }

    DPFPDD_DEV_CAPS* pCaps = (DPFPDD_DEV_CAPS*)new char[size];
    if (!pCaps)
    {
        return DPI;
    }
    pCaps->size = size;

    result = dpfpdd_get_device_capabilities(dev, pCaps);
    if (result != DPFPDD_SUCCESS)
    {
        // ErrorMsg(_T("error when calling dpfpdd_get_device_capabilities()"), result);
    }
    else
    {
        DPI = pCaps->resolutions[0];
    }
    delete pCaps;
    return DPI;
}


bool initializeSecuGenDevice(HSGFPM& hFpm, std::string& cause) {
    SGFPM_STATUS status = SGFPM_Create(&hFpm);
    if (status != SGFDX_ERROR_NONE) {
        cause = "Failed to create SecuGen device, error code: " + std::to_string(status);
        LOG_ERROR << cause;
        return false;
    }

    status = SGFPM_InitEx(hFpm, 300, 400, 500);
    if (status != SGFDX_ERROR_NONE) {
        cause = "Failed to initialize SecuGen device, error code: " + std::to_string(status);
        LOG_ERROR << cause;
        SGFPM_CloseDevice(hFpm);
        return false;
    }

    status = SGFPM_SetTemplateFormat(hFpm, TEMPLATE_FORMAT_ISO19794);
    if (status != SGFDX_ERROR_NONE) {
        cause = "Failed to set template format, error code: " + std::to_string(status);
        LOG_ERROR << cause;
        SGFPM_CloseDevice(hFpm);
        return false;
    }

    return true;
}


bool getTemplate(const cv::Mat& image, std::string& templateHex, std::string& cause) {
    HSGFPM hFpm;
    if (!initializeSecuGenDevice(hFpm, cause)) {
        return false;
    }
    DWORD max_size = 0;
    SGFPM_GetMaxTemplateSize(hFpm, &max_size);

    SGFingerInfo fp_info = { 0 };
    fp_info.FingerNumber = SG_FINGPOS_UK;
    fp_info.ViewNumber = 0;
    fp_info.ImpressionType = SG_IMPTYPE_LP;
    fp_info.ImageQuality = 0;

    // Convert OpenCV Mat to raw buffer
    std::vector<BYTE> raw_image;
    raw_image.assign(image.data, image.data + image.total() * image.elemSize());

    std::vector<BYTE> template_data(max_size);
    SGFPM_STATUS status = SGFPM_CreateTemplate(hFpm, &fp_info, raw_image.data(), template_data.data());
    if (status != SGFDX_ERROR_NONE) {
        SGFPM_Terminate(hFpm);
        cause = "Failed to create template, error code: " + std::to_string(status);
        return false;
    }

    DWORD template_size = 0;
    status = SGFPM_GetTemplateSize(hFpm, template_data.data(), &template_size);
    SGFPM_Terminate(hFpm);
    if (status != SGFDX_ERROR_NONE) {
        cause = "Failed to get template size, error code: " + std::to_string(status);
        return false;
    }
    LOG_INFO << "Template created successfully";

    // Convert template_data to hexadecimal string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < template_size; ++i) {
        ss << std::setw(2) << static_cast<int>(template_data[i]);
    }
    templateHex = ss.str();

    return true; // Successfully created and saved the template
}


void convertResizeAndSaveToPNG(cv::Mat& imageData,  const unsigned char* buffer, int width, int height, const std::string& outputFilename) {
    // Create cv::Mat from the buffer
    cv::Mat image(height, width, CV_8UC1, const_cast<unsigned char*>(buffer));

    // Save the image directly to a PNG file
    if (!cv::imwrite(outputFilename, image)) {
       // std::cerr << "Failed to save image to PNG file: " << outputFilename << std::endl;
        return;
    }
    else {
       // std::cout << "Image saved successfully as " << outputFilename << std::endl;
    }

    // Reload the saved image for resizing
    cv::Mat savedImage = cv::imread(outputFilename, cv::IMREAD_COLOR);
    if (savedImage.empty()) {
        std::cerr << "Failed to load saved PNG image for resizing" << std::endl;
        return;
    }

    // Resize the image to 300x400
    cv::resize(savedImage, imageData, cv::Size(300, 400));

}

void adjustContrastBrightness(cv::Mat& inputImage, cv::Mat& outputImage, double alpha, int beta) {
    // Apply contrast (alpha) and brightness (beta)
    inputImage.convertTo(outputImage, -1, alpha, beta);
}
void zoomImage(cv::Mat& inputImage, cv::Mat& outputImage, double zoomFactor) {
    cv::Size newSize(static_cast<int>(inputImage.cols * zoomFactor), static_cast<int>(inputImage.rows * zoomFactor));
    cv::Mat zoomedImage;
    cv::resize(inputImage, zoomedImage, newSize, 0, 0, cv::INTER_LINEAR);
    int cropX = (zoomedImage.cols - inputImage.cols) / 2;
    int cropY = (zoomedImage.rows - inputImage.rows) / 2;
    cv::Rect cropRegion(cropX, cropY, inputImage.cols, inputImage.rows);
    zoomedImage(cropRegion).copyTo(outputImage);
}

void resizeFrame(cv::Mat& inputFrame, cv::Mat& outputFrame, cv::Size newSize) {
    cv::resize(inputFrame, outputFrame, newSize, 0, 0, cv::INTER_LINEAR);
}

void adaptiveHistogramEqualization(const cv::Mat& inputImage, cv::Mat& outputImage, double clipLimit = 2.0, int tileGridSize = 8) {
    // Convert the image to grayscale if it is not already
    cv::Mat grayscaleImage;
    if (inputImage.channels() == 3) {
        cv::cvtColor(inputImage, grayscaleImage, cv::COLOR_BGR2GRAY);
    }
    else {
        grayscaleImage = inputImage;
    }

    // Create a CLAHE object (Contrast Limited Adaptive Histogram Equalization)
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(clipLimit); // Set the clip limit to prevent noise amplification
    clahe->setTilesGridSize(cv::Size(tileGridSize, tileGridSize)); // Set the tile grid size

    // Apply CLAHE to the grayscale image
    clahe->apply(grayscaleImage, outputImage);
}


void binarizeImage(const cv::Mat& inputImage, cv::Mat& outputImage, double thresholdValue = 128) {
    // Convert image to grayscale if it's not already
    if (inputImage.channels() == 3) {
        cv::cvtColor(inputImage, inputImage, cv::COLOR_BGR2GRAY);
    }

    // Apply binary thresholding
    cv::threshold(inputImage, outputImage, thresholdValue, 255, cv::THRESH_BINARY);
}

void applyGaussianBlur(const cv::Mat& inputImage, cv::Mat& outputImage, int kernelSize, double sigmaX, double sigmaY = 0) {
    // Ensure kernel size is odd and greater than 0
    if (kernelSize % 2 == 0 || kernelSize <= 0) {
        std::cerr << "Kernel size must be a positive odd number." << std::endl;
        return;
    }

    // Apply Gaussian Blur
    cv::GaussianBlur(inputImage, outputImage, cv::Size(kernelSize, kernelSize), sigmaX, sigmaY);
}

cv::Mat smoothFingerprintEdges(const cv::Mat& fingerprint) {
    if (fingerprint.empty()) {
        throw std::invalid_argument("Invalid input image");
    }

    // Convert the fingerprint image to grayscale if it's not already
    cv::Mat gray;
    if (fingerprint.channels() == 3) {
        cv::cvtColor(fingerprint, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = fingerprint.clone();
    }

    // Apply Gaussian blur to smooth the image
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 1.5);

    // Apply threshold to obtain binary image (you can adjust the threshold value)
    cv::Mat binary;
    cv::threshold(blurred, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);

    // Perform morphological closing to smooth the lines
    cv::Mat morph;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(1, 1));
    cv::morphologyEx(binary, morph, cv::MORPH_DILATE, kernel);

    // Apply another Gaussian blur to further smooth edges
    cv::Mat smoothEdges;
    cv::GaussianBlur(morph, smoothEdges, cv::Size(1, 1), 1.0);

    return smoothEdges;
}


cv::Mat smoothFingerprintEdgesMe(const cv::Mat& fingerprint) {
    if (fingerprint.empty()) {
        throw std::invalid_argument("Invalid input image");
    }

    // Convert the fingerprint image to grayscale if it's not already
    cv::Mat gray;
    if (fingerprint.channels() == 3) {
        cv::cvtColor(fingerprint, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = fingerprint.clone();
    }

    // Apply Gaussian blur to smooth the image
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 1.5);

    // Apply threshold to obtain binary image (you can adjust the threshold value)
    cv::Mat binary;
    cv::threshold(blurred, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);

    // Perform morphological closing to smooth the lines
    cv::Mat morph;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(1, 1));
    cv::morphologyEx(binary, morph, cv::MORPH_ERODE, kernel);

    // Apply another Gaussian blur to further smooth edges
    cv::Mat smoothEdges;
    cv::GaussianBlur(morph, smoothEdges, cv::Size(1, 1), 1.0);

    return smoothEdges;
}

cv::Mat convertAndResizeImage(const unsigned char* buffer, int width, int height) {
    // Create cv::Mat from the buffer
    cv::Mat image(height, width, CV_8UC1, const_cast<unsigned char*>(buffer));

    cv::imwrite("before.png", image);

    // do the finger preparation

    cv::Mat zoomedImage;
    double zoomFactor = 1.2; // Example zoom factor
    zoomImage(image, zoomedImage, zoomFactor);

    // Resize the frame
    cv::Mat resizedFrame;
    cv::Size newSize(300, 400); // Example new size for the frame
    resizeFrame(zoomedImage, resizedFrame, newSize);

    cv::Mat equalizedImage;
    adaptiveHistogramEqualization(resizedFrame, equalizedImage, 2.0, 8);

    cv::Mat GaussianBlurImage;
    // Apply Gaussian Blur
    int kernelSize = 3;  // Size of the Gaussian kernel (e.g., 3, 5, 7, 9, etc.)
    double sigmaX = 1; // Standard deviation in the X direction
    applyGaussianBlur(equalizedImage, GaussianBlurImage, kernelSize, sigmaX);

    cv::Mat binarizedGImage;
    binarizeImage(GaussianBlurImage, binarizedGImage);

    cv::Mat smoothedFingerprint = smoothFingerprintEdgesMe(equalizedImage);

    cv::imwrite("after.png", smoothedFingerprint);

    return binarizedGImage;
}




// Function to capture fingerprint image and return as cv::Mat
bool captureFingerprintImage(cv::Mat& imageData, std::string& cause, std::promise<void>& cancelPromise) {
    DPFPDD_DEV hReader = nullptr;
    DPFPDD_DEV_INFO* devInfos = nullptr;
    unsigned int devCount = 0;
    DWORD result = DPFPDD_SUCCESS;

    LOG_INFO << "Capture starting";
    // Initialize DPFPDD library
    result = dpfpdd_init();
    if (result != DPFPDD_SUCCESS) {
        cause = "Failed to initialize DPFPDD library";
        LOG_ERROR << cause;
        return false;
    }

    // Discover devices
    result = dpfpdd_query_devices(&devCount, NULL);
    if ((result != DPFPDD_SUCCESS && result != DPFPDD_E_MORE_DATA) || devCount == 0) {
        cause = "No fingerprint readers found.";
       // LOG_ERROR << cause;
        dpfpdd_exit();
        return false;
    }

    devInfos = new DPFPDD_DEV_INFO[devCount];
    result = dpfpdd_query_devices(&devCount, devInfos);
    if (result != DPFPDD_SUCCESS) {
        cause = "Failed to query device info";
        LOG_ERROR << cause;
        dpfpdd_exit();
        delete[] devInfos;
        return false;
    }

    // Open the first device
    result = dpfpdd_open(devInfos[0].name, &hReader);
    delete[] devInfos;
    if (result != DPFPDD_SUCCESS) {
        cause = "Failed to open device";
        LOG_ERROR << cause;
        dpfpdd_exit();
        return false;
    }

    // Set capture parameters
    DPFPDD_CAPTURE_PARAM captureParams = { 0 };
    captureParams.size = sizeof(captureParams);
    captureParams.image_fmt = DPFPDD_IMG_FMT_PIXEL_BUFFER;
    captureParams.image_proc = DPFPDD_IMG_PROC_DEFAULT;
    captureParams.image_res = 500;

    int dpi = GetFirstDPI(hReader);
    if (dpi != 0)
        captureParams.image_res = dpi;

    // Capture fingerprint image
    DPFPDD_CAPTURE_RESULT captureResult;
    std::memset(&captureResult, 0, sizeof(captureResult));
    captureResult.size = sizeof(captureResult);

    unsigned int bufferSize = 400 * 400;
    unsigned char* buffer = new unsigned char[bufferSize];
    std::memset(buffer, 0, bufferSize);

    unsigned int actualSize = bufferSize; // Use bufferSize to store the actual size of the captured image

    result = dpfpdd_capture(hReader, &captureParams, 30000, &captureResult, &actualSize, buffer);

    // Cleanup
    dpfpdd_close(hReader);
    dpfpdd_exit();

    if (result == DPFPDD_SUCCESS && captureResult.success) {
        LOG_INFO << "Capture successful!: " << captureResult.info.height << ":" << captureResult.info.width;
        LOG_INFO << "Capture actual size!: " << actualSize;
 
        imageData = convertAndResizeImage(buffer, captureResult.info.width, captureResult.info.height);

        cv::imwrite("output.png", imageData); 

        LOG_INFO << "Capture CV size!: " << imageData.total() * imageData.elemSize();

        //bufferSize = actualSize; // Update bufferSize with the actual captured size
        delete[] buffer;
        return true;
    }
    else {
        cause = " value: " + std::to_string(result);
     //   LOG_ERROR << cause;
        delete[] buffer;
        return false;
    }
}




// Function to cancel fingerprint capture
void cancelFingerprintCapture(std::promise<void>& cancelPromise) {
    cancelPromise.set_value();
}


void printHexString(const unsigned char* buffer, unsigned int bufferSize) {
    if (buffer == nullptr) {
        //std::cerr << "Buffer is null." << std::endl;
        return;
    }

    // Ensure we only print the first 100 bytes or the buffer size, whichever is smaller
    unsigned int bytesToPrint = std::min(bufferSize, 100U);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < bytesToPrint; ++i) {
        ss << std::setw(2) << static_cast<int>(buffer[i]) << " ";
    }

    //std::cout << "Hex string of the first " << bytesToPrint << " bytes: " << ss.str() << std::endl;
}


std::string convertTemplateToHex(const std::vector<BYTE>& templateData, int size) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        ss << std::setw(2) << static_cast<int>(templateData[i]);
    }
    return ss.str();
}

bool isSimilarityValid(const std::string& similarityStr, const std::string& versionStr) {

    try {
        int similarity = std::stoi(similarityStr);
        int threshold = (versionStr == "0") ? 10 : 45;

        if (similarity >= threshold) {
            return true;
        }
        else {
            return false;
        }
    }
    catch (const std::invalid_argument& e) {
        LOG_ERROR << "Invalid argument: " << e.what();
        return false;
    }
    catch (const std::out_of_range& e) {
        LOG_ERROR << "Out of range: " << e.what();
        return false;
    }
}


bool captureAndCreateTemplate(int numCaptures, std::string& templatesHex, std::string& cause, std::promise<void>& cancelPromise) {
    DPFPDD_DEV hReader = nullptr;
    DPFPDD_DEV_INFO* devInfos = nullptr;
    unsigned int devCount = 0;
    DWORD result = DPFPDD_SUCCESS;
    SGFPM_STATUS sgStatus;

    LOG_INFO << "Capture starting";
    // Initialize DPFPDD library
    result = dpfpdd_init();
    if (result != DPFPDD_SUCCESS) {
        cause = "Failed to initialize DPFPDD library";
        LOG_ERROR << cause;
        return false;
    }

    // Discover devices
    result = dpfpdd_query_devices(&devCount, NULL);
    if ((result != DPFPDD_SUCCESS && result != DPFPDD_E_MORE_DATA) || devCount == 0) {
        cause = "No fingerprint readers found.";
        LOG_ERROR << cause;
        dpfpdd_exit();
        return false;
    }

    devInfos = new DPFPDD_DEV_INFO[devCount];
    result = dpfpdd_query_devices(&devCount, devInfos);
    if (result != DPFPDD_SUCCESS) {
        cause = "Failed to query device info";
        LOG_ERROR << cause;
        dpfpdd_exit();
        delete[] devInfos;
        return false;
    }

    // Open the first device
    result = dpfpdd_open(devInfos[0].name, &hReader);
    delete[] devInfos;
    if (result != DPFPDD_SUCCESS) {
        cause = "Failed to open device";
        LOG_ERROR << cause;
        dpfpdd_exit();
        return false;
    }

    // Set capture parameters
    DPFPDD_CAPTURE_PARAM captureParams = { 0 };
    captureParams.size = sizeof(captureParams);
    captureParams.image_fmt = DPFPDD_IMG_FMT_PIXEL_BUFFER;
    captureParams.image_proc = DPFPDD_IMG_PROC_DEFAULT;
    captureParams.image_res = 500;

    int dpi = GetFirstDPI(hReader);
    if (dpi != 0)
        captureParams.image_res = dpi;

    HSGFPM hFpm;

    if (!initializeSecuGenDevice(hFpm, cause)) {
        return false;
    }
    // Capture images and create templates
    std::vector<std::vector<BYTE>> capturedTemplates;
    for (int i = 0; i < numCaptures; ++i) {
        DPFPDD_CAPTURE_RESULT captureResult;
        std::memset(&captureResult, 0, sizeof(captureResult));
        captureResult.size = sizeof(captureResult);

        unsigned int bufferSize = 400 * 400;
        unsigned char* buffer = new unsigned char[bufferSize];
        std::memset(buffer, 0, bufferSize);

        unsigned int actualSize = bufferSize;

        result = dpfpdd_capture(hReader, &captureParams, 30000, &captureResult, &actualSize, buffer);

        if (result == DPFPDD_SUCCESS && captureResult.success) {
            LOG_INFO << "Capture successful!: " << captureResult.info.height << ":" << captureResult.info.width;

            cv::Mat image = convertAndResizeImage(buffer, captureResult.info.width, captureResult.info.height);

            std::vector<BYTE> raw_image;
            raw_image.assign(image.data, image.data + image.total() * image.elemSize());

        
            DWORD max_size = 0;
            SGFPM_GetMaxTemplateSize(hFpm, &max_size);

            SGFingerInfo fp_info = { 0 };
            fp_info.FingerNumber = SG_FINGPOS_UK;
            fp_info.ViewNumber = 0;
            fp_info.ImpressionType = SG_IMPTYPE_LP;
            fp_info.ImageQuality = 0;

            std::vector<BYTE> template_data(max_size);
            sgStatus = SGFPM_CreateTemplate(hFpm, &fp_info, raw_image.data(), template_data.data());
         
            if (sgStatus != SGFDX_ERROR_NONE) {
                cause = "Failed to create template, error code: " + std::to_string(sgStatus);
                delete[] buffer;
                return false;
            }

            DWORD template_size = 0;
            sgStatus = SGFPM_GetTemplateSize(hFpm, template_data.data(), &template_size);

            if (sgStatus != SGFDX_ERROR_NONE) {
                cause = "Failed to get template size, error code: " + std::to_string(sgStatus);
                SGFPM_Terminate(hFpm);
                return false;
            }

            template_data.resize(template_size);

            capturedTemplates.push_back(template_data);
        }
        else {
            cause = "Capture failed. Check USB connection. Cause: " + std::to_string(result);
            LOG_ERROR << cause;
            delete[] buffer;
            return false;
        }

        delete[] buffer;
    }

    dpfpdd_close(hReader);
    dpfpdd_exit();

    // Merge templates
    if (capturedTemplates.size() == 2) {
        DWORD max_size = 0;
        sgStatus = SGFPM_GetIsoTemplateSizeAfterMerge(hFpm, capturedTemplates[0].data(), capturedTemplates[1].data(), &max_size);
        std::vector<BYTE> mergedTemplate(max_size);
        sgStatus = SGFPM_MergeIsoTemplate(hFpm, capturedTemplates[0].data(), capturedTemplates[1].data(), mergedTemplate.data());
        if (sgStatus != SGFDX_ERROR_NONE) {
            cause = "Failed to merge templates, error code: " + std::to_string(sgStatus);
            SGFPM_Terminate(hFpm);
            return false;
        }

        DWORD template_size = 0;
        sgStatus = SGFPM_GetTemplateSize(hFpm, mergedTemplate.data(), &template_size);

        if (sgStatus != SGFDX_ERROR_NONE) {
            cause = "Failed to get template size, error code: " + std::to_string(sgStatus);
            SGFPM_Terminate(hFpm);
            return false;
        }
        templatesHex = convertTemplateToHex(mergedTemplate, template_size);
        SGFPM_Terminate(hFpm);
        return true;
    }
    else if (capturedTemplates.size() > 2) {
        DWORD max_size = 0;
  
        std::vector<BYTE> combinedVector;
        for (const auto& innerVector : capturedTemplates) {
            combinedVector.insert(combinedVector.end(), innerVector.begin(), innerVector.end());
        }

        std::vector<BYTE> mergedTemplate(combinedVector.size());
        sgStatus = SGFPM_MergeMultipleIsoTemplate(hFpm, combinedVector.data(), numCaptures, mergedTemplate.data());
        if (sgStatus != SGFDX_ERROR_NONE) {
            cause = "Failed to merge multiple templates, error code: " + std::to_string(sgStatus);
            SGFPM_Terminate(hFpm);
            return false;
        }

        DWORD template_size = 0;
        sgStatus = SGFPM_GetTemplateSize(hFpm, mergedTemplate.data(), &template_size);

        if (sgStatus != SGFDX_ERROR_NONE) {
            cause = "Failed to get template size, error code: " + std::to_string(sgStatus);
            SGFPM_Terminate(hFpm);
            return false;
        }
        templatesHex = convertTemplateToHex(mergedTemplate, template_size);
    }

    SGFPM_Terminate(hFpm);

    return true;
}

//std::string createJsonResponse(const std::string& status, const std::string& cause) {
//    Json::Value jsonResponse;
//    jsonResponse["status"] = status;
//    jsonResponse["cause"] = cause;
//    Json::StreamWriterBuilder writer;
//    return Json::writeString(writer, jsonResponse);
//}

size_t WriteCallbackReg(void* contents, size_t size, size_t nmemb, std::string* s)
{
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void clusterReg(const std::string& ip, const std::string& port, const std::string& appId, const std::string& finger_, const std::string& returnId, const std::string& minutiae, const std::string& minutiae2, const ClusterRegCallback& callback)
{
    auto link = "http://" + ip + ":" + port + "/regverify";

    /*Json::Value jsonResponse;
    jsonResponse["appId"] = appId;
    jsonResponse["finger"] = finger_;
    jsonResponse["returnId"] = returnId;
    jsonResponse["minutiae"] = minutiae;
    jsonResponse["minutiae2"] = minutiae2;*/
    // Start building the JSON string
    //std::string jsonStr = jsonResponse.toStyledString();

    std::ostringstream jsonStream;
    jsonStream << "{";
    jsonStream << "\"appId\":\"" << appId << "\",";
    jsonStream << "\"finger\":\"" << finger_ << "\",";
    jsonStream << "\"returnId\":\"" << returnId << "\",";
    jsonStream << "\"minutiae\":\"" << minutiae << "\",";
    jsonStream << "\"minutiae2\":\"" << minutiae2 << "\"";
    jsonStream << "}";
    std::string jsonStr = jsonStream.str();



    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread([link, jsonStr, &promise]() {
        CURL* curl;
        CURLcode res;
        std::string response_string;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, link.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());

            // Set callback function to capture the response
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackReg);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                promise.set_value("error: " + std::string(curl_easy_strerror(res)));
            }
            else {
                promise.set_value(response_string);
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
        }).detach();

    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
        try {
            std::string result = future.get();
            callback(true, result);
        }
        catch (...) {
            callback(false, createJsonResponse("fail", "Internal server error."));
        }
    }
    else {
        callback(false, createJsonResponse("fail", "Request timeout."));
    }
}

void clusterRegx(const std::string& ip, const std::string& port, const std::string& appId, const std::string& finger_, const std::string& returnId, const std::string& minutiae, const ClusterRegCallback& callback)
{
    auto link = "http://" + ip + ":" + port + "/registration";

    /*Json::Value jsonResponse;
    jsonResponse["appId"] = appId;
    jsonResponse["finger"] = finger_;
    jsonResponse["returnId"] = returnId;
    jsonResponse["minutiae"] = minutiae;
    jsonResponse["minutiae2"] = minutiae2;*/
    // Start building the JSON string
    //std::string jsonStr = jsonResponse.toStyledString();

    std::ostringstream jsonStream;
    jsonStream << "{";
    jsonStream << "\"appId\":\"" << appId << "\",";
    jsonStream << "\"finger\":\"" << finger_ << "\",";
    jsonStream << "\"returnId\":\"" << returnId << "\",";
    jsonStream << "\"minutiae\":\"" << minutiae << "\"";
    jsonStream << "}";
    std::string jsonStr = jsonStream.str();



    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread([link, jsonStr, &promise]() {
        CURL* curl;
        CURLcode res;
        std::string response_string;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, link.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());

            // Set callback function to capture the response
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackReg);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                promise.set_value("error: " + std::string(curl_easy_strerror(res)));
            }
            else {
                promise.set_value(response_string);
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
        }).detach();

    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
        try {
            std::string result = future.get();
            callback(true, result);
        }
        catch (...) {
            callback(false, createJsonResponse("fail", "Internal server error."));
        }
    }
    else {
        callback(false, createJsonResponse("fail", "Request timeout."));
    }
}


void clusterRegTemplate(const std::string& ip, const std::string& port, const std::string& appId, const std::string& finger_, const std::string& returnId, const std::string& param, const std::string& minutiae, const ClusterRegCallback& callback)
{
    auto link = "http://" + ip + ":" + port + "/registration";

    /*Json::Value jsonResponse;
    jsonResponse["appId"] = appId;
    jsonResponse["finger"] = finger_;
    jsonResponse["returnId"] = returnId;
    jsonResponse["minutiae"] = minutiae;
    jsonResponse["minutiae2"] = minutiae2;*/
    // Start building the JSON string
    //std::string jsonStr = jsonResponse.toStyledString();

    std::ostringstream jsonStream;
    jsonStream << "{";
    jsonStream << "\"appId\":\"" << appId << "\",";
    jsonStream << "\"finger\":\"" << finger_ << "\",";
    jsonStream << "\"returnId\":\"" << returnId << "\",";
    jsonStream << "\"minutiae\":\"" << minutiae << "\"";
    jsonStream << param;
    jsonStream << "}";
    std::string jsonStr = jsonStream.str();



    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    std::thread([link, jsonStr, &promise]() {
        CURL* curl;
        CURLcode res;
        std::string response_string;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, link.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());

            // Set callback function to capture the response
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackReg);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                promise.set_value("error: " + std::string(curl_easy_strerror(res)));
            }
            else {
                promise.set_value(response_string);
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
        }).detach();

    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
        try {
            std::string result = future.get();
            callback(true, result);
        }
        catch (...) {
            callback(false, createJsonResponse("fail", "Internal server error."));
        }
    }
    else {
        callback(false, createJsonResponse("fail", "Request timeout."));
    }
}
//
//void clusterReg(const std::string& ip, const std::string& port, const std::string& appId, const std::string& finger_, const std::string& returnId, const std::string& minutiae, const std::string& minutiae2, const ClusterRegCallback& callback)
//{
//    auto link = "http://" + ip + ":" + port;
//    auto client = drogon::HttpClient::newHttpClient(link);
//
//    std::string hostname = "clientDevice";
//    if (hostname.empty()) {
//        hostname = "DummyHostname";
//    }
//
//    Json::Value jsonResponse;
//    jsonResponse["appId"] = appId;
//    jsonResponse["finger"] = finger_;
//    jsonResponse["returnId"] = returnId;
//    jsonResponse["minutiae"] = minutiae;
//    jsonResponse["minutiae2"] = minutiae2;
//
//    //std::cout << "minutiae: " << minutiae;
//
//
//    auto request = drogon::HttpRequest::newHttpJsonRequest(jsonResponse);
//
//    // Set request method to POST
//    request->setMethod(drogon::HttpMethod::Post);
//    request->setPath("/regverify");
//
//    LOG_INFO << "ClusterManager::clusterReg sent to " << link;
//
//    auto promise = std::make_shared<std::promise<std::string>>();
//    auto future = promise->get_future();
//
//    client->sendRequest(request,
//        [promise](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
//            std::string messageRet;
//
//            if (result == drogon::ReqResult::Ok && response->statusCode() == drogon::k200OK)
//            {
//                LOG_INFO << "sendRequest receive 200k.";
//                auto jsonResponse = response->getJsonObject();
//                if (jsonResponse)
//                {
//                    // Process the JSON response
//                    //LOG_INFO << "sendRequest Response received.";// << jsonResponse->toStyledString() << std::endl;
//                }
//                else
//                {
//                    // Handle the case where the response is not JSON
//                    std::string retstr =  "Response is not in JSON format.";
//                    //LOG_ERROR << retstr;
//                    Json::Value jsonResult;
//                    promise->set_value(createJsonResponse("error", retstr));
//                    return;
//                }
//                LOG_INFO << "sendRequest return json";
//                Json::Value jsonResult = *jsonResponse;
//                promise->set_value(jsonResult.toStyledString());
//            }
//            else
//            {
//                if (response->statusCode() == drogon::k400BadRequest) {
//                    auto jsonResponse = response->getJsonObject();
//                    if (jsonResponse)
//                    {
//                        Json::Value jsonResult = *jsonResponse;
//                        promise->set_value(jsonResult.toStyledString());
//                        return;
//                    }
//                }
//                std::string retstr = "sendRequest sending failed.";
//                //LOG_ERROR << retstr;
//                Json::Value jsonResult;
//                promise->set_value(createJsonResponse("error not send", retstr));
//                return;
//            }
//        });
//
//    std::thread([]() {
//        if(!drogon::app().isRunning())
//        { 
//            //LOG_INFO << "Drogon is not running. starting";
//            drogon::app().run();
//        }// else
//          //  LOG_INFO << "Drogon is  running.";
//
//    }).detach();
//
//    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
//        try {
//            LOG_INFO << "sendRequest waiting for future.";
//            // std::this_thread::sleep_for(std::chrono::seconds(10)); // Simulated delay
//            auto result = future.get();
//            callback(true, result);
//        }
//        catch (...) {
//            LOG_ERROR << "sendRequest internal error.";
//            Json::Value jsonResponse;
//            jsonResponse["status"] = "fail";
//            jsonResponse["cause"] = "Internal server error.";
//            callback(true, jsonResponse.toStyledString());
//        }
//    }
//    else {
//        LOG_ERROR << "sendRequest timeout.";
//        Json::Value jsonResponse;
//        jsonResponse["status"] = "fail";
//        jsonResponse["cause"] = "Request timeout.";
//        callback(true, jsonResponse.toStyledString());
//    }
//}
//

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s)
{
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}
std::unordered_map<std::string, std::string> manualJsonParse(const std::string& jsonStr) {
    std::unordered_map<std::string, std::string> jsonMap;


    // Remove the outer curly braces if present
    std::string cleanedStr = jsonStr;
    cleanedStr.erase(0, cleanedStr.find_first_not_of(" \n\r\t"));
    cleanedStr.erase(cleanedStr.find_last_not_of(" \n\r\t") + 1);

    if (cleanedStr.front() == '{' && cleanedStr.back() == '}') {
        cleanedStr = cleanedStr.substr(1, cleanedStr.length() - 2);
    }

    size_t pos = 0;

    while ((pos = cleanedStr.find(',')) != std::string::npos) {
        std::string pair = cleanedStr.substr(0, pos);
        size_t colonPos = pair.find(':');

        if (colonPos != std::string::npos) {
            std::string key = pair.substr(0, colonPos);
            std::string value = pair.substr(colonPos + 1);

            // Remove quotes from key and value
            key.erase(remove(key.begin(), key.end(), '\"'), key.end());
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \n\r\t"));
            key.erase(key.find_last_not_of(" \n\r\t") + 1);
            value.erase(0, value.find_first_not_of(" \n\r\t"));
            value.erase(value.find_last_not_of(" \n\r\t") + 1);

            if (value.front() == '{')
            {
                size_t nested_pos = 0;
                size_t nested_start_pos = 0;
                pos = nested_pos = cleanedStr.find('}');
                nested_start_pos = cleanedStr.find('{');
                value = cleanedStr.substr(nested_start_pos, nested_pos- nested_start_pos+1);

               // value.erase(remove(value.begin(), value.end(), '\"'), value.end());
                value.erase(0, value.find_first_not_of(" \n\r\t"));
                value.erase(value.find_last_not_of(" \n\r\t") + 1);
            }
            
          

            // Check if the value is a JSON object
            if (value.front() == '{' && value.back() == '}') {
                // Parse the nested JSON object
                auto nestedJson = manualJsonParse(value);

                //// Optionally serialize the nested JSON back to string if needed
                //// Serialize nestedJson to a string (basic implementation)
                //std::string nestedJsonStr = "{";
                //for (const auto& nPair : nestedJson) {
                //    nestedJsonStr += "\"" + nPair.first + "\":\"" + nPair.second + "\",";
                //}
                //if (nestedJsonStr.back() == ',') {
                //    nestedJsonStr.pop_back(); // Remove the last comma
                //}
                //nestedJsonStr += "}";

                //value = nestedJsonStr; // Assign the serialized nested JSON string
                jsonMap.insert(nestedJson.begin(), nestedJson.end());
            } else
                jsonMap[key] = value;
        }
        cleanedStr.erase(0, pos + 1);
    }

    // Handle the last key-value pair
    if (!cleanedStr.empty()) {
        size_t colonPos = cleanedStr.find(':');
        if (colonPos != std::string::npos) {
            std::string key = cleanedStr.substr(0, colonPos);
            std::string value = cleanedStr.substr(colonPos + 1);

            key.erase(remove(key.begin(), key.end(), '\"'), key.end());
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());
            key.erase(0, key.find_first_not_of(" \n\r\t"));
            key.erase(key.find_last_not_of(" \n\r\t") + 1);
            value.erase(0, value.find_first_not_of(" \n\r\t"));
            value.erase(value.find_last_not_of(" \n\r\t") + 1);

            // Check if the value is a JSON object
            if (value.front() == '{' && value.back() == '}') {
                // Parse the nested JSON object
                auto nestedJson = manualJsonParse(value);

                // Serialize nestedJson to a string (basic implementation)
                std::string nestedJsonStr = "{";
                for (const auto& nPair : nestedJson) {
                    nestedJsonStr += "\"" + nPair.first + "\":\"" + nPair.second + "\",";
                }
                if (nestedJsonStr.back() == ',') {
                    nestedJsonStr.pop_back(); // Remove the last comma
                }
                nestedJsonStr += "}";

                value = nestedJsonStr; // Assign the serialized nested JSON string
            }

            jsonMap[key] = value;
        }
    }

    return jsonMap;
}





void clusterVal(const std::string& ip, const std::string& port, const std::string& appId, const std::string& minutiae, const std::function<void(bool, const std::string&)>& callback) {
    auto url = "http://" + ip + ":" + port + "/validation";
    std::string ver = JSONCPP_VERSION_STRING;
    std::string jsonStr = "{"
        "\"appId\": \"" + appId + "\", "
        "\"minutiae\": \"" + minutiae + "\""
        "}";

 
    std::promise<std::string> promise;
    auto future = promise.get_future();

    std::thread([url, jsonStr, promise = std::move(promise)]() mutable {
        CURL* curl = curl_easy_init();
        if (!curl) {
            promise.set_value(createJsonResponse("error", "Unable to initialize curl"));
            return;
        }

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            promise.set_value(createJsonResponse("error", "Error: " + std::string(curl_easy_strerror(res))));
        }
        else {
            promise.set_value(response);
        }
        }).detach();

    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
        try {
            std::string result = future.get();
            LOG_INFO << "Response: " << result;

            // Check if result contains escape characters
            if (result.find('\\') != std::string::npos) {
                // Remove escape characters
                result.erase(std::remove(result.begin(), result.end(), '\\'), result.end());
            }

            // Manual parsing of the main response
            auto jsonResponse = manualJsonParse(result);
            LOG_INFO << "after Response: " << result;

            // Check if returnId exists
            if (jsonResponse.find("toReturn") != jsonResponse.end()) {
                std::string returnIdStr = jsonResponse["toReturn"];

                LOG_INFO << "toReturn: " << returnIdStr;

                // Manually parse returnIdStr
                auto returnIdJson = manualJsonParse(returnIdStr);
                if (returnIdJson.empty()) {
                    LOG_ERROR << "Failed to parse returnId: " << returnIdStr;
                    callback(false, createJsonResponse("error", "Invalid returnId format."));
                    return;
                }

                // Merge the fields from returnIdJson into jsonResponse
                jsonResponse.insert(returnIdJson.begin(), returnIdJson.end());
            }

            //std::string similarity = jsonResponse["similarity"];
            std::string version = jsonResponse["version"];
            //if (!isSimilarityValid(similarity, version)) {
            //    std::string cause = "Not found.";
            //    callback(false, createJsonResponse("error", cause));
            //    return;
            //}

            //// Remove the original returnId and similarity fields
            ////jsonResponse.erase("returnId");
            //jsonResponse.erase("similarity");

            // Add version information
            //jsonResponse["cversion"] = "v1.0"; // Replace with actual version

            //add minutiea if version == 0;
            if (version == "0")
            {
               // include templateHex on the response
                jsonResponse["templateHex"] = minutiae;

            }

            // Convert back to JSON string manually
            std::string newJson = "{";
            for (const auto& pair : jsonResponse) {
                newJson += "\"" + pair.first + "\":\"" + pair.second + "\",";
            }
            newJson.pop_back(); // Remove the last comma
            newJson += "}";

            callback(true, newJson);
        }
        catch (...) {
            callback(false, createJsonResponse("error", "Internal server error."));
        }
    }
    else {
        callback(false, createJsonResponse("error", "Request timeout."));
    }




}



void clusterValWithTemplate(const std::string& ip, const std::string& port, const std::string& appId, const std::string& minutiae, const std::function<void(bool, const std::string&)>& callback) {
    auto url = "http://" + ip + ":" + port + "/validation";
    std::string ver = JSONCPP_VERSION_STRING;
    std::string jsonStr = "{"
        "\"appId\": \"" + appId + "\", "
        "\"minutiae\": \"" + minutiae + "\""
        "}";


    std::promise<std::string> promise;
    auto future = promise.get_future();

    std::thread([url, jsonStr, promise = std::move(promise)]() mutable {
        CURL* curl = curl_easy_init();
        if (!curl) {
            promise.set_value(createJsonResponse("error", "Unable to initialize curl"));
            return;
        }

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            promise.set_value(createJsonResponse("error", "Error: " + std::string(curl_easy_strerror(res))));
        }
        else {
            promise.set_value(response);
        }
        }).detach();

    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
        try {
            std::string result = future.get();
            LOG_INFO << "Response: " << result;

            // Check if result contains escape characters
            if (result.find('\\') != std::string::npos) {
                // Remove escape characters
                result.erase(std::remove(result.begin(), result.end(), '\\'), result.end());
            }

            // Manual parsing of the main response
            auto jsonResponse = manualJsonParse(result);
            LOG_INFO << "after Response: " << result;

            // Check if returnId exists
            if (jsonResponse.find("toReturn") != jsonResponse.end()) {
                std::string returnIdStr = jsonResponse["toReturn"];

                LOG_INFO << "toReturn: " << returnIdStr;

                // Manually parse returnIdStr
                auto returnIdJson = manualJsonParse(returnIdStr);
                if (returnIdJson.empty()) {
                    LOG_ERROR << "Failed to parse returnId: " << returnIdStr;
                    callback(false, createJsonResponse("error", "Invalid returnId format."));
                    return;
                }

                // Merge the fields from returnIdJson into jsonResponse
                jsonResponse.insert(returnIdJson.begin(), returnIdJson.end());
            }

            std::string similarity = jsonResponse["similarity"];
            std::string version = jsonResponse["version"];
            if (!isSimilarityValid(similarity, version)) {
                std::string cause = "Not found.";
                callback(false, createJsonResponse("error", cause));
                return;
            }

            // Remove the original returnId and similarity fields
            //jsonResponse.erase("returnId");
            jsonResponse.erase("similarity");

            // Add version information
            //jsonResponse["cversion"] = "v1.0"; // Replace with actual version

            //add minutiea if version == 0;
            if (version == "0")
            {
                // make a re-save
                // for posting
            }

            // Convert back to JSON string manually
            std::string newJson = "{";
            for (const auto& pair : jsonResponse) {
                newJson += "\"" + pair.first + "\":\"" + pair.second + "\",";
            }
            newJson.pop_back(); // Remove the last comma
            newJson += "}";

            callback(true, newJson);
        }
        catch (...) {
            callback(false, createJsonResponse("error", "Internal server error."));
        }
    }
    else {
        callback(false, createJsonResponse("error", "Request timeout."));
    }




}
// 
// 
//void clusterVal(const std::string& ip, const std::string& port, const std::string& appId, const std::string& minutiae, const ClusterRegCallback& callback)
//{
//    auto link = "http://" + ip + ":" + port;
//    auto client = drogon::HttpClient::newHttpClient(link);
//
//    std::string hostname = "clientDevice";
//    if (hostname.empty()) {
//        hostname = "DummyHostname";
//    }
//
//    Json::Value jsonResponse;
//    jsonResponse["appId"] = appId;
//    jsonResponse["minutiae"] = minutiae;
//
//    auto request = drogon::HttpRequest::newHttpJsonRequest(jsonResponse);
//
//    // Set request method to POST
//    request->setMethod(drogon::HttpMethod::Post);
//    request->setPath("/validation");
//    //auto request = drogon::HttpRequest::newHttpRequest();
//
//    //// Set basic parameters
//    //request->setMethod(drogon::Post);
//    //request->setPath("/simple");
//
//    LOG_INFO << "ClusterManager::clusterVal sent to " << link;
//
//    auto promise = std::make_shared<std::promise<std::string>>();
//    auto future = promise->get_future();
//
//    client->sendRequest(request,
//        [promise](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
//            std::string messageRet;
//
//            if (result == drogon::ReqResult::Ok && response->statusCode() == drogon::k200OK)
//            {
//                LOG_INFO << "sendRequest receive 200k.";
//                auto jsonResponse = response->getJsonObject();
//                if (jsonResponse)
//                {
//                    // Process the JSON response
//                    //LOG_INFO << "sendRequest Response received.";// << jsonResponse->toStyledString() << std::endl;
//                }
//                else
//                {
//                    // Handle the case where the response is not JSON
//                    std::string cause = "Response is not in JSON format.";
//                    LOG_ERROR << cause;
//                    promise->set_value(createJsonResponse("error", cause));
//                    return;
//                }
//                LOG_INFO << "sendRequest return json";
//                Json::Value jsonResult = *jsonResponse;
//
//                std::string similarity = jsonResult["similarity"].asString();
//
//                if (!isSimilarityValid(similarity)) {
//                    //LOG_ERROR << "Not found:" << similarity;
//                    std::string cause = "Not found.";
//                    //LOG_ERROR << cause;
//                    promise->set_value(createJsonResponse("error", cause));
//                    return;
//                }
//                jsonResult.removeMember("similarity");
//                jsonResult["cversion"] = std::string(CLIENT_VERSION);
//                promise->set_value(jsonResult.toStyledString());
//            }
//            else
//            {
//                std::string cause = "sendRequest sending failed.";
//                //LOG_ERROR << cause;
//                promise->set_value(createJsonResponse("error", cause));
//                return;
//            }
//        });
//
//    std::thread([]() {
//        if (!drogon::app().isRunning())
//        {
//            //LOG_INFO << "Drogon is not running. starting";
//            drogon::app().run();
//        }
//      /*  else
//            LOG_INFO << "Drogon is  running.";*/
//
//        }).detach();
//
//    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
//        try {
//            LOG_INFO << "sendRequest waiting for future.";
//            // std::this_thread::sleep_for(std::chrono::seconds(10)); // Simulated delay
//            auto result = future.get();
//            callback(true, result);
//        }
//        catch (...) {
//            std::string cause = "sendRequest internal error.";
//            LOG_ERROR << cause;
//            callback(true, createJsonResponse("error", cause));
//        }
//    }
//    else {
//        std::string cause = "sendRequest timeout.";
//        LOG_ERROR << cause;
//        callback(true, createJsonResponse("error", cause));
//    }
//}


//BOOL WINAPI ConsoleHandler(DWORD signal) {
//    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT ||
//        signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT) {
//        LOG_INFO << "Received signal to stop the application";
//        drogon::app().quit();
//        return TRUE;
//    }
//    return FALSE;
//}
bool isValidNumber(const char* numCaptured) {
    // Convert const char* to integer
    try {
        int num = std::stoi(numCaptured);

        // Check if the number is within the range 1 to 10
        if (num >= 1 && num <= 10) {
            return true;
        }
        else {
            return false;
        }
    }
    catch (const std::invalid_argument& e) {
        // Handle the case where numCaptured is not a valid integer
        return false;
    }
    catch (const std::out_of_range& e) {
        // Handle the case where numCaptured is out of the range of int
        return false;
    }
}



const char* ScanValidateFinger(const char* ipAddress, const char* port, const char* appId)
{
    std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
        //unsigned int bufferSize = 400 * 400;
        //unsigned char* buffer = new unsigned char[bufferSize];
        //std::memset(buffer, 0, bufferSize);
        std::string cause;
        std::string templateHex;
        mMessageRet.clear();
        mStage = STAGE_IDLE;

        LOG_INFO << "Ready to scan a fingerprint for verification purposes.";

        cv::Mat fingerprintImage;
        std::string ip = ipAddress;
        std::string mport = port;
        std::string mappId = appId;

        if (!validateInputVal(ip, mport, mappId, cause)) {
            std::string retstr = "Validation fail. Cause: " + cause;
            LOG_ERROR << retstr;
            mMessageRet = createJsonResponse("error", retstr);
            return mMessageRet.c_str();
        }


        // Start capture in a separate thread
        mStage = STAGE_CAPTURE;
        if (!captureFingerprintImage(fingerprintImage, cause, cancelPromise)) {
            std::string retstr = "Failed to capture. Cause: " + cause;
            //LOG_ERROR << retstr;
            mMessageRet = createJsonResponse("error", retstr);
            return mMessageRet.c_str();
        }

        if (!getTemplate(fingerprintImage, templateHex, cause)) {
            std::string retstr = "Failed to getTemplate. Cause: " + cause;
            LOG_ERROR << retstr;
            mMessageRet = createJsonResponse("error", retstr);
            return mMessageRet.c_str();
        }


      /*  if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
            std::string retstr = "Failed to set control handler";
            LOG_ERROR << retstr;
            mMessageRet = createJsonResponse("error", retstr);
            return mMessageRet.c_str();
        }*/
      
        mStage = STAGE_SENDING;
        std::string messages_base;
        clusterVal(ip, mport, mappId, templateHex, [&messages_base](bool success, const std::string& message) {
      
            if (success) {
                LOG_INFO << "clusterReg success: " << message;
                messages_base = message;
            }
            else {
                LOG_SYSERR << "clusterReg - Operation failed: " << message;
                messages_base = message;
            }
        });

        mMessageRet = messages_base;

	return mMessageRet.c_str();
}

//const char* startScanAndGetFingerIDn(const char* ipAddress, const char* port, const char* appId)
//{
//    initLog(false);
//    return ScanValidateFinger(ipAddress, port, appId);
//}

const char* startScanAndGetFingerID(const char* ipAddress, const char* port, const char* appId, const bool isLog)
{
    initLog(isLog);
    return ScanValidateFinger(ipAddress, port, appId);
}



const char* ScanRegisterFinger(const char* ipAddress, const char* port, const char* appId, const char* finger_, const char* returnId, const char* minutiae_)
{
    std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
    //unsigned int bufferSize = 400 * 400;
    //unsigned char* buffer = new unsigned char[bufferSize];
    //std::memset(buffer, 0, bufferSize);
    std::string cause;
    std::string templateHex;
    mMessageRet.clear();
    mStage = STAGE_IDLE;
    cv::Mat fingerprintImage;

    LOG_INFO << "Ready to scan a fingerprint for registration.";

    std::string ip = ipAddress;
    std::string mport = port;
    std::string mappId = appId;
    std::string finger = finger_;
    std::string mreturnId = returnId;
    std::string minutiae2 = minutiae_;

    if (!validateInputReg(ip, mport, mappId, mreturnId, minutiae2, cause)) {
        std::string retstr = "Validation fail. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }


    // Start capture in a separate thread
    mStage = STAGE_CAPTURE;
    if (!captureFingerprintImage(fingerprintImage, cause, cancelPromise)) {
        std::string retstr = "Failed to capture. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }
  
    if (!getTemplate(fingerprintImage, templateHex, cause)) {
        std::string retstr = "Failed to getTemplate. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }


    //if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
    //    std::string retstr = "Failed to set control handler";
    //    LOG_ERROR << retstr;
    //    mMessageRet = createJsonResponse("error", retstr);
    //    return mMessageRet.c_str();
    //}

    mStage = STAGE_SENDING;
    std::string messages_base;
    clusterReg(ip, mport, mappId, finger, mreturnId, templateHex, minutiae2, [&messages_base](bool success, const std::string& message) {

        if (success) {
            LOG_INFO << "clusterReg success: " << message;
            messages_base = message;
        }
        else {
            LOG_SYSERR << "clusterReg - Operation failed: " << message;
            messages_base = message;
        }
        });

    mMessageRet = messages_base;

    return mMessageRet.c_str();
}

// call for extension


const char* startScanAndRegisterFingerID(const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId, const char* minutiae, const bool isLog)
{
    // islog false for chrome extension
    initLog(isLog);
    // include minutiae from the scan
    return ScanRegisterFinger(ipAddress, port, appId, finger, returnId, minutiae);
}

const char* startScan(const char* ipAddress, const char* port, const char* appId, const char* finger_, const char* returnId, const bool isLog)
{
    // islog false for chrome extension
    initLog(isLog);
    std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
    //unsigned int bufferSize = 400 * 400;
    //unsigned char* buffer = new unsigned char[bufferSize];
    //std::memset(buffer, 0, bufferSize);
    std::string cause;
    std::string templateHex;
    mMessageRet.clear();
    mStage = STAGE_IDLE;
    cv::Mat fingerprintImage;

    LOG_INFO << "Ready to scan a fingerprint for registration.";

    std::string ip = ipAddress;
    std::string mport = port;
    std::string mappId = appId;
    std::string finger = finger_;
    std::string mreturnId = returnId;
    std::string minutiae2 = "none";

    if (!validateInputReg(ip, mport, mappId, mreturnId, minutiae2, cause)) {
        std::string retstr = "Validation fail. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }


    // Start capture in a separate thread
    mStage = STAGE_CAPTURE;
    if (!captureFingerprintImage(fingerprintImage, cause, cancelPromise)) {
        std::string retstr = "Failed to capture. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    if (!getTemplate(fingerprintImage, templateHex, cause)) {
        std::string retstr = "Failed to getTemplate. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    // save temaplate to memory
    //mMinutiae = templateHex;

    Json::Value jsonResult;
    jsonResult["status"] = "success";
    jsonResult["cause"] = "Finger has been captured.";
    jsonResult["minutiae"] = templateHex;
    jsonResult["client version"] = std::string(CLIENT_VERSION);

    mMessageRet = jsonResult.toStyledString();
    return mMessageRet.c_str();
}



const char* ScanRegisterFingerx(const char* ipAddress, const char* port, const char* appId, const char* finger_, const char* returnId)
{
    std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
    //unsigned int bufferSize = 400 * 400;
    //unsigned char* buffer = new unsigned char[bufferSize];
    //std::memset(buffer, 0, bufferSize);
    std::string cause;
    std::string templateHex;
    mMessageRet.clear();
    mStage = STAGE_IDLE;
    cv::Mat fingerprintImage;

    LOG_INFO << "Ready to scan a fingerprint for registration.";

    std::string ip = ipAddress;
    std::string mport = port;
    std::string mappId = appId;
    std::string finger = finger_;
    std::string mreturnId = returnId;
    std::string minutiae2 = "123123";

    if (!validateInputReg(ip, mport, mappId, mreturnId, minutiae2, cause)) {
        std::string retstr = "Validation fail. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }


    // Start capture in a separate thread
    mStage = STAGE_CAPTURE;
    if (!captureFingerprintImage(fingerprintImage, cause, cancelPromise)) {
        std::string retstr = "Failed to capture. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    if (!getTemplate(fingerprintImage, templateHex, cause)) {
        std::string retstr = "Failed to getTemplate. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    mStage = STAGE_SENDING;
    std::string messages_base;
    clusterRegx(ip, mport, mappId, finger, mreturnId, templateHex, [&messages_base](bool success, const std::string& message) {

        if (success) {
            LOG_INFO << "clusterReg success: " << message;
            messages_base = message;
        }
        else {
            LOG_SYSERR << "clusterReg - Operation failed: " << message;
            messages_base = message;
        }
        });

    mMessageRet = messages_base;

    return mMessageRet.c_str();
}


const char* startScanAndRegisterFingerIDx(const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId, const bool isLog)
{
    // islog false for chrome extension
    initLog(isLog);
    // include minutiae from the scan
    return ScanRegisterFingerx(ipAddress, port, appId, finger, returnId);
}


//
//const char* startScanAndRegisterFingerIDx(const char* numCaptured, const char* ipAddress, const char* port, const char* appId, const char* returnId)
//{
//    std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
//    initLog(true);
//    //unsigned int bufferSize = 400 * 400;
//    //unsigned char* buffer = new unsigned char[bufferSize];
//    //std::memset(buffer, 0, bufferSize);
//    std::string cause;
//    std::string templateHex;
//    mMessageRet.clear();
//
//    LOG_INFO << "Ready to scan a fingerprint for registration.";
//
//    std::string ip = ipAddress;
//    std::string mport = port;
//    std::string mappId = appId;
//    std::string mreturnId = returnId;
//
//    if (!isValidNumber(numCaptured))
//    {
//        std::string retstr = "Invalid numCapture value, must be 1-10. Provided: " + std::string(numCaptured);
//        LOG_ERROR << retstr;
//        mMessageRet = createJsonResponse("error", retstr);
//        return mMessageRet.c_str();
//    }
//
//    int number_captured = std::stoi(numCaptured);
//
//
//    //validation of give param
//    if (!validateInputReg(ip, mport, mappId, mreturnId, cause)) {
//        std::string retstr = "Validation fail. Cause: " + cause;
//        LOG_ERROR << retstr;
//        mMessageRet = createJsonResponse("error", retstr);
//        return mMessageRet.c_str();
//    }
//
//    // Start capture in a separate thread
//    if (!captureAndCreateTemplate(number_captured, templateHex, cause, cancelPromise)) {
//        std::string retstr = "Failed to capture. Cause: " + cause;
//        LOG_ERROR << retstr;
//        mMessageRet = createJsonResponse("error", retstr);
//        return mMessageRet.c_str();
//    }
//
//    // sending to server
//    std::string messages_base;
//    clusterReg(ip, mport, mappId, mreturnId, templateHex, [&messages_base](bool success, const std::string& message) {
//
//        if (success) {
//            LOG_INFO << "clusterReg success: " << message;
//            messages_base = message;
//        }
//        else {
//            LOG_SYSERR << "clusterReg - Operation failed: " << message;
//            messages_base = message;
//        }
//        });
//
//    mMessageRet = messages_base;
//
//    return mMessageRet.c_str();
//}


const char* startScanAndGetFingerIDx(const char* numCaptured, const char* ipAddress, const char* port, const char* appId)
{
    std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
    initLog(true);
    //unsigned int bufferSize = 400 * 400;
    //unsigned char* buffer = new unsigned char[bufferSize];
    //std::memset(buffer, 0, bufferSize);
    std::string cause;
    std::string templateHex;
    mMessageRet.clear();

    LOG_INFO << "Ready to scan a fingerprint for registration.";

    std::string ip = ipAddress;
    std::string mport = port;
    std::string mappId = appId;

    if (!isValidNumber(numCaptured))
    {
        std::string retstr = "Invalid numCapture value, must be 1-10. Provided: " + std::string(numCaptured);
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    int number_captured = std::stoi(numCaptured);



    //validation of give param
    if (!validateInputVal(ip, mport, mappId, cause)) {
        std::string retstr = "Validation fail. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    // Start capture in a separate thread
    if (!captureAndCreateTemplate(number_captured, templateHex, cause, cancelPromise)) {
        std::string retstr = "Failed to capture. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    // sending to server
    std::string messages_base;
    clusterVal(ip, mport, mappId, templateHex, [&messages_base](bool success, const std::string& message) {

        if (success) {
            LOG_INFO << "clusterReg success: " << message;
            messages_base = message;
        }
        else {
            LOG_SYSERR << "clusterReg - Operation failed: " << message;
            messages_base = message;
        }
        });

    mMessageRet = messages_base;

    return mMessageRet.c_str();
}

const char* cancelRequest()
{
    if(mStage == STAGE_IDLE) 
    {
        mMessageRet = createJsonResponse("success", "On idle state.");
        return mMessageRet.c_str();
    }
    if (mStage == STAGE_CAPTURE)
    {

        mMessageRet = createJsonResponse("success", "On idle state.");
        return mMessageRet.c_str();
    }
    if (mStage == STAGE_SENDING)
    {
        mMessageRet = createJsonResponse("success", "On idle state.");
        return mMessageRet.c_str();
    }
}



const char* ScanRegisterFingerf(const char* filename, const char* ipAddress, const char* port, const char* appId, const char* finger_, const char* returnId)
{
    std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
    //unsigned int bufferSize = 400 * 400;
    //unsigned char* buffer = new unsigned char[bufferSize];
    //std::memset(buffer, 0, bufferSize);
    std::string cause;
    std::string templateHex;
    mMessageRet.clear();
    mStage = STAGE_IDLE;
    cv::Mat fingerprintImage;

    LOG_INFO << "Ready to scan a fingerprint for registration.";

    std::string ip = ipAddress;
    std::string mport = port;
    std::string mappId = appId;
    std::string finger = finger_;
    std::string mreturnId = returnId;
    std::string minutiae2 = "123123";

    if (!validateInputReg(ip, mport, mappId, mreturnId, minutiae2, cause)) {
        std::string retstr = "Validation fail. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);

    if (!getTemplate(image, templateHex, cause)) {
        std::string retstr = "Failed to getTemplate. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    mStage = STAGE_SENDING;
    std::string messages_base;
    clusterRegx(ip, mport, mappId, finger, mreturnId, templateHex, [&messages_base](bool success, const std::string& message) {

        if (success) {
            LOG_INFO << "clusterReg success: " << message;
            messages_base = message;
        }
        else {
            LOG_SYSERR << "clusterReg - Operation failed: " << message;
            messages_base = message;
        }
        });

    mMessageRet = messages_base;

    return mMessageRet.c_str();
}

const char* startScanAndRegisterFingerIDf(const char* filename, const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId, const bool isLog)
{
    // islog false for chrome extension
    initLog(isLog);
    // include minutiae from the scan
    return ScanRegisterFingerf(filename, ipAddress, port, appId, finger, returnId);
}


const  char* convertImageToTemplate(unsigned char* imageData, int size,  const bool isLog)
{
    initLog(isLog);
    mMessageRet.clear();

    std::vector<uchar> imageBuffer(imageData, imageData + size);



    // Decode image from buffer
    cv::Mat image = cv::imdecode(imageBuffer, cv::IMREAD_GRAYSCALE); // Change to cv::IMREAD_COLOR if needed
    //cv::imwrite("image.png", image);



    size_t totalElements = image.total();  // Total number of elements (rows * cols)
    std::cout << "Total elements: " << totalElements << std::endl;


    if (image.empty()) {
        std::string retstr = "Error: Cannot load image";
        mMessageRet = retstr;
        return mMessageRet.c_str();
    }

    cv::Mat flippedImage;
    cv::flip(image, flippedImage, -1); // 0 means flip vertically

    // cv::imwrite("imageFlip.png", flippedImage);
   
    cv::Mat resizedFrame;
    cv::Size newSize(300, 400); // Example new size for the frame
    resizeFrame(flippedImage, resizedFrame, newSize);
 //   cv::resize(image, resizedFrame, cv::Size(300, 400));
    cv::imwrite("resize.png", resizedFrame);
   
    cv::Mat contrastBrightness;
    double alpha = 1; // Contrast control (1.0 - 3.0)
    int beta = -50;      // Brightness control (0 - 100)
    adjustContrastBrightness(resizedFrame, contrastBrightness, alpha, beta);
    cv::imwrite("contrast.png", contrastBrightness);

    //cv::Mat GaussianBlurImage;
    //// Apply Gaussian Blur
    //int kernelSize = 3;  // Size of the Gaussian kernel (e.g., 3, 5, 7, 9, etc.)
    //double sigmaX = 1; // Standard deviation in the X direction
    //applyGaussianBlur(contrastBrightness, GaussianBlurImage, kernelSize, sigmaX);

    //cv::Mat binarizedGImage;
    //binarizeImage(GaussianBlurImage, binarizedGImage);

    cv::Mat smoothedFingerprint = smoothFingerprintEdges(contrastBrightness);
    cv::imwrite("Smooth2.png", smoothedFingerprint);

    std::string templateHex, cause;

    if (!getTemplate(smoothedFingerprint, templateHex, cause)) {
        std::string retstr = "Error: Failed to gconvert Template. Cause: " + cause;
        mMessageRet = retstr;
        return mMessageRet.c_str();
    }

    mMessageRet = templateHex;
    return mMessageRet.c_str();
}

const  char* convertImageToTemplateWithId(unsigned char* imageData, int size, int id, const bool isLog)
{
    initLog(isLog);
    mMessageRet.clear();

    std::vector<uchar> imageBuffer(imageData, imageData + size);

    std::string id_string = std::to_string(id);



    // Decode image from buffer
    cv::Mat image = cv::imdecode(imageBuffer, cv::IMREAD_GRAYSCALE); // Change to cv::IMREAD_COLOR if needed
    //cv::imwrite("image.png", image);



    size_t totalElements = image.total();  // Total number of elements (rows * cols)
    std::cout << "Total elements: " << totalElements << std::endl;


    if (image.empty()) {
        std::string retstr = "Error: Cannot load image";
        mMessageRet = retstr;
        return mMessageRet.c_str();
    }

    cv::Mat flippedImage;
    cv::flip(image, flippedImage, -1); // 0 means flip vertically

    // cv::imwrite("imageFlip.png", flippedImage);

    cv::Mat resizedFrame;
    cv::Size newSize(300, 400); // Example new size for the frame
    resizeFrame(flippedImage, resizedFrame, newSize);
    //   cv::resize(image, resizedFrame, cv::Size(300, 400));
    cv::imwrite(id_string + "_resize.png", resizedFrame);

    cv::Mat contrastBrightness;
    double alpha = 1; // Contrast control (1.0 - 3.0)
    int beta = -50;      // Brightness control (0 - 100)
    adjustContrastBrightness(resizedFrame, contrastBrightness, alpha, beta);
    cv::imwrite(id_string + "_contrast.png", contrastBrightness);

    //cv::Mat GaussianBlurImage;
    //// Apply Gaussian Blur
    //int kernelSize = 3;  // Size of the Gaussian kernel (e.g., 3, 5, 7, 9, etc.)
    //double sigmaX = 1; // Standard deviation in the X direction
    //applyGaussianBlur(contrastBrightness, GaussianBlurImage, kernelSize, sigmaX);

    //cv::Mat binarizedGImage;
    //binarizeImage(GaussianBlurImage, binarizedGImage);

    cv::Mat smoothedFingerprint = smoothFingerprintEdges(contrastBrightness);
    cv::imwrite(id_string + "_Smooth2.png", smoothedFingerprint);

    std::string templateHex, cause;

    if (!getTemplate(smoothedFingerprint, templateHex, cause)) {
        std::string retstr = "Error: Failed to gconvert Template. Cause: " + cause;
        mMessageRet = retstr;
        return mMessageRet.c_str();
    }

    mMessageRet = templateHex;
    return mMessageRet.c_str();
}

bool convertImageToTemplateBool(unsigned char* imageData, int width, int height,  bool isLog, std::string& result)
{
    initLog(isLog);
    mMessageRet.clear();

   // std::vector<uchar> imageBuffer(imageData, imageData + size);

    // Decode image from buffer
   // cv::Mat image = cv::imdecode(imageBuffer, cv::IMREAD_GRAYSCALE); // Change to cv::IMREAD_COLOR if needed
    //cv::Mat image(height, width, CV_8UC1, imageData);

    //if (image.empty()) {
    //    result = "Error: Cannot load image";
    //    return false;
    //}

    //cv::Mat flippedImage;
    //cv::flip(image, flippedImage, -1); // Flip both horizontally and vertically

    //cv::Mat resizedFrame;
    //cv::Size newSize(300, 400); // Example new size for the frame
    //resizeFrame(flippedImage, resizedFrame, newSize);
    //cv::imwrite("resize.png", resizedFrame);

    //cv::Mat contrastBrightness;
    //double alpha = 1; // Contrast control (1.0 - 3.0)
    //int beta = -50;   // Brightness control (-100 to 100)
    //adjustContrastBrightness(resizedFrame, contrastBrightness, alpha, beta);
    //cv::imwrite("contrast.png", contrastBrightness);

    //cv::Mat smoothedFingerprint = smoothFingerprintEdges(contrastBrightness);
    //cv::imwrite("Smooth2.png", smoothedFingerprint);

    std::string templateHex, cause;

    cv::Mat image = convertAndResizeImage(imageData, width, height);

    cv::imwrite("FinalConvert.png", image);

    if (!getTemplate(image, templateHex, cause)) {
        result = "Error: Failed to convert Template. Cause: " + cause;
        return false;
    }

    result = templateHex;
    return true;
}


const char* getImageAndGetFingerIdSmooth(unsigned char* imageData, int width, int height, const char* ipAddress, const char* port, const char* appId, const bool isLog)
{
    std::string templateHex;

    // templateHex = convertImageToTemplate(imageData, width * height, isLog);
    if (!convertImageToTemplateBool(imageData, width, height, isLog, templateHex))
    {
        LOG_INFO << "getImageAndGetFingerId error: " << templateHex;
        mMessageRet = createJsonResponse("error", templateHex);
        return mMessageRet.c_str();
    }

    std::string messages_base;
    clusterVal(ipAddress, port, appId, templateHex, [&messages_base](bool success, const std::string& message) {

        if (success) {
            LOG_INFO << "clusterReg success: " << message;
            messages_base = message;
        }
        else {
            LOG_SYSERR << "clusterReg - Operation failed: " << message;
            messages_base = message;
        }
        });

    mMessageRet = messages_base;

    return mMessageRet.c_str();
}



const char* getImageAndGetFingerId(unsigned char* imageData, int width, int height, const char* ipAddress, const char* port, const char* appId, const bool isLog)
{
    std::string templateHex;

   // templateHex = convertImageToTemplate(imageData, width * height, isLog);
    if (!convertImageToTemplateBool(imageData, width, height, isLog, templateHex))
    {
        LOG_INFO << "getImageAndGetFingerId error: " << templateHex;
        mMessageRet = createJsonResponse("error", templateHex);
        return mMessageRet.c_str();
    }
   
    std::string messages_base;
    clusterVal(ipAddress, port, appId, templateHex, [&messages_base](bool success, const std::string& message) {

        if (success) {
            LOG_INFO << "clusterReg success: " << message;
            messages_base = message;
        }
        else {
            LOG_SYSERR << "clusterReg - Operation failed: " << message;
            messages_base = message;
        }
        });

    mMessageRet = messages_base;

    return mMessageRet.c_str();
}

const char* startTemplateRegistration(const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId, const char* param, const char* templateHex, const bool isLog)
{
    initLog(isLog);

      std::lock_guard<std::mutex> lock(g_mutex); // Lock the mutex for the duration of this scope
    //unsigned int bufferSize = 400 * 400;
    //unsigned char* buffer = new unsigned char[bufferSize];
    //std::memset(buffer, 0, bufferSize);
    std::string cause;

    mMessageRet.clear();
    mStage = STAGE_IDLE;
    cv::Mat fingerprintImage;

    LOG_INFO << "Ready to scan a fingerprint for registration.";

    std::string ip = ipAddress;
    std::string mport = port;
    std::string mappId = appId;
    std::string finger_ = finger;
    std::string mreturnId = returnId;
    std::string minutiae2 = "123123";
    std::string mtemplateHex = templateHex;
    std::string mparam = param;

  /*  if (!validateInputReg(ip, mport, mappId, mreturnId, minutiae2, cause)) {
        std::string retstr = "Validation fail. Cause: " + cause;
        LOG_ERROR << retstr;
        mMessageRet = createJsonResponse("error", retstr);
        return mMessageRet.c_str();
    }

    mStage = STAGE_SENDING;*/
    std::string messages_base;
    clusterRegTemplate(ip, mport, mappId, finger_, mreturnId, mparam, mtemplateHex, [&messages_base](bool success, const std::string& message) {

        if (success) {
            LOG_INFO << "clusterReg success: " << message;
            messages_base = message;
        }
        else {
            LOG_SYSERR << "clusterReg - Operation failed: " << message;
            messages_base = message;
        }
        });

    mMessageRet = messages_base;

    return mMessageRet.c_str();
}
