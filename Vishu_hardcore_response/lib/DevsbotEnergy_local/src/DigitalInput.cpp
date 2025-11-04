#include "DigitalInput.h"
#include "sdcard.h"
#include "DevsbotEnergyLocal.h" // Required to access the Devsbot class definition
#include "HardwareRTC.h"      // <-- ADDED: Include the new RTC manager
#include "DebugMacro.h"

// Make the global 'card' object (which contains our LogManagers) available in this file.
extern sdcard card;

// --- CONSTANTS for Debouncing ---
const unsigned long DEBOUNCE_DELAY_MS = 50; // 50 milliseconds

// Constructor
DigitalInput::DigitalInput() {
    statusInputCount = 0;
    pulseInputCount = 0;
    jobEnabled = false;
    pulseCountEnabled = false;
    resetPulseCount = false;
    currentPulseCount = 0;
    lastPulseCount = 0;
    pcntCounterPin = -1;
    sensorTaskHandle = NULL;
    devsbotInstance = nullptr;
    
    digitalInputPin = "";
    widgetData = "";

    // --- Initialize wear-leveling variables ---
    lastPulseSaveTime = 0;
    pulseDataDirty = false;

        // --- START MODIFICATION ---
    currentJobId = 0; // Default to 0
    // --- END MODIFICATION ---
    lastOfflinePulseCount = -1; // Initialize to -1 to force first save

    for (int i = 0; i < 20; i++) {
        statusChanged[i] = false;
        statusStates[i] = 0;
    }
}


// Destructor
DigitalInput::~DigitalInput() {
    stopSensorTask();
}

// SIMPLIFIED: We only need a reference to the main Devsbot instance now.
void DigitalInput::initialize(Devsbot* devsbotRef) {
    devsbotInstance = devsbotRef;
}

// // <-- MODIFIED: This function now gets the correct time from the rtcManager.
// // Function to get current timestamp in required format
// String DigitalInput::getCurrentTimestamp() {
//     return rtcManager.getTimestamp();
// }


// --- THIS IS THE FIX ---
// This function now formats the LOCAL time from the RTC manager, not UTC.
String DigitalInput::getCurrentTimestamp() {
    DateTime localTime = rtcManager.now(); // Get the local DateTime object
    char timestampBuffer[20];
    
    // Format the LOCAL time into the "YYYY-MM-DD HH:MM:SS" format.
    sprintf(timestampBuffer, "%04d-%02d-%02d %02d:%02d:%02d", 
            localTime.year(), 
            localTime.month(), 
            localTime.day(), 
            localTime.hour(), 
            localTime.minute(), 
            localTime.second());
            
    return String(timestampBuffer);
}
// --- END OF FIX ---



// --- REPLACE your initializePCNT function with this final version ---commeneted on 24/10/2025
// The only change is to re-enable the hardware noise filter.
// Function to initialize PCNT for pulse counting 
// void DigitalInput::initializePCNT(int pin) {
//     // 1. Configure all hardware settings
//     pcnt_config_t pcntConfig = {};
//     pcntConfig.pulse_gpio_num = pin;
//     pcntConfig.ctrl_gpio_num = PCNT_PIN_NOT_USED;
//     pcntConfig.unit = PCNT_UNIT;
//     pcntConfig.channel = PCNT_CHANNEL;
//     pcntConfig.pos_mode = PCNT_COUNT_DIS;  // Ignore rising edge
//     pcntConfig.neg_mode = PCNT_COUNT_INC;  // Count falling edge (HIGH to LOW)
//     pcntConfig.lctrl_mode = PCNT_MODE_KEEP;
//     pcntConfig.hctrl_mode = PCNT_MODE_KEEP;
//     pcntConfig.counter_h_lim = PCNT_H_LIM_VAL;
//     pcntConfig.counter_l_lim = PCNT_L_LIM_VAL;

//     pcnt_unit_config(&pcntConfig);

//     // 2. Enable the hardware filter to prevent noise/bounce
//     pcnt_set_filter_value(PCNT_UNIT, 1023);
//     pcnt_filter_enable(PCNT_UNIT);
    
//     // 3. Reset the hardware counter
//     pcnt_counter_pause(PCNT_UNIT);
//     pcnt_counter_clear(PCNT_UNIT);
//     pcnt_counter_resume(PCNT_UNIT);

//     pcntCounterPin = pin;

//     // 4. Synchronize the software's memory with the hardware's state
//     // This does NOT reset your total saved pulse count.
//     lastPulseCount = 0;

//     Serial.println(" PCNT initialized for pulse pin " + String(pin));
// }

