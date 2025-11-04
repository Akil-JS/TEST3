#include <readmodbusdata.h>
#include "sdcard.h"

#include "DebugConfig.h"
#include "DebugMacro.h"
#include "HardwareRTC.h" // <-- ADDED: Include the new RTC manager

/* ==========  QC WEB SERVER INCLUDES  ========== */
#include <AsyncTCP.h>
#include "QCWebServer.h"
/* ============================================== */


// =========================================================================
// === STABILITY FIX: New Task Handle for SD Card Processing ===
// We create a handle so the timer callback can notify our new dedicated task.
TaskHandle_t sdCardTaskHandle = NULL; 
TaskHandle_t mainAppTaskHandle = NULL; // <-- ADD THIS HANDLE

// =========================================================================



TimerHandle_t myTimer1=NULL;
TimerHandle_t myTimer2=NULL;



unsigned long previousmillis=0;
unsigned long previousmillis1=0;
unsigned long currentmillis;
bool previousmillisFlag=0;

int modbusTimeOut=2000;

const uint8_t ledPins[4]={13,23,25,4};//change 
const uint8_t buttonPins[4]={26,27,22,21};

bool ledPinsState[4]={0};
bool buttonPinsStatus[4]={1,1,1,1};// initially all pins are enabed before reading eeprom value;
uint64_t debounceDelay=5000;
uint64_t lastDebounce=0;
uint64_t lastDebounce1=0;
bool firstGoVal=0;

Preferences ledState;

extern readMeterData energy;
extern sdcard card;

/* ==========  QC WEB SERVER GLOBALS  ========== */
AsyncWebServer  server(80);
AsyncWebSocket  ws("/ws");
QCWebServer     qc(&server, &ws);
/* ============================================ */

void writeLedStatusToEEPROM(uint8_t tempVal)
{
    // Find which LEDs to change based on the button index (tempVal)
    uint8_t ledToTurnOn = tempVal;
    uint8_t ledToTurnOff = (tempVal % 2 == 0) ? (tempVal + 1) : (tempVal - 1); // Pair logic

    // Ensure indices are valid
    if (ledToTurnOn < 4 && ledToTurnOff < 4) {
        DEBUG("[LED] Button " + String(tempVal) + ": Turning ON LED " + String(ledToTurnOn) + ", Turning OFF LED " + String(ledToTurnOff));
        ledPinsState[ledToTurnOn] = 1;
        ledPinsState[ledToTurnOff] = 0;
    } else {
         DEBUG("[ERROR] Invalid LED index calculation in writeLedStatusToEEPROM for tempVal=" + String(tempVal));
         return; // Don't proceed if indices are wrong
    }

    // Save all current states
    ledState.begin("ledState", false);
    DEBUG("[LED] Saving LED states to EEPROM:");
    for (uint8_t i = 0; i < 4; i++) {
        ledState.putBool(String("LED" + String(i)).c_str(), ledPinsState[i]);
        DEBUG("  - LED" + String(i) + " = " + (ledPinsState[i] ? "ON" : "OFF"));
    }
    ledState.end();
}

void readLedStatusEEPROM()
{
    ledState.begin("ledState", false);
    DEBUG("[LED] Reading LED states from EEPROM...");
    for (uint8_t i = 0; i < (sizeof(ledPins) / sizeof(ledPins[0])); i++)
    {
        ledPinsState[i] = ledState.getBool(String("LED" + String(i)).c_str(), 0); // Read as bool
        DEBUG("  - Pin[" + String(i) + "] = " + (ledPinsState[i] ? "ON" : "OFF"));
        digitalWrite(ledPins[i], ledPinsState[i]); // Set LED to stored state
        buttonPinsStatus[i] = !ledPinsState[i];    // Button enabled ONLY if LED is OFF
    }

    if (!ledState.getBool("checkState", 0))
    {
        DEBUG("[LED] First-time setup detected. Writing default states.");
        ledState.putBool("checkState", 1);
        ledState.putBool("firstgo", 1);
        firstGoVal = ledState.getBool("firstgo", 0);
        // Set default ON states explicitly (overrides loop above for first boot)
        ledPinsState[1] = 1; // Default ON
        ledPinsState[3] = 1; // Default ON
        ledState.putBool("LED1", ledPinsState[1]);
        ledState.putBool("LED3", ledPinsState[3]);
        // Update button status based on defaults
        buttonPinsStatus[1] = 0; // Disable button S1 Stop
        buttonPinsStatus[3] = 0; // Disable button S2 Stop

        DEBUG("[LED] First Go: " + String(firstGoVal) + " (LEDs 1 & 3 ON by default)");
    }
    else
    {
         firstGoVal = ledState.getBool("firstgo", 0); // Still need to read this even if not first time
    }

    ledState.end();
}

