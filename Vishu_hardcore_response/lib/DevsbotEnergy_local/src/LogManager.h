#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include <functional>

// Forward declaration of Devsbot class to avoid circular includes
class Devsbot; 
// Define the function type for sending data to the server
// It takes a String payload and returns true on success
using SendDataCallback = std::function<bool(String&)>;

class LogManager {
public:
    LogManager();
    // --- MODIFICATION: Added Devsbot* devsbotRef as the first parameter ---
    void initialize(Devsbot* devsbotRef, const char* logName, const char* file1, const char* file2, const char* prefsNamespace, SendDataCallback callback);
    void saveLog(const String& logData);
    void processAndSendData();
    // Clears both log files for this manager.
    void clearLogs();

private:
    // Configuration
    String logName;
    String file1Path;
    String file2Path;
    String prefsNamespace;
    SendDataCallback sendDataCallback;

    // --- NEW: Add a pointer to the main Devsbot instance ---
    Devsbot* devsbotInstance;
    // State
    String currentWriteFile;
    String currentReadFile;
    Preferences preferences;
    bool processingInProgress;

    // Private Methods
    void switchFiles();
    void loadState();
    void saveState();
};

#endif // LOG_MANAGER_H