void DigitalInput::initializePCNT(int pin) {
    // 1. Configure all hardware settings
    pcnt_config_t pcntConfig = {};
    pcntConfig.pulse_gpio_num = pin;
    pcntConfig.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    pcntConfig.unit = PCNT_UNIT;
    pcntConfig.channel = PCNT_CHANNEL;
    pcntConfig.pos_mode = PCNT_COUNT_DIS;
    pcntConfig.neg_mode = PCNT_COUNT_INC;
    pcntConfig.lctrl_mode = PCNT_MODE_KEEP;
    pcntConfig.hctrl_mode = PCNT_MODE_KEEP;
    pcntConfig.counter_h_lim = PCNT_H_LIM_VAL;
    pcntConfig.counter_l_lim = PCNT_L_LIM_VAL;

    pcnt_unit_config(&pcntConfig);

    // 2. Enable the hardware filter
    pcnt_set_filter_value(PCNT_UNIT, 1023);
    pcnt_filter_enable(PCNT_UNIT);

    // 3. Pause, clear, and resume the counter
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    
    // *** CRITICAL FIX: Add a small delay to ensure clear completes ***
    delayMicroseconds(100); // Give hardware time to actually clear
    
    pcnt_counter_resume(PCNT_UNIT);

    pcntCounterPin = pin;

    // 4. Read the hardware counter AFTER clear and set both variables to match
    pcnt_get_counter_value(PCNT_UNIT, &currentPulseCount);
    lastPulseCount = currentPulseCount;
    
    Serial.printf("PCNT initialized for pin %d. Hardware counter cleared and synced. "
                  "currentPulseCount=%d, lastPulseCount=%d\n", 
                  pin, currentPulseCount, lastPulseCount);
}

// === REVISED: This is the updated function to handle the SERVER's JSON format ===
void DigitalInput::widgetPinInitialize(const String& widgetJsonData) {
    Serial.println("DigitalInput widgetPinInitialize begin");
    digitalInputPin = "";
    statusInputCount = 0;
    pulseInputCount = 0;

    // --- THIS IS THE FIX: Load the last known job state from persistent storage ---
    jobStatePrefs.begin("DI_JobState", true); // Open in read-only mode first
    jobEnabled = jobStatePrefs.getBool("jobEnabled", false); // Default to false if not found
    currentJobId = jobStatePrefs.getInt("job_id", 0);
    pulseCountEnabled = jobEnabled; // Sync pulse counting with the loaded job state
    jobStatePrefs.end();
    Serial.println("Loaded persistent job state. Job Enabled: " + String(jobEnabled ? "Yes" : "No") + ", Job ID: " + String(currentJobId));

    // --- END OF FIX ---
    if (!widgetJsonData.isEmpty()) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, widgetJsonData);

        if (!error) {
            for (JsonVariant elem : doc.as<JsonArray>()) {
                String datastreamName = elem["datastream_name"];
                String pinModeStr = elem["pinmode"];

                if (datastreamName == "Digital") {
                    JsonArray pinArray = elem["pin"];
                    
                    // --- THIS IS THE FIX ---
                    // Read the arrays using the keys that the server is actually sending.
                    JsonArray inputMethodArray = elem["input_method"];
                    JsonArray typeStringArray = elem["type"]; // Changed from "type_id" to "type"
                    // --- END OF FIX ---

                    for (size_t i = 0; i < pinArray.size(); i++) {
                        int pin = pinArray[i];
                        String inputMethodStr = inputMethodArray[i].as<String>();
                        String typeStr = typeStringArray[i].as<String>();

                        digitalInputPin += String(pin) + (i < pinArray.size() - 1 ? "," : "");

                        if (inputMethodStr == "status") {
                            // Map the type string (e.g., "heater") to an internal enum
                            int mappedTypeName = 0;
                            if (typeStr == "motor") mappedTypeName = MOTOR;
                            else if (typeStr == "heater") mappedTypeName = HEATER;
                            else if (typeStr == "machine") mappedTypeName = MACHINE;
                            
                            if (mappedTypeName == 0) {
                                Serial.println("Warning: Unknown status type '" + typeStr + "' for pin " + String(pin));
                                continue;
                            }

                            if (pinModeStr == "INPUT") pinMode(pin, INPUT);
                            else if (pinModeStr == "INPUT_PULLUP") pinMode(pin, INPUT_PULLUP);
                            
                            statusInputs[statusInputCount].pin = pin;
                            statusInputs[statusInputCount].type_name = mappedTypeName;
                            statusInputs[statusInputCount].type_id = TYPE_STATUS;
                            
                            // statusInputs[statusInputCount].currentState = digitalRead(pin);
                            statusInputs[statusInputCount].currentState = !digitalRead(pin);

                            statusInputs[statusInputCount].lastState = statusInputs[statusInputCount].currentState;
                            statusInputs[statusInputCount].stateChanged = false;
                            statusInputs[statusInputCount].lastChangeTime = "";
                            statusInputs[statusInputCount].lastDebounceTime = 0;
                            
                            Serial.println("Status Input Pin=> " + String(pin) + " Type: " + typeStr);
                            statusInputCount++;

                        // --- THIS IS THE SECOND FIX ---
                        // Check for "pcount" (lowercase) to match the server's response.
                        } else if (inputMethodStr == "pcount") {
                        // --- END OF FIX ---
                            pinMode(pin, INPUT_PULLUP); // Pulse pins should default to pullup for falling edge
                            //pinMode(pin, INPUT); // Pulse pins should default to pullup for falling edge
                            
                            
                            pulseInputs[pulseInputCount].pin = pin;
                            pulseInputs[pulseInputCount].type_name = PULSE;
                            pulseInputs[pulseInputCount].type_id = TYPE_PULSE_COUNT;
                            // ... (rest of pulse config) ...
                            pulseInputs[pulseInputCount].pulseCount = 0;
                            pulseInputs[pulseInputCount].lastSentCount = 0;
                            pulseInputs[pulseInputCount].usePCNT = true;
                            pulseInputs[pulseInputCount].initialized = false;
                            
                            Serial.println("Pulse Input Pin=> " + String(pin) + " (Cycle Counter)");
                            pulseInputCount++;
                        }
                    }
                }
            }
            loadPulseCountsFromSPIFFS();
            for (int i = 0; i < pulseInputCount; i++) {
                if (pulseInputs[i].usePCNT) {
                    initializePCNT(pulseInputs[i].pin);
                    pulseInputs[i].initialized = true;
                    
                    // *** ADD THIS: Explicitly prevent phantom pulses on reconnection ***
                    // Reset the baseline to ignore any pulses during WiFi disconnection
                    currentPulseCount = 0;
                    lastPulseCount = 0;
                    
                    Serial.printf("Pulse input on pin %d initialized. "
                                "Saved count from SPIFFS: %d (will continue from here)\n",
                                pulseInputs[i].pin, pulseInputs[i].pulseCount);
                    break;
                }
            }

        } else {
            Serial.println("Error: Failed to parse widget JSON data.");
        }
    } else {
        Serial.println("Warning: No widget data provided for initialization.");
    }
    Serial.println("DigitalInput widgetPinInitialize End");
}