void buttonReadTask(void * parameter)
{
    DEBUG("[Task] ButtonReadTask started on Core 0.");
    TickType_t lastStackCheck = 0;

    for(;;)
    {
        // Print stack HWM periodically (e.g., every 30 seconds)
        if (xTaskGetTickCount() - lastStackCheck > pdMS_TO_TICKS(30000)) {
            UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            DEBUG("[Task] ButtonReadTask Stack HWM: " + String(stackHighWaterMark) + " bytes free");
            lastStackCheck = xTaskGetTickCount();
        }


        if (dBot.wifiStatus == 1) // Check WiFi status first
        {
            // Check Button 0 (Start S1)
            if (buttonPinsStatus[0] && digitalRead(buttonPins[0]) == LOW && (millis() - lastDebounce) > debounceDelay)
            {
                DEBUG("[Button] Start S1 (Pin " + String(buttonPins[0]) + ") PRESSED. Attempting action...");
                if (dBot.postDataToServer(1, 20, 1)) // Slave 1, Pin 20, Status 1 (ON)
                {
                    writeLedStatusToEEPROM(0);      // Update LEDs for state 0
                    buttonPinsStatus[0] = 0;        // Disable Start S1 button
                    buttonPinsStatus[1] = 1;        // Enable Stop S1 button
                    digitalWrite(ledPins[0], 1);    // LED 0 ON
                    digitalWrite(ledPins[1], 0);    // LED 1 OFF
                    lastDebounce = millis();
                    DEBUG("[Button] Start S1 action SUCCEEDED.");
                }
                else
                {
                    DEBUG("[Button] Start S1 action FAILED (server post error).");
                }
            }
            // Check Button 1 (Stop S1) - Assuming Normally Open (LOW when pressed)
            else if (buttonPinsStatus[1] && digitalRead(buttonPins[1]) == LOW && (millis() - lastDebounce) > debounceDelay)
            {
                 DEBUG("[Button] Stop S1 (Pin " + String(buttonPins[1]) + ") PRESSED. Attempting action...");
                if (dBot.postDataToServer(1, 20, 0)) // Slave 1, Pin 20, Status 0 (OFF)
                {
                    writeLedStatusToEEPROM(1);      // Update LEDs for state 1
                    buttonPinsStatus[1] = 0;        // Disable Stop S1 button
                    buttonPinsStatus[0] = 1;        // Enable Start S1 button
                    digitalWrite(ledPins[0], 0);    // LED 0 OFF
                    digitalWrite(ledPins[1], 1);    // LED 1 ON
                    lastDebounce = millis();
                    DEBUG("[Button] Stop S1 action SUCCEEDED.");
                }
                else
                {
                    DEBUG("[Button] Stop S1 action FAILED (server post error).");
                }
            }
            // Check Button 2 (Start S2)
            else if (buttonPinsStatus[2] && digitalRead(buttonPins[2]) == LOW && (millis() - lastDebounce1) > debounceDelay)
            {
                DEBUG("[Button] Start S2 (Pin " + String(buttonPins[2]) + ") PRESSED. Attempting action...");
                if (dBot.postDataToServer(2, 21, 1)) // Slave 2, Pin 21, Status 1 (ON)
                {
                    writeLedStatusToEEPROM(2);      // Update LEDs for state 2
                    buttonPinsStatus[2] = 0;        // Disable Start S2 button
                    buttonPinsStatus[3] = 1;        // Enable Stop S2 button
                    digitalWrite(ledPins[2], 1);    // LED 2 ON
                    digitalWrite(ledPins[3], 0);    // LED 3 OFF
                    lastDebounce1 = millis();
                    DEBUG("[Button] Start S2 action SUCCEEDED.");
                }
                else
                {
                     DEBUG("[Button] Start S2 action FAILED (server post error).");
                }
            }
            // Check Button 3 (Stop S2) - Assuming Normally Open (LOW when pressed)
            else if (buttonPinsStatus[3] && digitalRead(buttonPins[3]) == LOW && (millis() - lastDebounce1) > debounceDelay)
            {
                 DEBUG("[Button] Stop S2 (Pin " + String(buttonPins[3]) + ") PRESSED. Attempting action...");
                if (dBot.postDataToServer(2, 21, 0)) // Slave 2, Pin 21, Status 0 (OFF)
                {
                    writeLedStatusToEEPROM(3);      // Update LEDs for state 3
                    buttonPinsStatus[3] = 0;        // Disable Stop S2 button
                    buttonPinsStatus[2] = 1;        // Enable Start S2 button
                    digitalWrite(ledPins[2], 0);    // LED 2 OFF
                    digitalWrite(ledPins[3], 1);    // LED 3 ON
                    lastDebounce1 = millis();
                     DEBUG("[Button] Stop S2 action SUCCEEDED.");
                }
                else
                {
                    DEBUG("[Button] Stop S2 action FAILED (server post error).");
                }
            }
        }
        else // WiFi is disconnected
        {
            // Optional: Log periodically that button checks are paused due to WiFi
            // DEBUG("[Button] Skipping check - WiFi disconnected.");
        }

        // Delay at the end of the loop
        vTaskDelay(pdMS_TO_TICKS(100)); // Check buttons every 100ms
    }
}



