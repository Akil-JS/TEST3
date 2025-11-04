#include<readmodbusdata.h>
#include<sdcard.h>
// #include "DebugHelper.h"
#include "DebugConfig.h"
#include "DebugMacro.h"
// #include "DevsbotEnergyLocal.h"
#include "HardwareRTC.h" // <-- ADDED: Include the new RTC manager


// --- START OF MODIFICATION (BLOCK 1) ---
// Set to 1 to enable hardcoded data, 0 to use real Modbus.
#define HARDCODE_ENERGY_DATA 1
// --- END OF MODIFICATION (BLOCK 1) ---


ModbusMaster node;
byte keyValThree=0;
const char* values[]={"R","Y","B"};
extern meterParams mtrparam[12];
extern sdcard card;

void readMeterData::serialInit()
{
  DEBUG("serialinit begin\n");
  DEBUG("baudRate : ");DEBUG(baudRate); 
  DEBUG("parity and stop bit : ");DEBUG(parityStopbit);

  if(!dBot.storeDataToSd)
  {
    pinMode(MAX485_RE_NEG, OUTPUT);
    pinMode(MAX485_DE, OUTPUT);
  //Init in receive mode
    digitalWrite(MAX485_RE_NEG, LOW);
    digitalWrite(MAX485_DE, LOW);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    Serial2.begin(baudRate,parityStopbit);
  }
  else
    Serial2.begin(baudRate,parityStopbit,32,33);

  // Serial2.begin(baudRate,parityStopbit); //serial 2: RX2 and TX2 in Arduino Mega, SERIAL_8E1 8bit data,evenparity,1stopbit
  //  Serial2.begin(9600,SERIAL_8E1); //serial 2: RX2 and TX2 in Arduino Mega, SERIAL_8E1 8bit data,evenparity,1stopbit
  // Serial2.begin(9600,  SERIAL_8N1, 32, 33);
  delay(200);
  while(!Serial2){}
  // node.begin(1,Serial2);
  DEBUG("Serial init completed \n");
  DEBUG("Serial init completed \n");
}

void postTransmission()   //Set up call back function9
{
  digitalWrite(MAX485_RE_NEG, LOW);
  digitalWrite(MAX485_DE, LOW);
}

void preTransmission()  //Set up call back function
{
  digitalWrite(MAX485_RE_NEG, HIGH);
  digitalWrite(MAX485_DE, HIGH);
}



void readMeterData:: conversion(void* ptr,uint8_t dataType,uint8_t program)
{
  float pfVal=0.0;
  if(program == 1)
  {
    DEBUG("multiply by 1000 program 1 ");

    if(dataType==1)
      *(uint16_t*)ptr*=1000;
    else if (dataType == 2) 
      *(float*)ptr *= 1000;
    else if (dataType == 3)
      *(uint32_t*)ptr *= 1000;
    else if (dataType == 4)
      *(uint64_t*)ptr *= 1000;
  }
  else if(program == 2)
  {
    

    if (dataType == 2)
    {
      DEBUG("tempfloat :"); DEBUG(*(float*)ptr);
      powerfactorcalculation(ptr);

    } 

  }
  else if (program == 3)
  {
    DEBUG("div by 10 program 3 ");
    if(dataType==1)
      *(uint16_t*)ptr/=10;
    else if (dataType == 2) 
      *(float*)ptr/= 10;
    else if (dataType == 3)
      *(uint32_t*)ptr/= 10;
    else if (dataType == 4)
      *(uint64_t*)ptr/= 10;

  }
  
}


void readMeterData::powerfactorcalculation(void *vdrptr) //power factor calculation for each phase as per IEEE
{
  DEBUG("pf cal function for schenider meter");

  float tempfloat=0.0;
  DEBUG("tempfloat :"); DEBUG(tempfloat);

  if (*(float*)vdrptr > 1)
  {
    tempfloat = 2 - *(float*)vdrptr;
  }
  else if (*(float*)vdrptr < -1)
  {
    tempfloat = -2 - *(float*)vdrptr;
    //PF is leading
  }

  DEBUG("temp float val: ");DEBUG(tempfloat);

  *(float*)vdrptr = tempfloat;
}


