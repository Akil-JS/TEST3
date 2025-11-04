// #include "LogManager.h"
// #include "DebugConfig.h"
// #include "DebugMacro.h"
// #include <SD.h>
// #include <Preferences.h>

// // Note: This file no longer references 'fileMutex' or other external variables.

// LogManager::LogManager() : processingInProgress(false) {}

// void LogManager::initialize(const char* name, const char* f1, const char* f2, const char* prefs_ns, SendDataCallback cb) {
//     logName = name;
//     file1Path = f1;
//     file2Path = f2;
//     prefsNamespace = prefs_ns;
//     sendDataCallback = cb;
    
//     // Each LogManager gets its own Preferences object
//     preferences.begin(prefsNamespace.c_str(), false);
//     currentWriteFile = preferences.getString("writeFile", file1Path.c_str());
//     preferences.end();
    
//     // The file to be read is always the one we are NOT currently writing to.
//     currentReadFile = (currentWriteFile == file1Path) ? file2Path : file1Path;

//     DEBUG("LogManager [" + logName + "] initialized. Writing to: " + currentWriteFile);
// }

// void LogManager::saveState() {
//     preferences.begin(prefsNamespace.c_str(), false);
//     preferences.putString("writeFile", currentWriteFile.c_str());
//     preferences.end();
// }

// void LogManager::switchFiles() {
//     currentWriteFile = (currentWriteFile == file1Path) ? file2Path : file1Path;
//     currentReadFile = (currentWriteFile == file1Path) ? file2Path : file1Path;
//     saveState();
//     DEBUG("LogManager [" + logName + "] switched files. Now writing to: " + currentWriteFile);
// }

// void LogManager::saveLog(const String& logData) {
//     File dataFile = SD.open(currentWriteFile.c_str(), FILE_APPEND);
//     if (dataFile) {
//         dataFile.println(logData);
//         dataFile.close();
//     } else {
//         DEBUG("LogManager [" + logName + "] FAILED to open " + currentWriteFile + " for writing.");
//     }
// }

// // --- CORRECTED: This function now only uses its own member variables ---
// // It no longer handles the mutex; that is the responsibility of the sdcard class.
// void LogManager::clearLogs() {
//     DEBUG("LogManager [" + logName + "]: Clearing log files...");

//     // Delete the first log file if it exists
//     if (SD.exists(file1Path.c_str())) {
//         if (SD.remove(file1Path.c_str())) {
//             DEBUG("  - Deleted: " + file1Path);
//         } else {
//             DEBUG("  - FAILED to delete: " + file1Path);
//         }
//     }

//     // Delete the second log file if it exists
//     if (SD.exists(file2Path.c_str())) {
//         if (SD.remove(file2Path.c_str())) {
//             DEBUG("  - Deleted: " + file2Path);
//         } else {
//             DEBUG("  - FAILED to delete: " + file2Path);
//         }
//     }

//     // Reset the internal state back to the default file and save it
//     currentWriteFile = file1Path;
//     saveState();
// }


// void LogManager::processAndSendData() {
//     if (processingInProgress || !SD.exists(currentReadFile.c_str())) {
//         return;
//     }

//     File readFile = SD.open(currentReadFile.c_str(), FILE_READ);
//     if (!readFile || readFile.size() == 0) {
//         if (readFile) readFile.close();
//         if (SD.exists(currentReadFile.c_str())) {
//             // If the file is empty, delete it and switch to prevent it from being checked again.
//             SD.remove(currentReadFile.c_str());
//             switchFiles();
//         }
//         return;
//     }

//     processingInProgress = true;
//     DEBUG("LogManager [" + logName + "] Processing offline data from: " + currentReadFile);

//     String payload = "[";
//     bool firstEntry = true;
    
//     while (readFile.available()) {
//         String line = readFile.readStringUntil('\n');
//         line.trim();
//         if (line.length() > 0) {
//             if (!firstEntry) {
//                 payload += ",";
//             }
//             payload += line;
//             firstEntry = false;
//         }
//     }
//     payload += "]";
//     readFile.close();