// --- MODIFIED: The readEnergyData task is now a long-running loop ---
// It is created once at startup and waits for notifications from the timer.
void readEnergyData(void *pvParameters) {
    for (;;) { // Use for(;;) instead of while(1) for style consistency in FreeRTOS
        // Wait indefinitely for a notification from the timer callback.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        
        DEBUG("[Task] EnergyReadTask: Woke up.");
        
        // This check is important to prevent reads before the meter is configured.
        if (energy.readDataFlag == 0 && dBot.meterAddInSpiff) {
            energy.readModbusJson(dBot.numSlave);
        } else {
            DEBUG("[Task] EnergyReadTask: Skipping read (flag set or meter not configured).");
        }
        
        DEBUG("[Task] EnergyReadTask: Completed and sleeping.");
        // The task no longer deletes itself. It loops back to wait for the next notification.
    }
}



// --- MODIFIED: The SD card processing task is also a long-running loop ---
// This was already a good pattern in your sdcard.cpp, this formalizes it here.
// void sdProcessingTask(void *pvParameters) {
//     for (;;) {
//         ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
//         DEBUG("[Task] SDProcessTask: Woke up. Processing queues...");

//         if (SD.cardType() != CARD_NONE) {
//             if (dBot.wifiStatus && !card.isBusy()) {
//                 card.processLogQueues();
//             } else {
//                 DEBUG("[Task] SDProcessTask: Skipping (No WiFi or card busy).");
//             }
//         } else {
//             DEBUG("[Task] SDProcessTask: Skipping (SD card not mounted).");
//         }
//     }
// }