/** if wrong address is encountered move continue to nxt address value got jumped to nxt pin is corected */
// void readMeterData::readModbusJson(uint8_t slaveNum)
// {
//   DEBUG("readModbusJson begin\n");

//   readDataFlag=1;
//   DynamicJsonDocument docJson1(2048);
//   JsonObject meterData;
//   JsonObject phaseQty;
//   JsonArray metersArray;

//   uint8_t result;
//   // uint8_t noOfSuccessCnt=0;
//   uint8_t numRegToRead=0;
//   bool modpollError=0;
//   void *vp;
//   float floatValue;
//   byte slaveFailCount=0;
//   byte count;
//   bool timeOutFlag=0;
//   bool onceSetFlag;
//   uint8_t timeOutCnt=0;
//   const uint8_t noOfTimeout=2;
//   bool cnt=true;
//   uint16_t plcData=0;
//   uint32_t value_32;
//   uint64_t value_64;
//   //uint16_t data[2];
//   metersArray = docJson1.to<JsonArray>();
//   for (byte slave = 0; slave < slaveNum; slave++)
//   {
//     onceSetFlag=1;
//     count=0;
//     timeOutCnt=0;
//     Serial.printf("slave : %d\n",dBot.slaveIdArray[slave]);
//     //DEBUG("slave : %d\n",slave);
//     node.begin(dBot.slaveIdArray[slave],Serial2);
//     //node.begin(1,Serial2);

//     if(dBot.wifiStatus)
//       dBot.deviceContinue();
//     uint8_t i,j;
//     for(i=0,j=0;i<mtrparam[slave].size;i++) //sizevPins=6,size=6
//     {
      
//       DEBUG("reg Address: " );DEBUG(mtrparam[slave].regAddr[i]);
//       DEBUG(" vpins : ");DEBUG(mtrparam[slave].vPins[j]);
//       DEBUG(" count : ");DEBUG(count);
//       DEBUG("reg type : " );DEBUG(mtrparam[slave].regType[i]);
      

//       if(mtrparam[slave].dataType[i]==3)
//         numRegToRead=2;
//       else
//         numRegToRead=mtrparam[slave].dataType[i];


//       if(mtrparam[slave].regType[i]==1 )
//       {
//         DEBUG("coil Reg\n");
//         result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
//       if(mtrparam[slave].regType[i]==2 )
//       {
//         DEBUG("readDiscrete Reg\n");
//         result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
//       else if(mtrparam[slave].regType[i]==3)
//       {
//         DEBUG("holding Reg\n");
//         // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//         result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
//       else if(mtrparam[slave].regType[i]==4)
//       {
//         DEBUG("Input Reg\n");
//         // result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//         result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
      
//       delay(200);



//       uint64_t checkStartTimer = millis();
//       uint64_t checkEndTimer = checkStartTimer + modbusTimeOut;
//       while (result!=node.ku8MBSuccess)
//       {
//         checkStartTimer=millis();
//         switch (result) 
//         {
//           case node.ku8MBIllegalFunction:
//             DEBUG("Illegal function.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal function" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBIllegalDataAddress:
//             DEBUG("Illegal data address.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data address" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBIllegalDataValue:
//             DEBUG("Illegal data value.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data value" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBSlaveDeviceFailure:
//             DEBUG("Slave device failure.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Slave device failure" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBInvalidSlaveID:
//             DEBUG("Invalid Slave ID.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid Slave ID" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBInvalidFunction:
//             DEBUG("Invalid function.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid function" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBResponseTimedOut:
//             DEBUG("Response timed out.");
//             // dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Response timed out " + String(mtrparam[slave].regAddr[i]));
//             break;
//           case node.ku8MBInvalidCRC:
//             DEBUG("Invalid CRC.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid CRC" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           default:
//             DEBUG("Unknown error.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Unknown error" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//         }