// Widget API function
bool DigitalInput::widgetAPI() {
    Serial.println("DigitalInput widgetAPI begin");
    
    // NOTE: This function should not contain the actual API call
    // The API call should be made from DevsbotEnergyLocal.cpp
    // This function just processes the received widget data
    
    // For now, return true - the actual API call will be handled by DevsbotEnergyLocal
    Serial.println("DigitalInput widgetAPI End");
    return true;
}

// --- MODIFIED: Implemented Power-Fail-Safe (Atomic) Write ---
void DigitalInput::savePulseCountsToSPIFFS() {
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.createNestedArray("pulse_counts");

    for (int i = 0; i < pulseInputCount; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["pin"] = pulseInputs[i].pin;
        obj["count"] = pulseInputs[i].pulseCount;
        obj["last_sent"] = pulseInputs[i].lastSentCount;
    }

    // 1. Write the new data to a temporary file. This is the "journaling" step.
    File tempFile = SPIFFS.open("/pulse_counts_new.json", "w");
    if (!tempFile) {
        DEBUG("--> ERROR: Failed to open temporary file for pulse count save.");
        return;
    }
    
    if (serializeJson(doc, tempFile) == 0) {
        DEBUG("--> ERROR: Failed to write to temporary pulse count file.");
        tempFile.close();
        return;
    }
    tempFile.close(); // The new data is now safely on disk in the temp file.

    // 2. Perform the "atomic" switch: delete the old file and rename the new one.
    SPIFFS.remove("/pulse_counts.json"); 
    
    if (SPIFFS.rename("/pulse_counts_new.json", "/pulse_counts.json")) {
        DEBUG("Pulse counts saved to SPIFFS (Power-Fail-Safe).");
    } else {
        DEBUG("--> ERROR: Failed to rename pulse count file.");
    }
}


// --- MODIFIED: Implemented Power-Fail Recovery Logic ---
void DigitalInput::loadPulseCountsFromSPIFFS() {
    // On boot, check for a leftover temp file, which indicates a power failure during the last save.
    if (SPIFFS.exists("/pulse_counts_new.json")) {
        DEBUG("--> RECOVERY: Power loss detected. Attempting to recover from temp file.");
        SPIFFS.remove("/pulse_counts.json"); 
        SPIFFS.rename("/pulse_counts_new.json", "/pulse_counts.json");
    }

    if (!SPIFFS.exists("/pulse_counts.json")) {
        DEBUG("No pulse count file found. Starting from 0.");
        return;
    }

    File file = SPIFFS.open("/pulse_counts.json", "r");
    if (!file) return;

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        DEBUG("--> WARNING: Pulse count file is corrupt. Data may be lost.");
        return;
    }

    JsonArray arr = doc["pulse_counts"];
    for (JsonObject obj : arr) {
        int pin = obj["pin"];
        for (int i = 0; i < pulseInputCount; i++) {
            if (pulseInputs[i].pin == pin) {
                pulseInputs[i].pulseCount = obj["count"] | 0;
                pulseInputs[i].lastSentCount = obj["last_sent"] | 0;
                if (pulseInputs[i].pin == pcntCounterPin) {
                    currentPulseCount = pulseInputs[i].pulseCount;
                    lastPulseCount = currentPulseCount;
                }
                break;
            }
        }
    }
    Serial.println("Pulse counts loaded from SPIFFS");
}