// Inside sdProcessingTask() in main.cpp
void sdProcessingTask(void *pvParameters) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        DEBUG("[Task] SDProcessTask: Woke up. Checking conditions..."); // Modified log

        // --- ADD DETAILED CHECKS ---
        bool cardOK = (SD.cardType() != CARD_NONE);
        bool wifiOK = dBot.wifiStatus;
        bool busyOK = !card.isBusy();

        DEBUG("  - SD Card OK? " + String(cardOK ? "Yes" : "No"));
        DEBUG("  - WiFi OK?    " + String(wifiOK ? "Yes" : "No"));
        DEBUG("  - Card Busy?  " + String(card.isBusy() ? "Yes" : "No"));
        // --- END ADDED CHECKS ---


        if (cardOK) { // Simplified check based on most likely issue
            if (wifiOK && busyOK) {
                DEBUG("[Task] SDProcessTask: Conditions met. Calling processLogQueues()..."); // Added log
                card.processLogQueues();
            } else {
                DEBUG("[Task] SDProcessTask: Skipping (No WiFi or card busy).");
            }
        } else {
            DEBUG("[Task] SDProcessTask: Skipping (SD card not mounted).");
        }
         DEBUG("[Task] SDProcessTask: Finished cycle."); // Added log
    }
}


// --- MODIFIED: Timer callbacks are now simple and fast ---
// They only send a notification and do no heavy work themselves.

void timerCallback1(TimerHandle_t xTimer) {
    if (dataTaskHandle != NULL) {
        DEBUG("[Timer] EnergyTimer: Notifying EnergyReadTask.");
        xTaskNotifyGive(dataTaskHandle);
    }
}
void timerCallback2(TimerHandle_t xTimer) {
    if (sdCardTaskHandle != NULL) {
        DEBUG("[Timer] SDTimer: Notifying SDProcessTask.");
        xTaskNotifyGive(sdCardTaskHandle);
    }
}



// void setup()
// {
//   Serial.begin(115200);
//   delay(200);
//     DebugConfig::init();
//     delay(1000); // Ensure configuration is loaded before using DEBUG macro

//     // Log the current debug mode
//     Serial.printf("Runtime Debug mode: SERIAL=%s, WEB=%s, BOTH=%s\n",
//                   DebugConfig::DEBUG_SERIAL ? "ON" : "OFF",
//                   DebugConfig::DEBUG_WEB ? "ON" : "OFF",
//                   DebugConfig::DEBUG_BOTH ? "ON" : "OFF");

//     // This makes the correct time available for all subsequent operations.
//     // rtcManager.init();
//     // Check if QCWebServer instance is available
//     if (QCWebServer::getInstance()) {
//         DEBUG("[QC] QCWebServer instance is available.");
//     } else {
//         DEBUG("[QC] QCWebServer instance is NOT available.");
//     }
//   dBot.begin();
//   delay(2000);

//       // This makes the correct time available for all subsequent operations.
//   rtcManager.init();
//   energy.serialInit();


//   if(dBot.maduraSteelOn)
// {
//   for(uint8_t i=0;i<sizeof(ledPins)/sizeof(ledPins[0]);i++)
//   {
//     pinMode(ledPins[i],OUTPUT);
//     pinMode(buttonPins[i],INPUT);
//     digitalWrite(ledPins[i],0);
//   }

//   readLedStatusEEPROM();

//   if(firstGoVal==1)
//   {
//     DEBUG("[Main] First Go: Disabling buttons for default ON LEDs."); 
//     digitalWrite(ledPins[1],1);
//     digitalWrite(ledPins[3],1);
//     buttonPinsStatus[1]=0;
//     buttonPinsStatus[3]=0;
//   }

//   xTaskCreatePinnedToCore(
//     buttonReadTask,     // Task function
//     "button press",   // Name of the task
//     1024*5,           // Stack size (in bytes)
//     NULL,             // Task input parameter
//     1,                // Task priority
//     &TaskHandle_PostData_Start_S1,  // Task handle
//     0                 // Core where the task should run
//   );
// }
  