//         if(modpollError)
//         {
//           DEBUG("error occured move to nxt data\n");
//           break;
//         }

//         if(mtrparam[slave].regType[i]==1)
//           result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         else if(mtrparam[slave].regType[i]==2)
//           result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         else if(mtrparam[slave].regType[i]==3)
//         {
//           // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//           result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(300);
//         }
//         else if(mtrparam[slave].regType[i]==4)
//         {
//           //result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//           result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(300);
//         }

//         delay(200);


//         /** check data until a for a certain time  */
//         if(checkEndTimer<=checkStartTimer)
//         {
//           timeOutFlag=1;
//           timeOutCnt++;
//           Serial.printf("failcount: %d\n",slaveFailCount);

         
//           if(slaveNum == 1)
//             break;
//           else if(slaveFailCount==slaveNum)
//           {
//             dBot.sentEnergyMeterData(noData);
//             docJson1.clear();
//             readDataFlag=0;
//             return;
//           }
//           break;
//         }
//       }


//       /** modpoll error occur mainly it will occur bcz of giving a wrong address in application ,
//        * if reading a data from a slave fails go for nxt value ,like that go for all n value   */
//       if(modpollError)
//       {
//         DEBUG("make a modpollerror to 0\n");
//         modpollError=0;
//         if(i != mtrparam[slave].size - 1)
//         {

//           if(mtrparam[slave].noParam[j]==3)
//           {
//             DEBUG("count : "); DEBUG(count);
//             count++;

//             if(count < sizeof(values)/sizeof(values[0]))
//               continue;
//             else
//             {
//               j++;
//               count=0;
//               if(cnt==false)
//                 cnt=true;
//             }
//           }
//           else
//             j++;

//           continue;
//         }
//         else
//         {
//           count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
//           cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
//           DEBUG("this is last data form a json\n");
//           break;
//         }
//       }
      

//       if(timeOutFlag==1)
//       {
//         timeOutFlag=0;
//         if(timeOutCnt < noOfTimeout)
//         {
//           DEBUG("timeout occured \n");
//           if(mtrparam[slave].noParam[j]==3)
//           {
//             DEBUG("count : "); DEBUG(count);
//             count++;

//             if(count < sizeof(values)/sizeof(values[0]))
//             {
//               continue;
//             }
//             else
//             {
//               j++;
//               count=0;
//               if(cnt==false)
//                 cnt=true;
//             }
//           }
//           else
//             j++;

//           continue; // reading a nxt address of current slave
//         }
//         else
//         {
//           count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
//           cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
//           slaveFailCount++;
//           timeOutCnt=0;
//           break; // moving to a nxt slave
//         }
//       }

//       timeOutCnt=0;

//       if (onceSetFlag)
//       {
//         onceSetFlag=0;
//         meterData=metersArray.createNestedObject();
//         if(dBot.storeDataToSd)
//         {
//           DateTime now = rtcManager.now();
//           // // Format the date as YYYY-MM-DD HH:MM:SS
//           char updatetime[20]={0}; //init
//           sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
//           meterData["updatetime"]=updatetime;
//         }

//         meterData["slave_id"]=dBot.slaveIdArray[slave];
//       }

//       /**  parsing a uint16_t value  */
//       if(mtrparam[slave].dataType[i]==1) 
//       {
//         plcData=node.getResponseBuffer(0); //read a 16 bit data (LSB)
//         vp=&plcData;
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(uint16_t*)vp);
//         // meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
//       }
      
//       /**  parsing a float value  */
//       else if(mtrparam[slave].dataType[i]==2)//float value
//       {
//         value_32=0;
//         if(mtrparam[slave].endian[i]==1)
//           value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//         else if(mtrparam[slave].endian[i]==2)
//           value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
        
