// #include "sdcard.h"
// #include "DevsbotEnergyLocal.h" // For access to the dBot instance

// // Define pins
// #define SD_CS_PIN   15
// #define SD_CLK_PIN  14
// #define SD_MOSI_PIN 13
// #define SD_MISO_PIN 2

// SPIClass spi2(HSPI);
// // --- ADD THIS LINE ---
// // This is the one and only place where the global 'card' object is actually created.
// sdcard card;
// extern Devsbot dBot; // Make the global dBot instance available


// // --- ADD THIS NEW FUNCTION IMPLEMENTATION ---
// void sdcard::saveEnergyLog(const String& logData) {
//     // This function provides a safe and simple way for other parts
//     // of the code to save energy data, without needing to know the details.
//     energyLogs.saveLog(logData);
// }

// sdcard::sdcard() : cardMountFail(true) { // Default to failed until sdInit succeeds
//     // The constructor now configures each LogManager instance.
//     // This is where you define the files and the server upload function for each log type.
//         // --- NEW: Initialize the flags to a safe default state ---
//     cardMounted = false;
//         // --- NEW: Initialize the processing flag ---
//     isProcessing = false;

//     //     // The single, master mutex is created here.
//     // fileMutex = xSemaphoreCreateMutex();
//     // if (fileMutex == NULL) {
//     //     Serial.println("CRITICAL: Failed to create file mutex!");
//     // }


//     // 1. Configure Energy Data Logger
//     energyLogs.initialize(
//         "Energy", 
//         "/energylog1.txt", 
//         "/energylog2.txt", 
//         "sdEnergy",
//         [&](String& payload) { return dBot.sentEnergyMeterData(payload); }
//     );

//     // 2. Configure Digital Input Status Logger
//     statusLogs.initialize(
//         "DI_Status", 
//         "/DI_Statuslog1.txt", 
//         "/DI_Statuslog2.txt", 
//         "sdDIStatus",
//         [&](String& payload) { return dBot.sendDIStatusData(payload); }
//     );

//     // 3. Configure Digital Input Pulse Logger
//     pulseLogs.initialize(
//         "DI_Pulse", 
//         "/DI_PulseLog1.txt", 
//         "/DI_PulseLog2.txt", 
//         "sdDIPulse",
//         [&](String& payload) { return dBot.sendDIPulseData(payload); }
//     );
// }



// // --- MODIFIED: This function now sets the cardMounted flag ---
// bool sdcard::sdInit() {
//     spi2.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
//     delay(200);

//     if (!SD.begin(SD_CS_PIN, spi2, 80000000)) {
//         Serial.println("Card Mount Failed");
//         dBot.deviceLog("!!card mount failed!!");
//         cardMounted = false; // Set flag to false on failure
//         return false;
//     }

//     uint8_t cardType = SD.cardType();
//     if (cardType == CARD_NONE) {
//         Serial.println("No SD card attached");
//         dBot.deviceLog("!!No SD card present in SD slot!!");
//         cardMounted = false; // Set flag to false on failure
//         return false;
//     }
    
//     Serial.println("SD card initialized successfully.");
//     cardMounted = true; // Set flag to true on success
//     return true;
// }


// void sdcard::processLogQueues() {
//     // If we are already processing, don't start again.
//     if (isProcessing) {
//         return;
//     }

//     // Set the lock flag to TRUE before starting work.
//     isProcessing = true;

//     // Process each log type.
//     energyLogs.processAndSendData();
//     statusLogs.processAndSendData();
//     pulseLogs.processAndSendData();

//     // Set the lock flag to FALSE after all work is done.
//     isProcessing = false;
// }


// // // --- MODIFIED: This function now only clears the digital input logs ---
// // void sdcard::clearDigitalInputLogs() {
// //     if (xSemaphoreTake(fileMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
// //         Serial.println("Clearing Digital Input log files for a new job...");
        
// //         // Only clear the status and pulse logs.
// //         statusLogs.clearLogs();
// //         pulseLogs.clearLogs();

// //         // The energyLogs are NOT cleared.
        
// //         xSemaphoreGive(fileMutex);
// //     } else {
// //         Serial.println("CRITICAL ERROR: Could not get mutex to clear DI log files!");
// //     }
// // }

