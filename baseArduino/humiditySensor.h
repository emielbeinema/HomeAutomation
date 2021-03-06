#ifndef HUMIDITYSENSOR_H
#define HUMIDITYSENSOR_H

#include <Arduino.h> //needed for Serial.print

//type declaration for saving space and sanity in passing functions
typedef void(*f1_type)(byte PIRs[2]);
typedef void(*f2_type)(signed short int sensorData[9], byte PIRs[2], 
                       byte rqUpdate1[1], byte rqUpdate2[1]);
typedef void(*f3_type)(signed short int sensorData[9]);

class TempHumid
{
  public:
    TempHumid(int dataPin, int clockPin);
    void readPIR();
    float readTemperatureC(f1_type f1, f2_type f2, f3_type f3,
                           signed short int sensorData[9], byte PIRs[2], 
                           byte rqUpdate1[1], byte rqUpdate2[1]);
    
    float readHumidity(float tempC, f1_type f1, f2_type f2, f3_type f3, 
                       signed short int sensorData[9], byte PIRs[2], 
                       byte rqUpdate1[1], byte rqUpdate2[1]);
  private:
    int _dataPin;
    int _clockPin;
    void skipCrcSHT(int _dataPin, int _clockPin);
    int getData16SHT(int _dataPin, int _clockPin);
    void sendCommandSHT(int _command, int _dataPin, int _clockPin);  
    
    float readTemperatureRaw(f1_type f1, f2_type f2, f3_type f3, 
                             signed short int sensorData[9], byte PIRs[2], 
                             byte rqUpdate1[1], byte rqUpdate2[1]);
                                 
    void waitForResultSHT(int _dataPin, f1_type f1, f2_type f2, f3_type f3, 
                          signed short int sensorData[9], byte PIRs[2], 
                          byte rqUpdate1[1], byte rqUpdate2[1]);
};

static const unsigned char PIRDATA2 = 202;//TODO might need removing when fast polling data
//function is implemented

const short PIN_TERM_DATA = 0b00000100;
const short PIN_TERM_CLOCK = 0b00000001;
const short PINS_OFF = 0b00000000;


//this prevents problems if someone accidently #include's your library twice.
#endif