//     if (!firstEntry) {
//         bool success = sendDataCallback(payload);
//         if (success) {
//             DEBUG("LogManager [" + logName + "] Batch data sent successfully. Deleting file: " + currentReadFile);
//             SD.remove(currentReadFile.c_str());
//             switchFiles();
//         } else {
//             DEBUG("LogManager [" + logName + "] Failed to send batch data. Will retry later.");
//         }
//     } else {
//        SD.remove(currentReadFile.c_str());
//        switchFiles();
//     }

//     processingInProgress = false;
// }

#include "LogManager.h"
#include "DebugConfig.h"
#include "DebugMacro.h"
#include <SD.h>
#include <Preferences.h>

LogManager::LogManager() : processingInProgress(false), devsbotInstance(nullptr) {}

// void LogManager::initialize(const char* name, const char* f1, const char* f2, const char* prefs_ns, SendDataCallback cb) {
//     logName = name;
//     file1Path = f1;
//     file2Path = f2;
//     prefsNamespace = prefs_ns;
//     sendDataCallback = cb;

//     preferences.begin(prefsNamespace.c_str(), false);
//     currentWriteFile = preferences.getString("writeFile", file1Path.c_str());
//     preferences.end();

//     currentReadFile = (currentWriteFile == file1Path) ? file2Path : file1Path;

//     DEBUG("LogManager [" + logName + "] initialized. Writing to: " + currentWriteFile + ", Reading from: " + currentReadFile); // Added read file info
// }

// --- MODIFICATION: Store the devsbotRef pointer ---
void LogManager::initialize(Devsbot* devsbotRef, const char* name, const char* f1, const char* f2, const char* prefs_ns, SendDataCallback cb) {
    devsbotInstance = devsbotRef; // <-- STORE THE POINTER
    logName = name;
    file1Path = f1;
    file2Path = f2;
    prefsNamespace = prefs_ns;
    sendDataCallback = cb;

    preferences.begin(prefsNamespace.c_str(), false);
    currentWriteFile = preferences.getString("writeFile", file1Path.c_str());
    preferences.end();

    currentReadFile = (currentWriteFile == file1Path) ? file2Path : file1Path;

    DEBUG("LogManager [" + logName + "] initialized. Writing to: " + currentWriteFile + ", Reading from: " + currentReadFile);
}


void LogManager::saveState() {
    preferences.begin(prefsNamespace.c_str(), false);
    preferences.putString("writeFile", currentWriteFile.c_str());
    preferences.end();
}

void LogManager::switchFiles() {
    // Determine the new write file (toggle)
    String newWriteFile = (currentWriteFile == file1Path) ? file2Path : file1Path;
    String newReadFile = currentWriteFile; // The old write file becomes the new read file

    currentWriteFile = newWriteFile;
    currentReadFile = newReadFile;

    saveState(); // Save the *new* write file state
    DEBUG("LogManager [" + logName + "] switched files. Now writing to: " + currentWriteFile + ", Reading from: " + currentReadFile); // Updated log
}

void LogManager::saveLog(const String& logData) {
    File dataFile = SD.open(currentWriteFile.c_str(), FILE_APPEND);
    if (dataFile) {
        // DEBUG("LogManager [" + logName + "] Saving: " + logData); // Optional: Verbose log for writes
        dataFile.println(logData);
        dataFile.close();
    } else {
        DEBUG("LogManager [" + logName + "] FAILED to open " + currentWriteFile + " for writing.");
    }
}

void LogManager::clearLogs() {
    DEBUG("LogManager [" + logName + "]: Clearing log files...");

    if (SD.exists(file1Path.c_str())) {
        if (!SD.remove(file1Path.c_str())) {
             DEBUG("  - FAILED to delete: " + file1Path);
        } else {
             DEBUG("  - Deleted: " + file1Path); // Log success
        }
    }
    if (SD.exists(file2Path.c_str())) {
        if (!SD.remove(file2Path.c_str())) {
             DEBUG("  - FAILED to delete: " + file2Path);
        } else {
             DEBUG("  - Deleted: " + file2Path); // Log success
        }
    }

    // Reset to writing file 1 after clearing
    currentWriteFile = file1Path;
    currentReadFile = file2Path; // Set corresponding read file
    saveState();
    DEBUG("LogManager [" + logName + "] state reset after clear. Writing to: " + currentWriteFile + ", Reading from: " + currentReadFile);
}