// // Mutex is also removed from here for the same reason.
// void sdcard::clearDigitalInputLogs() {
//     Serial.println("Clearing Digital Input log files for a new job...");
//     statusLogs.clearLogs();
//     pulseLogs.clearLogs();
// }


#include "sdcard.h"
#include "DevsbotEnergyLocal.h"
#include "DebugMacro.h"

#define SD_CS_PIN   15
#define SD_CLK_PIN  14
#define SD_MOSI_PIN 13
#define SD_MISO_PIN 2

SPIClass spi2(HSPI);
sdcard card;
extern Devsbot dBot; 

// --- STABILITY FIX: This function now properly encapsulates all SD card logic ---
void sdcard::saveEnergyLog(const String& logData) {
    // 1. Wait for up to 1 second to acquire exclusive access.
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        
        // 2. CRITICAL FIX: Check the card's status *inside* the protected block.
        if (!cardMounted) {
            DEBUG("!! SD card not mounted, cannot save energy log.");
        } else {
            // 3. If the card is mounted, proceed with the save operation.
            DEBUG("!! SD card mounted, saving energy log.");
            energyLogs.saveLog(logData);
        }
        
        // 4. Release the mutex for other tasks to use.
        xSemaphoreGive(spiMutex);

    } else {
        DEBUG("!! FAILED to get SPI mutex in saveEnergyLog. Data was not saved.");
    }
}

sdcard::sdcard() : cardMountFail(true) { 
    cardMounted = false;
    isProcessing = false;

    spiMutex = xSemaphoreCreateMutex();
    if (spiMutex == NULL) {
        DEBUG("CRITICAL: Failed to create SPI mutex!");
    }

    // --- MODIFICATION: Pass '&dBot' as the first argument ---
    energyLogs.initialize(
        &dBot, // <-- Pass the dBot pointer
        "Energy", 
        "/energylog1.txt", 
        "/energylog2.txt", 
        "sdEnergy",
        [&](String& payload) { return dBot.sentEnergyMeterData(payload); }
    );

    // --- MODIFICATION: Pass '&dBot' as the first argument ---
    statusLogs.initialize(
        &dBot, // <-- Pass the dBot pointer
        "DI_Status", 
        "/DI_Statuslog1.txt", 
        "/DI_Statuslog2.txt", 
        "sdDIStatus",
        [&](String& payload) { return dBot.sendDIStatusData(payload); }
    );

    // --- MODIFICATION: Pass '&dBot' as the first argument ---
    pulseLogs.initialize(
        &dBot, // <-- Pass the dBot pointer
        "DI_Pulse", 
        "/DI_PulseLog1.txt", 
        "/DI_PulseLog2.txt", 
        "sdDIPulse",
        [&](String& payload) { return dBot.sendDIPulseData(payload); }
    );
}

bool sdcard::sdInit() {
    bool success = false;
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        spi2.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
        delay(200);

        if (!SD.begin(SD_CS_PIN, spi2, 80000000)) {
            Serial.println("Card Mount Failed");
            cardMounted = false;
        } else {
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_NONE) {
                Serial.println("No SD card attached");
                cardMounted = false;
            } else {
                Serial.println("SD card initialized successfully.");
                cardMounted = true;
                success = true;
            }
        }
        xSemaphoreGive(spiMutex);
    }
    return success;
}

void sdcard::processLogQueues() {
    if (isProcessing) {
        return;
    }

    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (!cardMounted) {
            DEBUG("!! SD card not mounted, cannot process log queues.");
            xSemaphoreGive(spiMutex);
            return;
        }
        isProcessing = true;

        energyLogs.processAndSendData();
        statusLogs.processAndSendData();
        pulseLogs.processAndSendData();

        isProcessing = false;
        xSemaphoreGive(spiMutex);
    } else {
        DEBUG("!! FAILED to get SPI mutex in processLogQueues.");
    }
}

void sdcard::clearDigitalInputLogs() {
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (!cardMounted) {
            DEBUG("!! SD card not mounted, cannot clear logs.");
        } else {
            Serial.println("Clearing Digital Input log files for a new job...");
            statusLogs.clearLogs();
            pulseLogs.clearLogs();
        }
        xSemaphoreGive(spiMutex);
    } else {
        DEBUG("!! FAILED to get SPI mutex in clearDigitalInputLogs.");
    }
}

