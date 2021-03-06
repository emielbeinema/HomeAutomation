#!/usr/bin/python3

import serial #http://pythonhosted.org/pyserial/
import time 
import queue
import threading
import struct

boudrate = 115200
devicePort = '/dev/ttyUSB0'

LIGHTSENS_bed = bytes([25])
LIGHTSENS_window = bytes([26])
LIGHTSENS_kitchen = bytes([27])
LIGHTSENS_door = bytes([28])
ROOMSENSORS = bytes([201])
SETUP_done = bytes([200])
PIRDATA = bytes([202])

def readBinary(extraSensorTask):

    def resetArduino(ser):    
        ser.setDTR(False) # Drop DTR
        time.sleep(0.022)    # Read somewhere that 22ms is what the UI does.
        ser.setDTR(True)  # UP the DTR back
        ser.flushInput()
        return
    
    now = int(time.time())
    with serial.Serial(devicePort, boudrate, timeout = 10) as ser:
        resetArduino(ser)
        #wait till setup is complete and the arduino sends the start signal
        while ser.read(size=1) != SETUP_done:
            pass
        while True:                
            header = ser.read(size=1)
            if header == LIGHTSENS_bed:
                buffer_ = ser.read(size=2)
                light = int.from_bytes(buffer_, byteorder='little', signed=False) #unsigned short
#                if light>1024:
#                  print("light:",light)
            if header == ROOMSENSORS:            
                prev = now
                now = int(time.time())
                buffer_ = ser.read(size=18)
                temp_bed            = int.from_bytes(buffer_[0:2], byteorder='little')
                temp_bathroom       = int.from_bytes(buffer_[2:4], byteorder='little')
                humidity_bed        = int.from_bytes(buffer_[4:6], byteorder='little')
                humidity_bathroom   = int.from_bytes(buffer_[6:8], byteorder='little')
                co2                 = int.from_bytes(buffer_[8:10], byteorder='little')
                light_bed           = int.from_bytes(buffer_[10:12], byteorder='little')
                print("sensors:",temp_bed, temp_bathroom, humidity_bed,
                      humidity_bathroom, co2, light_bed, "dtime:", now-prev)
 
#            #if there are no headers the message must be a PIR status update
#            elif header == PIRDATA:    
#                processPIR(ser.read(size=2))

            if not extraSensorTask.empty():
                request = extraSensorTask.get()
                ser.write(request)           
    return

def processPIR(pirValues):
    pass


def processLight():
    pass











def init():
    pass
    
extraSensorTask = queue.Queue()

def timeReadSpeed(extraSensorTask):
    #read in analyse for short latency requests and ouput all data to buffer, 
    #then after 10 seconds put the buffer into a Queue to be further processed

    i = 0
    b = ['none']
    with serial.Serial(devicePort, boudrate, timeout=2) as ser:
        time.sleep(4)
        ser.flushInput()
        program_starts = time.time()
        while True and i < 1000:
            if not extraSensorTask.empty():
                request = extraSensorTask.get()
#                print('sending request:',request)
                ser.write(request)
            output = ser.readline()
            if output != b'm0\n':  #if something happening not sensor stdby
                process(output)
            i += 1
    now = time.time()
    print("Time between updates: {0} milli second".format((now - program_starts)))
    return

def timePIR(extraSensorTask):
    #read in analyse for short latency requests and ouput all data to buffer, 
    #then after 10 seconds put the buffer into a Queue to be further processed
    def putFast(extraSensorTask):
        while True:
            extraSensorTask.put(b'00c')
            time.sleep(2)
        return

    t = threading.Thread(target = putFast, 
                         args   = (extraSensorTask,))
    t.start()


    i = 0
    deltaT = 0
    with serial.Serial(devicePort, boudrate, timeout=2) as ser:
        time.sleep(4)
        ser.flushInput()
        t1 = time.clock()
        while True and i < 10:
            if not extraSensorTask.empty():
                request = extraSensorTask.get()
                ser.write(request)
            output = ser.readline()
#            print("output:",output)
            if b"\n" in output:
                t0 = t1
                t1 = time.clock()
                if t1-t0 > deltaT:
                    deltaT = t1-t0                  
            if b'h' in output: #if reply contains shit
                i += 1
    print("Max time between movement data recieved: {} milli seconds".format(deltaT*1000))
    return    

def timeACC(extraSensorTask):
    #read in analyse for short latency requests and ouput all data to buffer, 
    #then after 10 seconds put the buffer into a Queue to be further processed


    i = 0
    deltaT = 0
    sample = 100
    with serial.Serial(devicePort, boudrate, timeout=2) as ser:

        time.sleep(2)        
        ser.write(b'02') #switch to fast polling       
        time.sleep(4)
        
        ser.flushInput()
        program_starts = time.time()
        t1 = time.clock()
        while True and i < sample:
            if not extraSensorTask.empty():
                request = extraSensorTask.get()
                ser.write(request)
            output = ser.readline()
            if b"error" in output:
                t0 = t1
                t1 = time.clock()
                if t1-t0 > deltaT:
                    deltaT = t1-t0                  
                i += 1
                print(output)

    now = time.time()
    print("Max time between accl data recieved: {} milli seconds".format(deltaT*1000))
    print("avg between accl data: {0} second".format((now - program_starts)/sample))
    return    

def timeSHT(extraSensorTask):
    #read in analyse for short latency requests and ouput all data to buffer, 
    #then after 10 seconds put the buffer into a Queue to be further processed
    def putFast(extraSensorTask):
        while True:
            extraSensorTask.put(b'00c')
            time.sleep(0.5) #keep this lower then the avg time
        return

    t = threading.Thread(target = putFast, 
                         args   = (extraSensorTask,))
    t.start()


    i = 0
    with serial.Serial(devicePort, boudrate, timeout=2) as ser:
        program_starts = time.time()
        while True and i < 10:
            if not extraSensorTask.empty():
                request = extraSensorTask.get()
                print('sending request:',request)
                ser.write(request)
            output = ser.readline()
#            print(output)         
            if b'h' in output: #if reply contains shit
                i += 1
    now = time.time()
    print("avg between SHT data: {0} second".format((now - program_starts)/10))
    return
    
            
def read(extraSensorTask):
    #read in analyse for short latency requests and ouput all data to buffer, 
    #then after 10 seconds put the buffer into a Queue to be further processed

    with serial.Serial(devicePort, boudrate, timeout = 1) as ser:
        time.sleep(4) #allow the arduino to initialise
        ser.flushInput()
        while True:
            output = ser.readline()
            if not extraSensorTask.empty():
                request = extraSensorTask.get()
                ser.write(request)           
            if output != b'\n':  #if something happening not sensor stdby
                process(output)    
    return
    
    
def lamptrigger():
#    print('lamps activated for a certain timeout')
    pass

def process(output):
    #get new data from the queue and store it in a global variable and save it 
    #to the disk in binairy form
    print(output)
#    if output == b'm1\n':
#        lamptrigger()
#    elif b't' in output:#if the first part of output is letter t 
#        print("temperature is:")
#        print(output)
#        pass
    return


def queueput(extraSensorTask):
#   low arduino load optimised code for requesting data:
#   diget of number defines if we are requesting sensor data
#   or controlling something (0 or 1), then a number for the sensor to request
#   this number can be 2 digets
    time.sleep(5)
    while True:
        extraSensorTask.put(b'0')
        time.sleep(5)
    return    


t = threading.Thread(target = readBinary, 
                     args   = (extraSensorTask,))
t.start()
queueput(extraSensorTask)