// void LogManager::processAndSendData() {
//     // --- Added Detailed Logging ---
//     DEBUG("LogManager [" + logName + "] processAndSendData called.");
//     DEBUG("  - Current Read File: " + currentReadFile);
//     DEBUG("  - Processing In Progress? " + String(processingInProgress ? "Yes" : "No"));

//     if (processingInProgress) {
//         DEBUG("  - Exiting: Already processing.");
//         return;
//     }

//     bool fileExists = SD.exists(currentReadFile.c_str());
//     DEBUG("  - Read File Exists? " + String(fileExists ? "Yes" : "No"));

//     if (!fileExists) {
//          DEBUG("  - Exiting: Read file does not exist.");
//          // If the intended read file doesn't exist, maybe switch anyway?
//          // Otherwise, if the write file fills up, it might never switch back.
//          // Let's try switching if the read file is missing.
//          DEBUG("  - Read file missing, attempting to switch files.");
//          switchFiles();
//          return;
//     }

//     File readFile = SD.open(currentReadFile.c_str(), FILE_READ);
//     if (!readFile) {
//         DEBUG("  - Exiting: FAILED to open read file: " + currentReadFile);
//         return; // Can't proceed if file won't open
//     }

//     size_t fileSize = readFile.size();
//     DEBUG("  - Read File Size: " + String(fileSize) + " bytes");

//     if (fileSize == 0) {
//         readFile.close();
//         DEBUG("  - Read file is empty. Deleting and switching.");
//         SD.remove(currentReadFile.c_str()); // Remove empty file
//         switchFiles();
//         return;
//     }

//     // --- Processing Start ---
//     processingInProgress = true;
//     DEBUG("LogManager [" + logName + "] Processing offline data from: " + currentReadFile);

//     String payload = "[";
//     bool firstEntry = true;
//     unsigned long lineCount = 0; // Count lines read

//     while (readFile.available()) {
//         String line = readFile.readStringUntil('\n');
//         line.trim(); // Important to remove potential trailing \r or spaces
//         if (line.length() > 0) {
//             if (!firstEntry) {
//                 payload += ",";
//             }
//             payload += line;
//             firstEntry = false;
//             lineCount++;
//         }
//     }
//     payload += "]";
//     readFile.close(); // Close file ASAP

//     DEBUG("  - Read " + String(lineCount) + " lines from file.");

//     if (firstEntry) {
//         // This case means the file had content, but it was likely just whitespace
//         DEBUG("  - File contained only whitespace? Deleting and switching.");
//         SD.remove(currentReadFile.c_str());
//         switchFiles();
//     } else {
//         DEBUG("  - Calling sendDataCallback for " + logName + "...");
//         bool success = sendDataCallback(payload); // Call the specific send function
//         DEBUG("  - sendDataCallback result: " + String(success ? "Success" : "Failed"));

//         if (success) {
//             DEBUG("  - Batch data sent successfully. Deleting file: " + currentReadFile);
//             if(SD.remove(currentReadFile.c_str())) {
//                  DEBUG("  - File deleted successfully.");
//             } else {
//                  DEBUG("  - FAILED to delete file after sending!");
//             }
//             switchFiles(); // Switch to the other file for next read/write cycle
//         } else {
//             DEBUG("  - Failed to send batch data. File kept for retry: " + currentReadFile);
//             // Do NOT switch files if send failed
//         }
//     }

//     processingInProgress = false; // Release lock
//     DEBUG("LogManager [" + logName + "] processAndSendData finished.");
//     // --- End Added Logging ---
// }