//   /** below is section of code for storing a data to a sd card */
//     if (dBot.storeDataToSd) 
//     {
//         DEBUG("[Main] SD card logging is ENABLED.");
//         if (card.sdInit()) { // Only proceed if SD card is successfully mounted
//             rtcManager.init(); // Initialize RTC after SD card is confirmed

//             // --- THIS IS THE FIX: Create tasks ONCE at startup ---
            
//             // 1. Create the dedicated task for reading energy data.
//             xTaskCreatePinnedToCore(readEnergyData, "EnergyReadTask", 4096, NULL, 1, &dataTaskHandle, 0);

//             // 2. Create the dedicated task for processing SD card data.
//             xTaskCreatePinnedToCore(sdProcessingTask, "SDProcessTask", 4096, NULL, 1, &sdCardTaskHandle, 0);

//             // --- END OF FIX ---

//             // Start the timers that will notify these tasks.
//             myTimer1 = xTimerCreate("EnergyTimer", pdMS_TO_TICKS(1000 * 60), pdTRUE, NULL, timerCallback1);
//             if (myTimer1 && xTimerStart(myTimer1, 0) != pdPASS) {
//                 DEBUG("[ERROR] Failed to start EnergyTimer!");
//             }

//             myTimer2 = xTimerCreate("SDTimer", pdMS_TO_TICKS(1000 * 90), pdTRUE, NULL, timerCallback2);
//             if (myTimer2 && xTimerStart(myTimer2, 0) != pdPASS) {
//                 DEBUG("[ERROR] Failed to start SDTimer!");
//             }
            
//             DEBUG("[Main] SD tasks and timers started.");
//         } else {
//             DEBUG("[ERROR] SD Card init FAILED. Logging disabled.");
//         }
//     } 
//     else 
//     {
//         DEBUG("[Main] SD card logging is DISABLED.");
//     }
      
//     /* ==========  QC WEB SERVER START-UP  ========== */
//     // 1. export live config from DevsbotEnergyLocal
//     int* dioPins; String* dioModes; int dioCnt;
//     uint8_t* slaveIds; int slaveCnt;
//     uint16_t* regs;     int regCnt;
//     uint32_t    baud; uint8_t dbits, par, sb;

//     dBot.getDIOConfiguration(dioPins, dioModes, dioCnt);
//     dBot.getRS485Configuration(slaveIds, slaveCnt,
//                                regs,     regCnt,
//                                baud, dbits, par, sb);

//     qc.setDIOConfiguration(dioPins, dioModes, dioCnt);
//     qc.setRS485Configuration(slaveIds, slaveCnt,
//                                regs,     regCnt,
//                                baud, dbits, par, sb,
//                                17, 16, 4, 2);   // example HW pins

//     qc.setDevsbotReference(&dBot);   // callback bridge
//     qc.begin();                     // start web + ws
//     QCWebServer::activeInstance = &qc;  // Set the active instance
//     server.begin();
//     if (WiFi.isConnected()) {
//       DEBUG("\n[MAIN] QC Web Server ready on http://" + WiFi.localIP().toString());
//     } else {
//       DEBUG("\n[MAIN] QC Web Server ready on http://192.168.4.1");
//     }
    

// }



// =========================================================================
// === Main Application Task (Replaces Arduino loop()) ===
// =========================================================================
void mainAppTask(void * parameter) {
    DEBUG("[Task] Main App Task started on Core 1.");
    for(;;) // Loop forever
    {
        // 1. Let the Devsbot object handle its core logic (heartbeat, WiFi management, etc.)
        dBot.Loop(); // This now runs safely in its own task context

        // 2. If not storing to SD, manually trigger energy reads.
        //    (This logic stays here as it's part of the main application flow)
        if (!dBot.storeDataToSd) {
            // Check if STA is connected before trying to read based on time
            // Use dBot.wifiStatus which is updated by the event handler
            if (dBot.wifiStatus == 1) { // Check if connected
                currentmillis = millis();
                if (previousmillisFlag == 0) {
                    previousmillisFlag = 1;
                    previousmillis = millis();
                }
                if (currentmillis - previousmillis >= (1000 * dBot.completeJsonReading)) {
                    DEBUG("[MainApp] Triggering manual energy read (no SD card).");
                    energy.readModbusJson(dBot.numSlave);
                    previousmillis = millis();
                }
            } else {
                 // Reset flag if disconnected so timer starts fresh on reconnect
                 previousmillisFlag = 0;
            }
        }

        // 3. Handle the Quality Control web server interface.
        //    (This should also run frequently)
        qc.loop();

        // 4. Yield time to other tasks. Increased delay slightly.
        vTaskDelay(pdMS_TO_TICKS(50)); // Yield for 50ms
    }
}


