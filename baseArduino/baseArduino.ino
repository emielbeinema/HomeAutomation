#include <Wire.h>
#include <SPI.h>
#include "RF24.h"
#include "printf.h"

#include "humiditySensor.h"
#include "accelerometer.h"

//the "union" construct is useful, in which you can refer to the 
//same memory space in two different ways
typedef union
{
  int number;
  uint8_t bytes[2];
} INTUNION_t;

// Specify data and clock connections
static const int term_dataPin = 24; //PB2
static const int term_clockPin = 22; //PB0

// Specify signal connections
static const int pir_signal = 49;
static const int light_signal = 0; //anolog

// Radio connections
static const int CEPIN = 7;
static const int CSPIN = 9;
static const uint8_t ADDRESSES[][4] = { "1No", "2No", "3No" }; // Radio pipe addresses 3 bytes
//is the absolute minimum

// script setup parameters
static const int readSpeed = 1; //time between reading individual chars
static const int debugSpeed = 0; //time between reading and reply-ing used for debug
static const int resetSpeed = 0; //time for the connection to reset
static const int calibrationTime = 2000; //setup wait period

static const byte REQUESTCO2[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; 

// headers for diffrent data
// light sensors, 25 - 50 three bytes long including header
// room sensors package, 101
// pir packages are always 2 bytes long and never have a header

static const unsigned char SETUP_done = 200; //light sensor under the bed

static const unsigned char LIGHTSENS_bed = 25; //light sensor under the bed
static const unsigned char LIGHTSENS_window = 26;
static const unsigned char LIGHTSENS_kitchen = 27;
static const unsigned char LIGHTSENS_door = 28;

static const unsigned char PIRDATA = 202;//
static const unsigned char ROOMSENSORS = 201; //light sensor under the bed

int buffer[3];
int bufferLen = 0;

static const unsigned char NODE1_PIR = 1;
static const unsigned char NODE1_TEMP = 't';
static const unsigned char NODE1_TEMP_RESEND = 'd';

static const unsigned char NODE2_PIR = 2;
static const unsigned char NODE2_LIGHT = 'l';
static const unsigned char NODE2_LIGHT_RESEND = 'i';


byte rqUpdate1[1] = {NODE1_PIR};//non const as we change this to respect if we recieved
//the temperature yet, when we have not yet recieved it it's 'd' otherise its 1
byte rqUpdate2[1] = {NODE2_PIR};

int lightCounter = 0;
int accCounter = 0;
int debugCounter= 0;
int accPeriod = 200;


byte PIRs[2]; //stores pir data, first byte stores if a sensor has detected 
//a signal (1 = yes, 0 = no) second byte stores which sensors are included in
//this readout, [bedSouth, bedNorth, bathroomWest, bathroomEast, door, krukje,
//trashcan, heater 



static const signed short int SENSORDATA_SIZE = 9;
static const signed short int sensorData_def[SENSORDATA_SIZE] = {32767,32767,32767,32767,32767,32767,0,0,0};
signed short int sensorData[SENSORDATA_SIZE] = {32767,32767,32767,32767,32767,32767,0,0,0};
//initialised as 32767 for every value stores: temp_bed, temp_bathroom, 
//humidity_bed, humidity_bathroom, co2, light_bed, light_outside, light_door, 
//light_kitchen, whenever data is send we reset to this value


//runs constructor to make opjebt test of class tempHumid from humiditySensor.h
TempHumid thSen (term_dataPin, term_clockPin);
Accelerometer acSen;
RF24 radio(CEPIN,CSPIN); //Set up nRF24L01 radio on SPI bus plus cepin, cspin

//needed for passing function to a class, dont know why its needed though..
void readAcc(){
  acSen.readOut();
}

void readLocalPIRs(byte PIRs[2]){
  //check the PIR sensor for movement as fast as possible, this happens
  //many many times a second
  
  //read registery of pin bank L (fast way to read state), 
  //returns byte on is high bit off is low. See this chart for which bit in the 
  //byte corrosponds to which pin http://forum.arduino.cc/index.php?topic=45329.0
  delay(1);//crashes if removed  TODO checkthis!!!
  if ((PINL & 1) != 0){
    PIRs[0] = PIRs[0] | 0b10000000;  //set bedSouth value to recieved data
    PIRs[1] = PIRs[1] | 0b10000000;  //indicate bed sensor has been read
  }
}

void readLight(signed short int sensorData[SENSORDATA_SIZE]){
  //read light sensor (anolog) and return over serial, this happens multiple times
  //a second, convert the data to binairy and send using hte following format:
  //[header for this light sensor (see the top of file)][lightLevel byte 1]
  //[light level byte 2]
  
  INTUNION_t light;
  
  light.number = analogRead(light_signal);    // read the input pin
  sensorData[5] = light.number;
  //Serial.write(LIGHTSENS_bed);
  //Serial.write(light.bytes[0]);
  //Serial.write(light.bytes[1]);
}


int readCO2(Stream& sensorSerial){
  //reads awnser from Co2 sensor that resides in the hardware serial buffer
  //this can be called some time after reqeusting the data thus it is not 
  //needed to wait for a reply after the request, Will also not 
  //block if no reply present but return -1 instead

  char response[9]; //changed was char, byte is better prevents issue with char
                    //in reading out the array in hex (like values such as 
                    //FFFFFFF86 instead of 86) //again to char since makefile
                    //wants it to be char really badly
  int responseHigh;
  int responseLow;
  int ppm;
  
  while (sensorSerial.available() > 8){
    if (!sensorSerial.read() == 0XFF){//check if reply from sensor in buffer
      sensorSerial.read();
    }
    else{
      sensorSerial.readBytes(response, 8);

      responseHigh = (int) response[1];
      responseLow = (int) response[2];
      ppm = (256*responseHigh)+responseLow;
      return ppm;
    }
  }
  return -1;
}    

void processRemoteLight(signed short int sensorData[SENSORDATA_SIZE], byte rcbuffer[5]){
  //copy data from radio buffer back to integers and store it in the SensorsData
  //array to be send later by sendSensorsdata if the array is complete
  
  INTUNION_t light_door, light_kitchen;
  
  memcpy(light_door.bytes, rcbuffer, 2); 
  memcpy(light_kitchen.bytes, rcbuffer+2, 2); //copy from buffer[2] t/m buffer[3]
  
  sensorData[7] = light_door.number;//set to the int representation of the 2 byte array
  sensorData[8] = light_kitchen.number;//TODO remove this (do this while rewriting for    
}

void processRemoteTemp(signed short int sensorData[SENSORDATA_SIZE], byte rcbuffer[5]){
  //copy data from radio buffer back to integers and store it in the SensorsData
  //array to be send later by sendSensorsdata if the array is complete
  
  INTUNION_t temp_c, humidity;
  
  memcpy(temp_c.bytes, rcbuffer, 2); 
  memcpy(humidity.bytes, rcbuffer+2, 2); //copy from buffer[2] t/m buffer[3]
  
  sensorData[1] = temp_c.number;//set to the int representation of the 2 byte array
  sensorData[3] = humidity.number;//TODO remove this (do this while rewriting for    
}

void checkWirelessNodes(signed short int sensorData[SENSORDATA_SIZE], byte PIRs[2],
                        byte rqUpdate1[1], byte rqUpdate2[1]){
  byte rcbuffer[5];
  
  //request data from the bathroom sensors (pipe 1)
  radio.openWritingPipe(ADDRESSES[1]);
  radio.write(rqUpdate1, 1 ); //write rqUpdate to the currently opend writingPipe  
  if(radio.available() ){
    radio.read( &rcbuffer, 5 );//empty internal buffer from awk package
    
    //check if temp data is present and we still have an outstanding request 
    //for temp data.
    if (rcbuffer[0] != 255 && rqUpdate1[0] == NODE1_TEMP_RESEND){
      rqUpdate1[0] = NODE1_PIR;//do no longer request temp data and indicate we have no
      //outstanding request
      processRemoteTemp(sensorData, rcbuffer);   
    }
    //package only contains PIR data 
    else{
      PIRs[0] = PIRs[0] | rcbuffer[4]; //set bathroomsensor values to recieved data
      PIRs[1] = PIRs[1] | 0b00110000;  //indicate bathroom sensors have been read   
    }
  }

  //request data from the kitchen sensors (pipe 2)
  radio.openWritingPipe(ADDRESSES[2]);
  radio.write(rqUpdate2, 2 ); //write rqUpdate to the currently opend writingPipe  
  if(radio.available() ){
    radio.read( &rcbuffer, 5 );//empty internal buffer from awk package
    
    if (rcbuffer[0] != 255 && rqUpdate2[0] == NODE2_LIGHT_RESEND){    
      rqUpdate2[0] = NODE2_PIR;
      processRemoteLight(sensorData, rcbuffer);
    }
    else{
      PIRs[0] = PIRs[0] | rcbuffer[4]; //set bathroomsensor values to recieved data
      PIRs[1] = PIRs[1] | 0b00110000;  //indicate bathroom sensors have been read   
    }
  }
}

  
void readRoomSensors(signed short int sensorData[SENSORDATA_SIZE], byte PIRs[2],
                     byte rqUpdate1[1], byte rqUpdate2[1]){
  //Read temperature, humidity and co2 wired to this device and
  //store the outcome to be reported over serial later by sendSensorsdata
  //light data though remote is not polled as this is done on a frequent basis
  //it is polled way more often then this should be called.
  //Function needs PIRs data since it also takes over pir and light checking
  //while it runs
  
  float humidity;
  float temp_c;

  static const byte rqTemp[1] = {NODE1_TEMP};
  static const byte rqLight[1] = {NODE2_LIGHT};

  //request the values of the other sensors in the room  
  radio.openWritingPipe(ADDRESSES[1]);
  radio.write( rqTemp, 1 ); //write len 1 to the currently opend writingPipes
  rqUpdate1[0] = NODE1_TEMP_RESEND;// = ascii 100 indicates we still have an outstanding request for
  //temperature

  radio.openWritingPipe(ADDRESSES[2]);
  radio.write( rqLight, 1 ); //write len 1 to the currently opend writingPipes
  rqUpdate2[0] = NODE2_LIGHT_RESEND;// = ascii 100 indicates we still have an outstanding request for
  //temperature
  
  //geather data from the local sensors
  Serial1.write(REQUESTCO2,9);// request the CO2 sensor to do a reading
  temp_c = thSen.readTemperatureC(readLocalPIRs,checkWirelessNodes,readLight,
                                  sensorData, PIRs, rqUpdate1, rqUpdate2);
  humidity = thSen.readHumidity(temp_c, readLocalPIRs,checkWirelessNodes,
                                readLight,sensorData, PIRs, rqUpdate1, rqUpdate2);
  
  sensorData[0] = int(temp_c*100);
  sensorData[2] = int(humidity*100);
  
  sensorData[4] = int(readCO2(Serial1) );
}

void sendSensorsdata(signed short int sensorData[SENSORDATA_SIZE]){
  //used to send the data to the raspberry pi 
  //when the sensorArray has been filled
  
  INTUNION_t toSend;
  
  //header that announces the data format
  //Serial.write(ROOMSENSORS);
  Serial.println("");//FIXME
  for (unsigned int i = 0; i < SENSORDATA_SIZE; i++){
  //send 16 bit integers over serial in binairy
    Serial.println(sensorData[i]);//FIXME
    toSend.number = sensorData[i];    
    //Serial.write(toSend.bytes[0]);
    //Serial.write(toSend.bytes[1]);
  }
  
  //reset sensorData to default values so we can easily check if it is complete
  memcpy(sensorData, sensorData_def, SENSORDATA_SIZE);
}

void setup()
{ 
  Serial.begin(115200); //Open serial connection to report values to host
  Serial1.begin(9600);  //Opens the second serial port with a baud of 9600 
                       //connect TX from MH Co2 sensor to TX1 on arduino etc
  printf_begin();
 
  //initialising and calibrating accelerometer
  acSen.setup();
 
  //initialise radio
  radio.begin();

  radio.setAddressWidth(3);               //sets adress with to 3 bytes long
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.enableAckPayload();               // Allow optional ack payloads
  radio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
  radio.setPayloadSize(5);                // Here we are sending 1-byte payloads to test the call-response speed
  
  //radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);// 2.508 Ghz - Above most Wifi Channels

  radio.printDetails();                   // Dump the configuration of the rf unit for debugging
    
  //give the pir sensor some time to calibrate
  delay(calibrationTime);
  Serial.print("setup done, starting response loop\n");
  radio.stopListening();

  //Serial.write(SETUP_done);
}


void loop(){
  // serial read section
  while (Serial.available()){ // this will be skipped if no data present, leading to
                              // the code sitting in the delay function below
    delay(readSpeed);  //delay to allow buffer to fill 
    if (Serial.available() >0)
    {
      int c = Serial.read(); //gets one byte from serial buffer
      if (c == 99){
        break;
      }
      buffer[bufferLen] = c;
      bufferLen++;
    }
  }

  if (bufferLen >0) {
    switch(buffer[0]){
      case 48: //acii 0
        readRoomSensors(sensorData, PIRs, rqUpdate1, rqUpdate2);//requests the remote sensor values
        //and reads in the local sensors
        break;
      case 49: //acii 1
        //nothing         
        break;
      case 50: //acii 2
        accPeriod = 1;  //fast polling    
        break;
      case 51: //acii 3
        accPeriod = 200;  //slow polling             
        break;
      case 52: //acii 4               
        break;
      default:
        //TODO replace with error code
        break;
    }//switch
  }//if

  bufferLen = 0;//empty the string


  //read local sensors
  if (lightCounter > 10) {
    readLight(sensorData);
    lightCounter = 0;
    }
  lightCounter++;

  //TODO DEBUGG measure temp periodically
  if (debugCounter > 1000) {
    readRoomSensors(sensorData, PIRs, rqUpdate1, rqUpdate2);//requests the remote sensor values
    debugCounter = 0;
    }
  debugCounter++;

//disabled till we can filter away the noise  
//  if (accCounter > accPeriod) {
//    acSen.readOut();
//    accCounter = 0;
//    }
//  accCounter++;    
  
  //read remote sensors
  checkWirelessNodes(sensorData, PIRs, rqUpdate1, rqUpdate2);
  
  //check if all data has been collected
  bool rdyToSend = true;
  for (unsigned int i =0; i < SENSORDATA_SIZE; i++){
    if(sensorData[i] == 32767){ //if the element is the default value not all
      rdyToSend = false;//data has been collected and we are not rdy to send
    }
  }

  if (rdyToSend){
    sendSensorsdata(sensorData);
  }

  readLocalPIRs(PIRs); 

  //send PIR data reset updated pirs byte for new loop
  //Serial.write(PIRDATA);
  //Serial.write(PIRs[0]);//TODO
  //Serial.write(PIRs[1]);
  PIRs[1] = 0; //reset the "polled PIR's record"
  
  delay(resetSpeed);
}