// --- MODIFIED: This function now calls the new, more specific clear function ---
void DigitalInput::resetAllPulseCounts() {
    // 1. Reset all the in-memory (RAM) variables to zero.
    for (int i = 0; i < pulseInputCount; i++) {
        pulseInputs[i].pulseCount = 0;
        pulseInputs[i].lastSentCount = 0;
    }
    
    // 2. Reset the physical hardware counter on the chip.
    if (pcntCounterPin != -1) {
        pcnt_counter_pause(PCNT_UNIT);
        pcnt_counter_clear(PCNT_UNIT);
        pcnt_counter_resume(PCNT_UNIT);
        currentPulseCount = 0;
        lastPulseCount = 0;
    }
    
    // 3. Explicitly remove the persistent state file from SPIFFS.
    if (SPIFFS.exists("/pulse_counts.json")) {
        if (SPIFFS.remove("/pulse_counts.json")) {
            Serial.println("Pulse count state file erased from SPIFFS.");
        } else {
            Serial.println("Error: Failed to erase pulse count state file from SPIFFS.");
        }
    }
    
    // --- THIS IS THE CHANGE ---
    // 4. Also clear the DI-specific logs from the SD card.
    if (card.cardMounted) {
        card.clearDigitalInputLogs(); // Call the new, specific function
        card.statusLogs.clearLogs();    // Keep clearing DI Status logs
        clearOfflinePulseLog();    
    }
    // --- END OF CHANGE ---
    
    Serial.println("All pulse counts have been reset to 0.");
}

// // --- MODIFIED: Includes Debouncing Logic ---
// void DigitalInput::readStatusInputs() {
//     if (!jobEnabled) return;
//     for (int i = 0; i < statusInputCount; i++) {
//         int newState = digitalRead(statusInputs[i].pin);
//         if (newState != statusInputs[i].lastState && (millis() - statusInputs[i].lastDebounceTime) > DEBOUNCE_DELAY_MS) {
//             statusInputs[i].lastDebounceTime = millis();
//             statusInputs[i].currentState = newState;
//             statusInputs[i].lastState = newState;
//             statusInputs[i].stateChanged = true;
//             statusInputs[i].lastChangeTime = getCurrentTimestamp();
//             String typeName = (statusInputs[i].type_name == MOTOR) ? "Motor" : (statusInputs[i].type_name == HEATER) ? "Heater" : "Machine";
//             Serial.println("Status change detected - Pin: " + String(statusInputs[i].pin) + " (" + typeName + ") State: " + String(newState));
//             logStatusChange(statusInputs[i].pin, newState, statusInputs[i].lastChangeTime);
//         }
//     }
// }

// --- MODIFIED: Includes Debouncing Logic and uses RTC timestamp ---
void DigitalInput::readStatusInputs() {
    if (!jobEnabled) return;
    for (int i = 0; i < statusInputCount; i++) {
        //int newState = digitalRead(statusInputs[i].pin);
                // --- THIS IS THE FIX ---
        // Invert the logic using the '!' (NOT) operator.
        int newState = !digitalRead(statusInputs[i].pin);
        // --- END OF FIX ---
        // Simplified debouncing logic
        if (newState != statusInputs[i].lastState) {
            statusInputs[i].lastDebounceTime = millis();
        }

        if ((millis() - statusInputs[i].lastDebounceTime) > DEBOUNCE_DELAY_MS) {
            if (newState != statusInputs[i].currentState) {
                statusInputs[i].currentState = newState;
                statusInputs[i].lastState = newState; // Sync lastState here
                statusInputs[i].stateChanged = true;
                statusInputs[i].lastChangeTime = getCurrentTimestamp(); // Use the RTC-aware function
                String typeName = (statusInputs[i].type_name == MOTOR) ? "Motor" : (statusInputs[i].type_name == HEATER) ? "Heater" : "Machine";
                Serial.println("Status change detected - Pin: " + String(statusInputs[i].pin) + " (" + typeName + ") State: " + String(newState));
                logStatusChange(statusInputs[i].pin, newState, statusInputs[i].lastChangeTime, statusInputs[i].type_name);
            }
        }
        statusInputs[i].lastState = newState; // Update lastState on every check
    }
}



// --- FINAL VERSION ---
// This version is clean and only prints a message when a new pulse is detected.
// bool DigitalInput::readPulseInputs() {
//     if (!pulseCountEnabled) return false;
    
//     bool dataChanged = false;
//     for (int i = 0; i < pulseInputCount; i++) {
//         if (pulseInputs[i].usePCNT && pulseInputs[i].pin == pcntCounterPin) {
            
//             // Read the hardware counter's current value.
//             pcnt_get_counter_value(PCNT_UNIT, &currentPulseCount);
            
//             // Check if the hardware count has changed since the last time we looked.
//             if (currentPulseCount != lastPulseCount) {
                
//                 // The hardware counter has changed, so we have real pulses.
//                 // This logic correctly handles the 16-bit counter rolling over.
//                 int16_t newPulses;
//                 if (currentPulseCount < lastPulseCount) {
//                     newPulses = (PCNT_H_LIM_VAL - lastPulseCount) + currentPulseCount;
//                 } else {
//                     newPulses = currentPulseCount - lastPulseCount;
//                 }

//                 if (newPulses > 0) {
//                     // --- This print statement ONLY runs when a new pulse is detected ---
//                     Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses. New total: %d\n", 
//                                   newPulses, pulseInputs[i].pulseCount + newPulses);
                    