//         memcpy(&floatValue, &value_32, sizeof(floatValue));
        
        
//         vp=&floatValue;
//         // if(mtrparam[slave].operation[i]!=0 && !(mtrparam[slave].operation[i]==2))
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(float*)vp);
//       }

//       /**  parsing a uint32_t value  */
//       else if(mtrparam[slave].dataType[i]==3)//uint32_t value
//       {
//         value_32=0;
//         if(mtrparam[slave].endian[i]==1)
//           value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//           //value_32=(node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//         else if(mtrparam[slave].endian[i]==2)
//           value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
//          // value_32= (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
        

//         // DEBUG("lower buff : ");DEBUG(node.getResponseBuffer(0));
//         // DEBUG("higher buff : ");DEBUG(node.getResponseBuffer(1));
//         vp=&value_32;
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(uint32_t*)vp);
//       }

//       /**  parsing a uint64_t value  */
//       else if(mtrparam[slave].dataType[i]==4)//uint64_t
//       {
//         value_64=0;
//         if(mtrparam[slave].endian[i]==1)
//         {
//           //for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
//           for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
//             value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (index*16));
//         }
//         if(mtrparam[slave].endian[i]==2)
//         {
//           // for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
//           for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
//             value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (48-16*index));
//         }
//         vp=&value_64;
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(uint64_t*)vp);

//         // meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);
//       }


//       /** if virtual pin takes a ryb value eg.v1{r,y,b} */
//       if(mtrparam[slave].noParam[j]==3)
//       {
//         if(cnt==true)
//         {
//           cnt=false;
//           phaseQty=meterData[mtrparam[slave].vPins[j]].to<JsonObject>();
//         }
//         //DEBUG("count : %d\n",count);
//         if(mtrparam[slave].dataType[i]==1)
//           phaseQty[values[count++]]=String(*(uint16_t*)vp);
//         else if(mtrparam[slave].dataType[i]==2)
//           phaseQty[values[count++]]=String(*(float*)vp,2);
//         else if(mtrparam[slave].dataType[i]==3)
//           phaseQty[values[count++]]=String(*(uint32_t*)vp);
//         else
//           phaseQty[values[count++]]=(*(uint64_t*)vp);

    
        
//         if(count < sizeof(values)/sizeof(values[0])) //fix the != to <
//         {
//           // DEBUG("count : %d\n",count);
//           DEBUG("continue\n");
//           continue;
//         }
//         else
//         {
//           DEBUG("count is 0 and cnt is true\n");
//           count=0;
//           cnt=true;
//         }
//       }
//       else
//       {
//         // DEBUG("vpins : ");DEBUG(mtrparam[slave].vPins[j]);
//         DEBUG();
//         if(mtrparam[slave].dataType[i]==1)
//           meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
//         else if(mtrparam[slave].dataType[i]==2)
//           meterData[mtrparam[slave].vPins[j]]=String(*(float*)vp,2);
//         else if(mtrparam[slave].dataType[i]==3)
//           meterData[mtrparam[slave].vPins[j]]=String(*(uint32_t*)vp);
//         else
//           meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);

//       }
//       j++;

     

//     }
//   }

//   String jsonString="";
//   // String jsonstringOneObj="";
//   serializeJson(docJson1,jsonString);
//   //DEBUG("On the whole memory usage: ");DEBUG(docJson1.memoryUsage());
//   DEBUG("json String : " + jsonString);
//   // JsonObject firstObject = docJson1[0];
//   // serializeJson(firstObject,jsonstringOneObj);
//   // DEBUG("length of one json string : ");DEBUG(jsonstringOneObj.length());//(lenof one object * numofslaves) + 2
//   // if(jsonString.length()>0 )
//   // {
//   //   if(dBot.SendDataToServer)
//   //   {
//   //     if(!dBot.storeDataToSd)
//   //       dBot.sentEnergyMeterData(jsonString);
//   //     else 
//   //     {
//   //       if (SD.cardType() != CARD_NONE && jsonString != "[]") {
//   //           DEBUG("Saving energy data to SD card queue...");
//   //           card.saveEnergyLog(jsonString); // Use the new, simple function
//   //       } else {
//   //           DEBUG("!!No SD card present in SD card slot!!");
//   //       }
//   //     }
//   //   }
//   //   else
//   //     DEBUG("!!sending data to server is blocked!!");

