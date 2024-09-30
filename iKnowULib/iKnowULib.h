// MyFunctions.h (Header file)
#ifdef MYFUNCTIONS_EXPORTS
#define MYFUNCTIONS_API __declspec(dllexport)
#else
#define MYFUNCTIONS_API __declspec(dllimport)
#endif

//extern "C" MYFUNCTIONS_API const char* GetHelloMessage();
//
//extern "C" MYFUNCTIONS_API void capturedFinger();

extern "C" MYFUNCTIONS_API const char* cancelRequest();

extern "C" MYFUNCTIONS_API const char* startScanAndGetFingerID(const char* ipAddress, const char* port, const char* appId, const bool isLog);
extern "C" MYFUNCTIONS_API const char* startScanAndRegisterFingerID(const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId, const char* minutiae, const bool isLog);

extern "C" MYFUNCTIONS_API const char* startScan(const char* ipAddress, const char* port, const char* appId, const char* finger_, const char* returnId, const bool isLog);

// not in use
//extern "C" MYFUNCTIONS_API const char* startScanAndRegisterFingerIDx(const char* numCaptured, const char* ipAddress, const char* port, const char* appId, const char* returnId);
extern "C" MYFUNCTIONS_API const char* startScanAndGetFingerIDx(const char* numCaptured, const char* ipAddress, const char* port, const char* appId);
//
//extern "C" MYFUNCTIONS_API const char* startScanAndGetFingerIDn(const char* ipAddress, const char* port, const char* appId);
extern "C" MYFUNCTIONS_API const char* startScanAndRegisterFingerIDx(const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId);

extern "C" MYFUNCTIONS_API const char* startScanAndRegisterFingerIDf(const char* filename, const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId, const bool isLog);

extern "C" MYFUNCTIONS_API const  char* convertImageToTemplate( unsigned char* imageData, int size, const bool isLog);
extern "C" MYFUNCTIONS_API const char* getImageAndGetFingerId(unsigned char* imageData, int width, int height, const char* ipAddress, const char* port, const char* appId, const bool isLog);
extern "C" MYFUNCTIONS_API const  char* convertImageToTemplateWithId(unsigned char* imageData, int size, int id, const bool isLog);

// register via template with param
// e.g. ,"name":"Juan dele Cruz"
extern "C" MYFUNCTIONS_API const char* startTemplateRegistration(const char* ipAddress, const char* port, const char* appId, const char* finger, const char* returnId, const char* param, const char* templateHex, const bool isLog);