//                     pulseInputs[i].pulseCount += newPulses;
//                     dataChanged = true;
                    
//                     // Log the real pulse data to the SD card if enabled.
//                     logPulseData(pulseInputs[i].pin, pulseInputs[i].pulseCount);
//                 }
                
//                 // Update our software copy of the hardware's last known count.
//                 lastPulseCount = currentPulseCount;
//             }
//         }
//     }
    
//     // If the count changed, save the new total to SPIFFS for persistence.
//     if (dataChanged) {
//         savePulseCountsToSPIFFS();
//     }
    
//     return dataChanged;
// }

// // --- MODIFIED: This no longer saves to SPIFFS on every pulse --- 24/10/2025
// bool DigitalInput::readPulseInputs() {
//     if (!pulseCountEnabled) return false;
    
//     bool dataChanged = false;
//     for (int i = 0; i < pulseInputCount; i++) {
//         if (pulseInputs[i].usePCNT && pulseInputs[i].pin == pcntCounterPin) {
//             pcnt_get_counter_value(PCNT_UNIT, &currentPulseCount);
            
//             if (currentPulseCount != lastPulseCount) {
//                 int16_t newPulses;
//                 if (currentPulseCount < lastPulseCount) {
//                     newPulses = (PCNT_H_LIM_VAL - lastPulseCount) + currentPulseCount;
//                 } else {
//                     newPulses = currentPulseCount - lastPulseCount;
//                 }

//                 if (newPulses > 0) {
//                     Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses. New total: %d\n", newPulses, pulseInputs[i].pulseCount + newPulses);
//                     pulseInputs[i].pulseCount += newPulses;
//                     dataChanged = true;
//                     pulseDataDirty = true; // Set the dirty flag
//                     logPulseData(pulseInputs[i].pin, pulseInputs[i].pulseCount);
//                 }
//                 lastPulseCount = currentPulseCount;
//             }
//         }
//     }
    
//     return dataChanged;
// }

// // --- MODIFIED: Checks pulseCountEnabled BEFORE adding pulses ---
// bool DigitalInput::readPulseInputs() {
//     // Log the state *at the very beginning*
//     //Serial.printf("[readPulseInputs] Entry: pulseCountEnabled = %s\n", pulseCountEnabled ? "true" : "false");

//     if (!pulseCountEnabled) {
//         //Serial.println("[readPulseInputs] Exiting early, job disabled."); // Add log here
//         return false;
//     }

//     bool dataChanged = false;
//     for (int i = 0; i < pulseInputCount; i++) {
//         if (pulseInputs[i].usePCNT && pulseInputs[i].pin == pcntCounterPin) {
//             pcnt_get_counter_value(PCNT_UNIT, &currentPulseCount);

//             if (currentPulseCount != lastPulseCount) {
//                 // Log state *before* calculating pulses
//                 //Serial.printf("[readPulseInputs] Hardware change detected: current=%d, last=%d, pulseCountEnabled=%s\n",
//                 //               currentPulseCount, lastPulseCount, pulseCountEnabled ? "true" : "false");

//                 int16_t newPulses;
//                 // ... (calculation remains the same) ...
//                 if (currentPulseCount < lastPulseCount) {
//                     newPulses = (PCNT_H_LIM_VAL - lastPulseCount) + currentPulseCount;
//                 } else {
//                     newPulses = currentPulseCount - lastPulseCount;
//                 }


//                 if (newPulses > 0) {
//                      // Log state *just before* the inner check
//                      //Serial.printf("[readPulseInputs] Checking inner condition: pulseCountEnabled = %s\n", pulseCountEnabled ? "true" : "false");

//                      if (pulseCountEnabled) {
//                         Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses. New total: %d\n", newPulses, pulseInputs[i].pulseCount + newPulses);
//                         pulseInputs[i].pulseCount += newPulses;
//                         dataChanged = true;
//                         pulseDataDirty = true;
//                         logPulseData(pulseInputs[i].pin, pulseInputs[i].pulseCount);
//                     } else {
//                          // This is the message you are seeing
//                          Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses (Ignored - Job Disabled)\n", newPulses);
//                     }
//                 }
//                 lastPulseCount = currentPulseCount;
//             }
//         }
//     }
//     return dataChanged;
// }