void setup()
{
    Serial.begin(115200);
    delay(200); // Short delay for serial init
    
    // --- Initialize Debugging First ---
    DebugConfig::init();
    Serial.printf("\n========================================\n");
    Serial.printf("        ESP32 Gateway Booting Up        \n");
    Serial.printf("========================================\n");
    Serial.printf("Runtime Debug: SERIAL=%s, WEB=%s, BOTH=%s\n",
                  DebugConfig::DEBUG_SERIAL ? "ON" : "OFF",
                  DebugConfig::DEBUG_WEB ? "ON" : "OFF",
                  DebugConfig::DEBUG_BOTH ? "ON" : "OFF");

    // --- Initialize Core Systems ---
    rtcManager.init(); // Init RTC early for timestamps
    dBot.begin();      // Init Devsbot Core (includes WiFi, SPIFFS, etc.)
    energy.serialInit(); // Init Modbus Serial

    // --- Configure Madura Steel Specifics (if enabled) ---
    if (dBot.maduraSteelOn)
    {
        DEBUG("[Setup] Configuring Madura Steel mode...");
        for (uint8_t i = 0; i < sizeof(ledPins) / sizeof(ledPins[0]); i++)
        {
            pinMode(ledPins[i], OUTPUT);
            pinMode(buttonPins[i], INPUT); // Assuming INPUT_PULLUP is not needed
            digitalWrite(ledPins[i], LOW);
        }
        readLedStatusEEPROM(); // Reads and applies previous LED states

        if (firstGoVal == 1)
        {
            DEBUG("[Setup] First Go: Disabling buttons for default ON LEDs.");
            digitalWrite(ledPins[1], 1);
            digitalWrite(ledPins[3], 1);
            buttonPinsStatus[1] = 0; // Disable button check
            buttonPinsStatus[3] = 0; // Disable button check
        }

        // --- Increase Stack for Button Task ---
        DEBUG("[Setup] Creating Button Read Task on Core 0...");
        xTaskCreatePinnedToCore(
            buttonReadTask,     // Task function
            "ButtonReadTask",   // Name of the task
            1024 * 8,           // Stack size (Increased to 8KB)
            NULL,               // Task input parameter
            1,                  // Task priority
            &TaskHandle_PostData_Start_S1, // Task handle
            0                   // Core: Pin to Core 0
        );
    }

    // --- Configure SD Card Logic (if enabled) ---
    if (dBot.storeDataToSd)
    {
        DEBUG("[Setup] SD card logging is ENABLED.");
        if (card.sdInit()) // sdInit now prints its own success/fail messages
        {
            // Create tasks ONCE at startup
            DEBUG("[Setup] Creating Energy Read Task on Core 0...");
            xTaskCreatePinnedToCore(readEnergyData, "EnergyReadTask", 4096, NULL, 1, &dataTaskHandle, 0);

            DEBUG("[Setup] Creating SD Process Task on Core 0...");
            xTaskCreatePinnedToCore(sdProcessingTask, "SDProcessTask", 4096, NULL, 1, &sdCardTaskHandle, 0);

            // Start the timers that will notify these tasks.
             DEBUG("[Setup] Starting Timers...");
            myTimer1 = xTimerCreate("EnergyTimer", pdMS_TO_TICKS(1000 * dBot.completeJsonReading), pdTRUE, NULL, timerCallback1); // Use configured interval
            if (myTimer1 && xTimerStart(myTimer1, 0) == pdPASS) {
                 DEBUG("  - EnergyTimer started (Interval: " + String(dBot.completeJsonReading) + "s)");
            } else {
                 DEBUG("[ERROR] Failed to start EnergyTimer!");
            }

            // Using 90s interval as before
            myTimer2 = xTimerCreate("SDTimer", pdMS_TO_TICKS(1000 * 90), pdTRUE, NULL, timerCallback2);
             if (myTimer2 && xTimerStart(myTimer2, 0) == pdPASS) {
                 DEBUG("  - SDTimer started (Interval: 90s)");
            } else {
                 DEBUG("[ERROR] Failed to start SDTimer!");
            }
        }
        // sdInit() handles the failure message now
    }
    else
    {
        DEBUG("[Setup] SD card logging is DISABLED.");
    }

    // --- Setup QC Web Server ---
    DEBUG("[Setup] Initializing QC Web Server...");
    int* dioPins; String* dioModes; int dioCnt;
    uint8_t* slaveIds; int slaveCnt;
    uint16_t* regs;     int regCnt;
    uint32_t    baud; uint8_t dbits, par, sb;

    dBot.getDIOConfiguration(dioPins, dioModes, dioCnt);
    dBot.getRS485Configuration(slaveIds, slaveCnt, regs, regCnt, baud, dbits, par, sb);

    qc.setDIOConfiguration(dioPins, dioModes, dioCnt);
    qc.setRS485Configuration(slaveIds, slaveCnt, regs, regCnt, baud, dbits, par, sb, 17, 16, 4, 2); // example HW pins
    qc.setDevsbotReference(&dBot);
    qc.begin();
    QCWebServer::activeInstance = &qc;
    server.begin();

    String qcServerAddress = (WiFi.isConnected()) ? WiFi.localIP().toString() : "192.168.4.1";
    DEBUG("[Setup] QC Web Server ready on http://" + qcServerAddress);

    // --- CRITICAL FIX: Create Main Application Task ---
    DEBUG("[Setup] Creating Main Application Task on Core 1...");
    xTaskCreatePinnedToCore(
        mainAppTask,        // Task function
        "MainAppTask",      // Name
        1024 * 16,          // Stack size (10KB - generous for network calls)
        NULL,               // Parameter
        1,                  // Priority
        &mainAppTaskHandle, // Task handle
        1                   // Core: Pin to Core 1 (where loop() used to run)
    );

    DEBUG("\n========================================");
    DEBUG("         Setup Complete - Running         ");
    DEBUG("========================================");
}



// // --- SIMPLIFIED: loop() function ---
// void loop() {
//     // 1. Let the Devsbot object handle its core logic (heartbeat, live data, etc.)
//     dBot.Loop();

//     // 2. If not storing to SD, we need to manually trigger energy reads.
//     if (!dBot.storeDataToSd) {
//         if (!dBot.notConnetedtoNetwork) {
//             currentmillis = millis();
//             if (previousmillisFlag == 0) {
//                 previousmillisFlag = 1;
//                 previousmillis = millis();
//             }
//             if (currentmillis - previousmillis >= (1000 * dBot.completeJsonReading)) {
//                 energy.readModbusJson(dBot.numSlave);
//                 previousmillis = millis();
//             }
//         }
//     }

//     // 3. Handle the Quality Control web server interface.
//     qc.loop();

//     // 4. A small delay is good practice to yield time to other FreeRTOS tasks.
//     vTaskDelay(pdMS_TO_TICKS(10));
// }

// --- Arduino loop() is now empty ---
// The main logic runs in mainAppTask
void loop() {
   // Intentionally empty.
   // You could put a very small delay here if needed, but mainAppTask has its own.
   // vTaskDelay(pdMS_TO_TICKS(1000)); // e.g., sleep for 1 second
}