//   // }

//   // if(jsonString.length()>0 )
//   if(jsonString.length() > 0 && jsonString != "[]")
//   {
//     if(dBot.SendDataToServer)
//     {
//       if(!dBot.storeDataToSd)
//         dBot.sentEnergyMeterData(jsonString);
//       else 
//       {
//         // to the thread-safe card.saveEnergyLog() function.
//         DEBUG("Saving energy data to SD card queue...");
//         card.saveEnergyLog(jsonString);
//       }
//     }
//     else
//       DEBUG("!!sending data to server is blocked!!");

//   }

//   jsonString.reserve(0);
//   docJson1.clear();
//   DEBUG("readModbusJson End\n");
//   DEBUG();
//   readDataFlag=0;
// }



/** if wrong address is encountered move continue to nxt address value got jumped to nxt pin is corected */
void readMeterData::readModbusJson(uint8_t slaveNum)
{
  DEBUG("readModbusJson begin\n");

  // --- FIX: Declare common variables *once* outside the #if block ---
  DynamicJsonDocument docJson1(2048);
  JsonArray metersArray;
  JsonObject meterData;
  String jsonString = ""; // Initialize as empty
  // --- END FIX ---


  // --- START OF MODIFICATION (BLOCK 2) ---
  // This block will run *instead* of the real Modbus code if the flag is enabled.
  #if HARDCODE_ENERGY_DATA == 1
    DEBUG("[TEST] Using hardcoded energy data. No real Modbus read.");
    
    // 1. Create the array (doc is already created)
    metersArray = docJson1.to<JsonArray>();

    // 2. Create a test object for one "slave"
    meterData = metersArray.createNestedObject();
    meterData["slave_id"] = 1; // Test slave ID 1

    // 3. Get a timestamp
    if (rtcManager.isReady()) {
        DateTime now = rtcManager.now();
        char updatetime[20] = {0};
        // Format as YYYY-MM-DD HH:MM:SS
        sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", 
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
        meterData["updatetime"] = updatetime;
    } else {
        meterData["updatetime"] = "2025-10-30 16:10:00"; // Fallback timestamp
    }

    // 4. Add your sample hardcoded data (with the new V-pins)
    meterData["V3"] = String(240.5 + (random(0, 200) / 100.0), 2);
    meterData["V1"] = String(1.25 + (random(0, 50) / 100.0), 2);
    meterData["V2"] = String(0.95 + (random(0, 5) / 100.0), 2);
    meterData["V0"] = 12345.67 + (millis() / 10000.0);

    // 5. Serialize
    serializeJson(docJson1, jsonString);
    DEBUG("json String : " + jsonString);

    readDataFlag = 0; // Ensure flag is cleared
    // We will let the common saving block at the end of the function handle saving
  
  #else // This is the original code path (if HARDCODE_ENERGY_DATA is 0)
  
    // --- FIX: Remove redeclarations ---
    readDataFlag=1;
    // DynamicJsonDocument docJson1(2048); // <-- REMOVED (Declared above)
    // JsonObject meterData; // <-- REMOVED (Declared above)
    JsonObject phaseQty;
    // JsonArray metersArray; // <-- REMOVED (Declared above)

    uint8_t result;
    // uint8_t noOfSuccessCnt=0;
    uint8_t numRegToRead=0;
    bool modpollError=0;
    void *vp;
    float floatValue;
    byte slaveFailCount=0;
    byte count;
    bool timeOutFlag=0;
    bool onceSetFlag;
    uint8_t timeOutCnt=0;
    const uint8_t noOfTimeout=2;
    bool cnt=true;
    uint16_t plcData=0;
    uint32_t value_32;
    uint64_t value_64;
    //uint16_t data[2];
    metersArray = docJson1.to<JsonArray>();
    for (byte slave = 0; slave < slaveNum; slave++)
    {
      onceSetFlag=1;
      count=0;
      timeOutCnt=0;
      Serial.printf("slave : %d\n",dBot.slaveIdArray[slave]);
      //DEBUG("slave : %d\n",slave);
      node.begin(dBot.slaveIdArray[slave],Serial2);
      //node.begin(1,Serial2);

      if(dBot.wifiStatus)
        dBot.deviceContinue();
      uint8_t i,j;
      for(i=0,j=0;i<mtrparam[slave].size;i++) //sizevPins=6,size=6
      {
        
        DEBUG("reg Address: " );DEBUG(mtrparam[slave].regAddr[i]);
        DEBUG(" vpins : ");DEBUG(mtrparam[slave].vPins[j]);
        DEBUG(" count : ");DEBUG(count);
        DEBUG("reg type : " );DEBUG(mtrparam[slave].regType[i]);
        

        if(mtrparam[slave].dataType[i]==3)
          numRegToRead=2;
        else
          numRegToRead=mtrparam[slave].dataType[i];


        if(mtrparam[slave].regType[i]==1 )
        {
          DEBUG("coil Reg\n");
          result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
          // delay(200);
        }
        if(mtrparam[slave].regType[i]==2 )
        {
          DEBUG("readDiscrete Reg\n");
          result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
          // delay(200);
        }
        else if(mtrparam[slave].regType[i]==3)
        {
          DEBUG("holding Reg\n");
          // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
          result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
          // delay(200);
        }
        else if(mtrparam[slave].regType[i]==4)
        {
          DEBUG("Input Reg\n");
          // result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
          result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
          // delay(200);
        }
        
        delay(200);



        uint64_t checkStartTimer = millis();
        uint64_t checkEndTimer = checkStartTimer + modbusTimeOut;
        while (result!=node.ku8MBSuccess)
        {
          checkStartTimer=millis();
          switch (result) 
          {
            case node.ku8MBIllegalFunction:
              DEBUG("Illegal function.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal function" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
            case node.ku8MBIllegalDataAddress:
              DEBUG("Illegal data address.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data address" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
            case node.ku8MBIllegalDataValue:
              DEBUG("Illegal data value.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data value" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
            case node.ku8MBSlaveDeviceFailure:
              DEBUG("Slave device failure.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Slave device failure" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
            case node.ku8MBInvalidSlaveID:
              DEBUG("Invalid Slave ID.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid Slave ID" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
            case node.ku8MBInvalidFunction:
              DEBUG("Invalid function.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid function" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
            case node.ku8MBResponseTimedOut:
              DEBUG("Response timed out.");
              // dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Response timed out " + String(mtrparam[slave].regAddr[i]));
              break;
            case node.ku8MBInvalidCRC:
              DEBUG("Invalid CRC.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid CRC" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
            default:
              DEBUG("Unknown error.");
              dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Unknown error" + String(mtrparam[slave].regAddr[i]));
              modpollError=1;
              break;
          }

          if(modpollError)
          {
            DEBUG("error occured move to nxt data\n");
            break;
          }

          if(mtrparam[slave].regType[i]==1)
            result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
          else if(mtrparam[slave].regType[i]==2)
            result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
          else if(mtrparam[slave].regType[i]==3)
          {
            // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
            result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
            // delay(300);
          }
          else if(mtrparam[slave].regType[i]==4)
          {
            //result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
            result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
            // delay(300);
          }

          delay(200);


          /** check data until a for a certain time  */
          if(checkEndTimer<=checkStartTimer)
          {
            timeOutFlag=1;
            timeOutCnt++;
            Serial.printf("failcount: %d\n",slaveFailCount);

          
            if(slaveNum == 1)
              break;
            else if(slaveFailCount==slaveNum)
            {
              dBot.sentEnergyMeterData(noData);
              docJson1.clear();
              readDataFlag=0;
              return;
            }
            break;
          }
        }


        /** modpoll error occur mainly it will occur bcz of giving a wrong address in application ,
        * if reading a data from a slave fails go for nxt value ,like that go for all n value   */
        if(modpollError)
        {
          DEBUG("make a modpollerror to 0\n");
          modpollError=0;
          if(i != mtrparam[slave].size - 1)
          {

            if(mtrparam[slave].noParam[j]==3)
            {
              DEBUG("count : "); DEBUG(count);
              count++;

              if(count < sizeof(values)/sizeof(values[0]))
                continue;
              else
              {
                j++;
                count=0;
                if(cnt==false)
                  cnt=true;
              }
            }
            else
              j++;

            continue;
          }
          else
          {
            count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
            cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
            DEBUG("this is last data form a json\n");
            break;
          }
        }
        

        if(timeOutFlag==1)
        {
          timeOutFlag=0;
          if(timeOutCnt < noOfTimeout)
          {
            DEBUG("timeout occured \n");
            if(mtrparam[slave].noParam[j]==3)
            {
              DEBUG("count : "); DEBUG(count);
              count++;

              if(count < sizeof(values)/sizeof(values[0]))
              {
                continue;
              }
              else
              {
                j++;
                count=0;
                if(cnt==false)
                  cnt=true;
              }
            }
            else
              j++;

            continue; // reading a nxt address of current slave
          }
          else
          {
            count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
            cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
            slaveFailCount++;
            timeOutCnt=0;
            break; // moving to a nxt slave
          }
        }

        timeOutCnt=0;

        if (onceSetFlag)
        {
          onceSetFlag=0;
          meterData=metersArray.createNestedObject();
          if(dBot.storeDataToSd)
          {
            DateTime now = rtcManager.now();
            // // Format the date as YYYY-MM-DD HH:MM:SS
            char updatetime[20]={0}; //init
            sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
            meterData["updatetime"]=updatetime;
          }

          meterData["slave_id"]=dBot.slaveIdArray[slave];
        }

        /** parsing a uint16_t value  */
        if(mtrparam[slave].dataType[i]==1) 
        {
          plcData=node.getResponseBuffer(0); //read a 16 bit data (LSB)
          vp=&plcData;
          if(mtrparam[slave].operation[i]!=0)
            conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
          DEBUG(" vp value : ");DEBUG(*(uint16_t*)vp);
          // meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
        }
        
        /** parsing a float value  */
        else if(mtrparam[slave].dataType[i]==2)//float value
        {
          value_32=0;
          if(mtrparam[slave].endian[i]==1)
            value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
          else if(mtrparam[slave].endian[i]==2)
            value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
          
          memcpy(&floatValue, &value_32, sizeof(floatValue));
          
          
          vp=&floatValue;
          // if(mtrparam[slave].operation[i]!=0 && !(mtrparam[slave].operation[i]==2))
          if(mtrparam[slave].operation[i]!=0)
            conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
          DEBUG(" vp value : ");DEBUG(*(float*)vp);
        }

        /** parsing a uint32_t value  */
        else if(mtrparam[slave].dataType[i]==3)//uint32_t value
        {
          value_32=0;
          if(mtrparam[slave].endian[i]==1)
            value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
            //value_32=(node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
          else if(mtrparam[slave].endian[i]==2)
            value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
          // value_32= (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
          

          // DEBUG("lower buff : ");DEBUG(node.getResponseBuffer(0));
          // DEBUG("higher buff : ");DEBUG(node.getResponseBuffer(1));
          vp=&value_32;
          if(mtrparam[slave].operation[i]!=0)
            conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
          DEBUG(" vp value : ");DEBUG(*(uint32_t*)vp);
        }

        /** parsing a uint64_t value  */
        else if(mtrparam[slave].dataType[i]==4)//uint64_t
        {
          value_64=0;
          if(mtrparam[slave].endian[i]==1)
          {
            //for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
            for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
              value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (index*16));
          }
          if(mtrparam[slave].endian[i]==2)
          {
            // for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
            for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
              value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (48-16*index));
          }
          vp=&value_64;
          if(mtrparam[slave].operation[i]!=0)
            conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
          DEBUG(" vp value : ");DEBUG(*(uint64_t*)vp);

          // meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);
        }


        /** if virtual pin takes a ryb value eg.v1{r,y,b} */
        if(mtrparam[slave].noParam[j]==3)
        {
          if(cnt==true)
          {
            cnt=false;
            phaseQty=meterData[mtrparam[slave].vPins[j]].to<JsonObject>();
          }
          //DEBUG("count : %d\n",count);
          if(mtrparam[slave].dataType[i]==1)
            phaseQty[values[count++]]=String(*(uint16_t*)vp);
          else if(mtrparam[slave].dataType[i]==2)
            phaseQty[values[count++]]=String(*(float*)vp,2);
          else if(mtrparam[slave].dataType[i]==3)
            phaseQty[values[count++]]=String(*(uint32_t*)vp);
          else
            phaseQty[values[count++]]=(*(uint64_t*)vp);

      
          
          if(count < sizeof(values)/sizeof(values[0])) //fix the != to <
          {
            // DEBUG("count : %d\n",count);
            DEBUG("continue\n");
            continue;
          }
          else
          {
            DEBUG("count is 0 and cnt is true\n");
            count=0;
            cnt=true;
          }
        }
        else
        {
          // DEBUG("vpins : ");DEBUG(mtrparam[slave].vPins[j]);
          DEBUG();
          if(mtrparam[slave].dataType[i]==1)
            meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
          else if(mtrparam[slave].dataType[i]==2)
            meterData[mtrparam[slave].vPins[j]]=String(*(float*)vp,2);
          else if(mtrparam[slave].dataType[i]==3)
            meterData[mtrparam[slave].vPins[j]]=String(*(uint32_t*)vp);
          else
            meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);

        }
        j++;

      

      }
    }

    // --- FIX: Moved serialization and flag reset out of the #else block ---
    serializeJson(docJson1, jsonString);
    DEBUG("json String : " + jsonString);
    readDataFlag=0; // Reset flag here
  
  #endif // HARDCODE_ENERGY_DATA == 1
  // --- END OF MODIFICATION (BLOCK 2) ---


  // --- FIX: This saving block is now common to both paths ---
  if(jsonString.length() > 0 && jsonString != "[]")
  {
    if(dBot.SendDataToServer)
    {
      if(!dBot.storeDataToSd)
        dBot.sentEnergyMeterData(jsonString);
      else 
      {
        // to the thread-safe card.saveEnergyLog() function.
        DEBUG("Saving energy data to SD card queue...");
        card.saveEnergyLog(jsonString);
      }
    }
    else
      DEBUG("!!sending data to server is blocked!!");
  }

  jsonString.reserve(0);
  docJson1.clear();
  DEBUG("readModbusJson End\n");
  DEBUG();
  // readDataFlag=0; // <-- Moved up
}

// void readMeterData::rtcInit()
// {
//   DEBUG("rtc init\n");
//   if (!rtc.begin()) 
//   {
//     DEBUG("Couldn't find RTC");
//     while (1);
//   }

//   // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//   rtc.adjust(DateTime(__DATE__, __TIME__));
//   // Check if the RTC lost power and if so, set the time
//   if (rtc.lostPower()) 
//   {
//     DEBUG("RTC lost power, let's set the time!");
//     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//   }
// }

readMeterData energy;