bool DigitalInput::readPulseInputs() {
    bool dataChanged = false;
    
    for (int i = 0; i < pulseInputCount; i++) {
        if (pulseInputs[i].usePCNT && pulseInputs[i].pin == pcntCounterPin) {
            // *** ALWAYS READ THE HARDWARE COUNTER (even if disabled) ***
            pcnt_get_counter_value(PCNT_UNIT, &currentPulseCount);

            if (currentPulseCount != lastPulseCount) {
                int16_t newPulses;
                if (currentPulseCount < lastPulseCount) {
                    newPulses = (PCNT_H_LIM_VAL - lastPulseCount) + currentPulseCount;
                } else {
                    newPulses = currentPulseCount - lastPulseCount;
                }

                if (newPulses > 0) {
                    // *** NOW CHECK IF WE SHOULD COUNT THEM ***
                    if (pulseCountEnabled) {
                        Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses. New total: %d\n", 
                                      newPulses, pulseInputs[i].pulseCount + newPulses);
                        pulseInputs[i].pulseCount += newPulses;
                        dataChanged = true;
                        pulseDataDirty = true;
                        //logPulseData(pulseInputs[i].pin, pulseInputs[i].pulseCount);
                    } else {
                        // Job is disabled - just discard these pulses silently
                        Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses (DISCARDED - Job Disabled)\n", newPulses);
                    }
                        // else {
                        // // Only log every 100 discarded pulses to reduce spam
                        // static uint16_t discardedCount = 0;
                        // discardedCount += newPulses;
                        // if (discardedCount >= 100) {
                        //     Serial.printf("==> %d pulses discarded (Job Disabled)\n", discardedCount);
                        //     discardedCount = 0;
                        // }
                }
                
                // *** CRITICAL: ALWAYS UPDATE lastPulseCount ***
                // This keeps it in sync with hardware regardless of job state
                lastPulseCount = currentPulseCount;
            }
        }
    }
    
    return dataChanged;
}

void DigitalInput::clearOfflinePulseLog() {
    // Only clear if SD card is available
    if (devsbotInstance && devsbotInstance->storeDataToSd && card.cardMounted) {
        
        // --- START MODIFICATION ---
        // If lastOfflinePulseCount is already -1, we know the log is clear.
        // This prevents running SD.exists() or SD.remove() on every success.
        if (lastOfflinePulseCount == -1) {
            // DEBUG("Heartbeat success. Offline pulse log is already clear."); // Optional: can be noisy
            return; 
        }
        // --- END MODIFICATION ---

        DEBUG("Heartbeat success. Clearing redundant offline pulse log.");
        card.pulseLogs.clearLogs(); // Deletes /DI_PulseLog1.txt and /DI_PulseLog2.txt
        lastOfflinePulseCount = -1; // Reset tracker
    }
}


// FreeRTOS sensor task
void DigitalInput::sensorTask(void* pvParameters) {
    DigitalInput* self = static_cast<DigitalInput*>(pvParameters);
    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Read status inputs and pulse inputs
        self->readStatusInputs();
        self->readPulseInputs();
        
        // 10ms delay for continuous monitoring
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10));
    }
}

// Start the sensor task
void DigitalInput::startSensorTask() {
    if (sensorTaskHandle == NULL) {
        xTaskCreatePinnedToCore(
            sensorTask,
            "DigitalInputTask",
            4096,
            this,
            1,
            &sensorTaskHandle,
            0 // use core 0
        );
        Serial.println("Digital Input sensor task started");
    }
}

// Stop the sensor task
void DigitalInput::stopSensorTask() {
    if (sensorTaskHandle != NULL) {
        vTaskDelete(sensorTaskHandle);
        sensorTaskHandle = NULL;
        Serial.println("Digital Input sensor task stopped");
    }
}

// --- MODIFIED: This function now handles the periodic saving ---
void DigitalInput::processLoop() {
    if (pulseDataDirty && (millis() - lastPulseSaveTime > PULSE_SAVE_INTERVAL_MS)) {
        savePulseCountsToSPIFFS();
        lastPulseSaveTime = millis();
        pulseDataDirty = false;
    }
}


// // Function to send activity tracker data for changed status pins
// void DigitalInput::sendActivityTrackerData() {
//     if (!devsbotInstance) return;
    
//     for (int i = 0; i < statusInputCount; i++) {
//         if (statusInputs[i].stateChanged) {
//             DynamicJsonDocument jsonDoc(512);
            
//             jsonDoc["slave_id"] = 1;
//             jsonDoc["gateway_api_id"] = devsbotInstance->getAuthToken();
//             jsonDoc["pin"] = statusInputs[i].pin;
//             jsonDoc["activity_status"] = statusInputs[i].currentState;
//             jsonDoc["time_stamp"] = statusInputs[i].lastChangeTime;
            
//             String jsonString;
//             serializeJson(jsonDoc, jsonString);
            
//             Serial.println("Sending Activity Tracker Data: " + jsonString);
            
//             // The actual HTTP request should be made by DevsbotEnergyLocal instance
//             // This is just preparing the data
            
//             // Reset the change flag
//             statusInputs[i].stateChanged = false;
//         }
//     }
// }

// Function to send alive status with pulse data
void DigitalInput::sendAliveStatusData() {
    if (!devsbotInstance) return;
    
    // The actual API calls should be made by DevsbotEnergyLocal
    // This function prepares the pulse data
    
    Serial.println("Preparing alive status data with pulse counts");
    
    // Update last sent counts
    for (int i = 0; i < pulseInputCount; i++) {
        pulseInputs[i].lastSentCount = pulseInputs[i].pulseCount;
    }
}

// // Function to process alive status response
// void DigitalInput::processAliveStatusResponse(String response) {
//     DynamicJsonDocument doc(512);
//     DeserializationError error = deserializeJson(doc, response);
    
//     if (!error) {
//         String jobStr = doc["job_status"].as<String>();
        