void LogManager::processAndSendData() {
    DEBUG("LogManager [" + logName + "] processAndSendData called.");

    if (processingInProgress) {
        DEBUG("  - Exiting: Already processing.");
        return;
    }

    if (!SD.exists(currentReadFile.c_str())) {
        DEBUG("  - Read file missing, attempting to switch files.");
        switchFiles();
        return;
    }

    File readFile = SD.open(currentReadFile.c_str(), FILE_READ);
    if (!readFile) {
        DEBUG("  - Exiting: FAILED to open read file: " + currentReadFile);
        return;
    }

    if (readFile.size() == 0) {
        readFile.close();
        DEBUG("  - Read file is empty. Deleting and switching.");
        SD.remove(currentReadFile.c_str());
        switchFiles();
        return;
    }

    processingInProgress = true;
    DEBUG("LogManager [" + logName + "] Processing offline data from: " + currentReadFile);

    // --- NEW BATCHING LOGIC (v3) ---
    // Use a *record limit* instead of an unreliable memory size check.
    // 10 records is very safe.
    const int MAX_RECORDS_PER_BATCH = 10; 
    
    bool fileFullyProcessed = false;
    bool sendSuccess = true;
    String line;
    
    // We loop as long as the file has data AND our last send was successful
    while (readFile.available() && sendSuccess) {
        DynamicJsonDocument batchDoc(4096); // 4KB is fine for the buffer
        JsonArray dataArray;
        bool batchHasData = false;
        String payload;

        // --- Setup the correct JSON structure based on Log Type ---
        if (logName == "Energy") {
            // Energy data is just a root-level array
            dataArray = batchDoc.to<JsonArray>(); 
        } else {
            // DI Status and Pulse get the new object format
            batchDoc["slave_id"] = 1; 
            if (devsbotInstance) {
                batchDoc["gateway_api_id"] = devsbotInstance->getAuthToken();
            }
            if (logName == "DI_Status") {
                dataArray = batchDoc.createNestedArray("DIStatusOffline");
            } else if (logName == "DI_Pulse") {
                dataArray = batchDoc.createNestedArray("DIPulseOffline");
            }
        }

        // --- Fill the batch (up to MAX_RECORDS_PER_BATCH) ---
        while (readFile.available()) {
            line = readFile.readStringUntil('\n');
            line.trim();
            
            if (line.length() > 0) {
                // This logic correctly handles both formats
                if (logName == "Energy" && line.startsWith("[")) {
                    DynamicJsonDocument lineDoc(1024);
                    if (deserializeJson(lineDoc, line) == DeserializationError::Ok) {
                        // Add the entire array as a single element
                        dataArray.add(lineDoc.as<JsonArray>());
                        batchHasData = true;
                    }
                } else if (line.startsWith("{")) {
                    DynamicJsonDocument lineDoc(512);
                    if (deserializeJson(lineDoc, line) == DeserializationError::Ok) {
                        dataArray.add(lineDoc.as<JsonObject>());
                        batchHasData = true;
                    }
                }
            }

            // --- THIS IS THE NEW CHECK ---
            // Check the *number of records* in the batch
            if (dataArray.size() >= MAX_RECORDS_PER_BATCH) {
                DEBUG("  - Batch record limit (" + String(MAX_RECORDS_PER_BATCH) + ") reached. Sending current batch...");
                break; // Stop filling this batch and send it
            }
            // --- END OF NEW CHECK ---
            
        } // End while(readFile.available()) for this batch

        // --- Send the completed batch ---
        if (batchHasData) {
            
            if (logName == "Energy") {
                // This is the fix for the "double payload" bug
                serializeJson(batchDoc, payload); // payload is just [...]
            } else {
                serializeJson(batchDoc, payload); // payload is {"slave_id":...}
            }
            
            DEBUG("  - Calling sendDataCallback for " + logName + " (Records: " + String(dataArray.size()) + ")");
            sendSuccess = sendDataCallback(payload); 

            if (!sendSuccess) {
                DEBUG("  - FAILED to send batch. Will retry file later.");
            } else {
                 DEBUG("  - Batch send SUCCESS.");
            }
        } else if (readFile.available()) {
            // This case should not be hit, but as a safety check
            sendSuccess = true; 
        } else {
            // File was readable but contained no valid data
            DEBUG("  - File contained no valid JSON data.");
            sendSuccess = true; 
        }

        if (!readFile.available()) {
            fileFullyProcessed = true;
        }

    } // End while(readFile.available() && sendSuccess)

    readFile.close();

    // If the entire file was processed AND all sends were successful, delete it.
    if (fileFullyProcessed && sendSuccess) {
        DEBUG("  - File fully processed. Deleting file: " + currentReadFile);
        if(SD.remove(currentReadFile.c_str())) {
             DEBUG("  - File deleted successfully.");
        } else {
             DEBUG("  - FAILED to delete file after processing!");
        }
        switchFiles(); 
    }

    processingInProgress = false;
    DEBUG("LogManager [" + logName + "] processAndSendData finished.");
}
// --- END: Replace the entire processAndSendData function ---