//         if (jobStr == "1") {
//             jobEnabled = true;
//             pulseCountEnabled = true;
//             Serial.println("Job enabled - digital input monitoring active");
//         } else if (jobStr == "0") {
//             jobEnabled = false;
//             pulseCountEnabled = false;
//             resetAllPulseCounts(); // Reset pulse counts when job = 0
//             Serial.println("Job disabled - digital input monitoring stopped, pulse counts reset");
//         }
        
//         int status = doc["status"];
//         if (status == 201) {
//             Serial.println("Digital input alive status successful");
//         }
//     } else {
//         Serial.println("Failed to parse digital input alive status response");
//     }
// }

// --- THIS IS THE FIX: This function now saves the job state to persistent storage --- commented on 24/10/2025
// void DigitalInput::processAliveStatusResponse(String response) {
//     DynamicJsonDocument doc(512);
//     DeserializationError error = deserializeJson(doc, response);
    
//     if (!error) {
//         String jobStr = doc["job_status"].as<String>();
//         bool newJobState = (jobStr == "1");

//         // Only act if the state has actually changed
//         if (newJobState != jobEnabled) {
//             jobEnabled = newJobState;
//             pulseCountEnabled = newJobState;

//             // --- SAVE THE NEW STATE ---
//             jobStatePrefs.begin("DI_JobState", false); // Open in read-write mode
//             jobStatePrefs.putBool("jobEnabled", jobEnabled);
//             jobStatePrefs.end();
//             Serial.println("--> Saved new persistent job state: " + String(jobEnabled ? "Enabled" : "Disabled"));
//             // --- END OF SAVE LOGIC ---

//             if (jobEnabled) {
//                 Serial.println("Job enabled - digital input monitoring active");
//             } else {
//                 resetAllPulseCounts(); // Reset pulse counts only when job becomes 0
//                 Serial.println("Job disabled - digital input monitoring stopped, pulse counts reset");
//             }
//         }
        
//         int status = doc["status"];
//         if (status == 201) {
//             Serial.println("Digital input alive status successful");
//         }
//     } else {
//         Serial.println("Failed to parse digital input alive status response");
//     }
// }

// // Job control methods
// void DigitalInput::setJobEnabled(bool enabled) {
//     jobEnabled = enabled;
//     if (!enabled) {
//         resetAllPulseCounts();
//     }
// }


// --- *** REQUIREMENT 2: ADDED NEW FUNCTION *** ---
// Log pulse data to SD
void DigitalInput::logOfflinePulseCount(int pin, int pulseCount) {
    // Only log if SD logging is enabled and card is mounted
    if (devsbotInstance && devsbotInstance->storeDataToSd && card.cardMounted) {
        
                // If the new count is the same as the last one we saved, do nothing.
        if (pulseCount == lastOfflinePulseCount) {
            DEBUG("[Offline Log] Pulse count unchanged. Skipping SD write.");
            return;
        }

        DEBUG("[Offline Log] Heartbeat send failed. Logging pulse count to SD card.");
        
        DynamicJsonDocument logDoc(256);
        logDoc["job_id"] = currentJobId;
        logDoc["pin"] = pin;
        logDoc["pulse"] = pulseCount;
        logDoc["timestamp"] = getCurrentTimestamp(); // Get the current time of failure
        
        String logString;
        serializeJson(logDoc, logString);

        card.pulseLogs.clearLogs();

        // Now, saveLog will create a new file (e.g., /DI_PulseLog1.txt) with only this new line.
        card.pulseLogs.saveLog(logString);
        
        // Update the last saved count
        lastOfflinePulseCount = pulseCount;
        
        DEBUG("  - Logged: " + logString);
    }
}
// --- *** END NEW FUNCTION *** ---

void DigitalInput::processAliveStatusResponse(String response) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
        String jobStr = doc["job_status"].as<String>();
        bool newJobState = (jobStr == "1");

        // Get the new job_id from the response, default to 0 if not present
        int newJobId = doc["job_id"] | 0;


        // Only act if the state has actually changed
        if (newJobState != jobEnabled || newJobId != currentJobId) {
            bool previouslyEnabled = jobEnabled; // Store the old state
            jobEnabled = newJobState;
            pulseCountEnabled = newJobState;
            currentJobId = newJobId;

            // --- SAVE THE NEW STATE ---
            jobStatePrefs.begin("DI_JobState", false); // Open in read-write mode
            jobStatePrefs.putBool("jobEnabled", jobEnabled);
            // Save the new job_id to persistent storage
            jobStatePrefs.putInt("job_id", currentJobId);
            jobStatePrefs.end();
            Serial.println("--> Saved new persistent job state: " + String(jobEnabled ? "Enabled" : "Disabled") + ", Job ID: " + String(currentJobId));
            // --- END OF SAVE LOGIC ---

            if (jobEnabled) {
                Serial.println("Job enabled - digital input monitoring active");

                // *** ADD THIS BLOCK TO DISCARD PULSES DURING DISABLE ***
                if (!previouslyEnabled && pcntCounterPin != -1) {
                    // Job was just re-enabled. Reset the baseline.
                    int16_t currentHardwareCount;
                    pcnt_get_counter_value(PCNT_UNIT, &currentHardwareCount);
                    lastPulseCount = currentHardwareCount; // Sync software baseline to hardware NOW
                    Serial.printf("--> Job re-enabled. Resetting pulse baseline. Discarding pulses accumulated while disabled. New baseline (lastPulseCount) = %d\n", lastPulseCount);
                }
                // *** END OF ADDED BLOCK ***

            } else {
                // Job was just disabled
                resetAllPulseCounts(); // Reset pulse counts only when job becomes 0
                Serial.println("Job disabled - digital input monitoring stopped, pulse counts reset");
            }
        }

        int status = doc["status"];
        if (status == 201) {
            Serial.println("Digital input alive status successful");
        }
    } else {
        Serial.println("Failed to parse digital input alive status response");
    }
}

// --- 5. In setJobEnabled() ---
void DigitalInput::setJobEnabled(bool enabled) {
    if (jobEnabled != enabled) { // Only act on change
        jobEnabled = enabled;
        pulseCountEnabled = enabled;

        // --- SAVE THE NEW STATE ---
        jobStatePrefs.begin("DI_JobState", false); // Open in read-write mode
        jobStatePrefs.putBool("jobEnabled", jobEnabled);
        
        // --- START MODIFICATION ---
        if (!enabled) {
            currentJobId = 0; // Reset job ID if manually disabled
            resetAllPulseCounts();
        }
        // Save the job_id (either the new '0' or the existing one)
        jobStatePrefs.putInt("job_id", currentJobId);
        // --- END MODIFICATION ---

        jobStatePrefs.end();
        
        // --- START MODIFICATION ---
        Serial.println("--> Manually set and saved persistent job state: " + String(jobEnabled ? "Enabled" : "Disabled") + ", Job ID: " + String(currentJobId));
        // --- END MODIFICATION ---
    }
}

void DigitalInput::setPulseCountEnabled(bool enabled) {
    pulseCountEnabled = enabled;
}

// Function to print current pin configuration (for debugging)
void DigitalInput::printPinConfiguration() {
    Serial.println("========= Digital Input Pin Configuration =========");
    Serial.println("Total Status Inputs: " + String(statusInputCount));
    Serial.println("Total Pulse Inputs: " + String(pulseInputCount));
    Serial.println("Job Enabled: " + String(jobEnabled ? "Yes" : "No"));
    Serial.println("Pulse Count Enabled: " + String(pulseCountEnabled ? "Yes" : "No"));
    
    Serial.println("=== Status Inputs ===");
    for (int i = 0; i < statusInputCount; i++) {
        String typeName = "";
        switch(statusInputs[i].type_name) {
            case MOTOR: typeName = "Motor"; break;
            case HEATER: typeName = "Heater"; break;
            case MACHINE: typeName = "Machine"; break;
        }
        Serial.println("Pin " + String(statusInputs[i].pin) + 
                       " - Type: " + typeName + 
                       " - State: " + String(statusInputs[i].currentState));
    }
    
    Serial.println("=== Pulse Inputs ===");
    for (int i = 0; i < pulseInputCount; i++) {
        Serial.println("Pin " + String(pulseInputs[i].pin) + 
                       " - Count: " + String(pulseInputs[i].pulseCount) +
                       " - PCNT: " + String(pulseInputs[i].usePCNT ? "Yes" : "No"));
    }
    Serial.println("==========================================");
}

// --- MODIFIED: Robust Logging Functions ---
// These now check if SD logging is enabled and if the card is mounted.
void DigitalInput::logStatusChange(int pin, int state, String timestamp, int type_name) {
    // Use the internal devsbotInstance pointer to check flags
    if (devsbotInstance && devsbotInstance->storeDataToSd && card.cardMounted) {
        DynamicJsonDocument logDoc(256);
        
        // Use the *exact* field names the server expects
        logDoc["pin"] = pin;
        logDoc["activity_status"] = state;
        logDoc["timestamp"] = timestamp;
        logDoc["type"] = type_name; // Add the missing 'type' field
        
        String logString;
        serializeJson(logDoc, logString);
        
        card.statusLogs.saveLog(logString);
    }
}

void DigitalInput::logPulseData(int pin, int pulseCount) {
    // Use the internal devsbotInstance pointer to check flags
    if (devsbotInstance && devsbotInstance->storeDataToSd && card.cardMounted) {
        DynamicJsonDocument logDoc(256);
        logDoc["pin"] = pin;
        logDoc["total_count"] = pulseCount;
        logDoc["timestamp"] = getCurrentTimestamp();

        String logString;
        serializeJson(logDoc, logString);

        card.pulseLogs.saveLog(logString);
    }
}



// Getter methods for input data
StatusInputData* DigitalInput::getStatusInput(int index) {
    if (index >= 0 && index < statusInputCount) {
        return &statusInputs[index];
    }
    return nullptr;
}

PulseInputData* DigitalInput::getPulseInput(int index) {
    if (index >= 0 && index < pulseInputCount) {
        return &pulseInputs[index];
    }
    return nullptr;
}

// Reset all inputs
void DigitalInput::resetAllInputs() {
    // Reset status inputs
    for (int i = 0; i < statusInputCount; i++) {
        statusInputs[i].stateChanged = false;
        statusInputs[i].lastChangeTime = "";
    }
    
    // Reset pulse inputs
    resetAllPulseCounts();
}