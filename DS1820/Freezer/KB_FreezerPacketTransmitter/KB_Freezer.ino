/*
 ---12-12-2012!!!
 
 Modified emonTX for DS18B20 monitor,  this is just basic setup to see if it all transmits data sensibly.
 (I have 18B20's, not tested with other versions.)
 
 The inital code comes from different examples, but I did not keep track of what I initially borrowed where.
 If I use some of your code, thanks! If I modified it: thanks for the initial code, if it is all mine: no need to thank me.

 Temp is running as is. (see rev Freezer04).
 I see my temp jumping in 0.06 degrees teps. Looks like it's working on 12 bits. Just slow down the readings due to inherent warming up becasue of 
 chip activity.
 
 Many thanks to: 
 Dallastemperaturecontrollibrary code was started from one of the examples here.
 http://www.jeelabs.org
 http://www.openenergymonitor.org
 http://mbed.org/users/snatch59/notebook/onewirecrc
 and many more.
 (brewpi.com might have good filters if you have noise problems.)
*/

// Some includes to make the compiler happy.

#include <avr/wdt.h>                                                    
#include <JeeLib.h>   
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port P1, DIO of a Jeenode v6. 
#define ONE_WIRE_BUS 4                 //  arduino pin4 is actually 6 on Jeenode P1
#define TEMPERATURE_PRECISION 12       // 9,10,11 or 12 bits? It works on 12!

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Array to hold device addresses, adjust for total number of sensors! This contains the hex addresses of the sensors. 
// I have no idea how many I can string together, for me a sensible amount is 3 or 4. 

DeviceAddress DS18B20_1, DS18B20_2, DS18B20_3, DS18B20_4 ;

// Jeenode setup

#define freq RF12_868MHZ                                                
const int nodeID = 16;                             // get a unique number on your network.                                       
const int networkGroup = 210;                                           
const int UNO = 1;                                                      
                                                   // Download JeeLib: http://github.com/jcw/jeelib
ISR(WDT_vect) { Sleepy::watchdogEvent(); }         // watchdog 
  
typedef struct { float T1, T2, T3, T4;  } PayloadTmp;  // this needs to be the same as on the transmitting side. 
PayloadTmp montmp;                                 // neat way of packaging data for RF comms

// end Jeenode setup


//---------------------------------------------

void setup() {
  // start serial port
        Serial.begin(9600);
        Serial.println("Test multiple Dallas 1820 Sensors");

        rf12_initialize(16,RF12_868MHZ,210); // NodeID, Frequency, Group
        rf12_sleep(RF12_SLEEP);              // and go to sleep. 
  
  // Start up the sensors library, must be called before search()
        sensors.begin();

  // Locate devices on the bus, only on startup. Find a method to send a panic message if I have less then expected!
  
        Serial.print("Locating devices...");
        Serial.print("Found ");
        Serial.print(sensors.getDeviceCount(), DEC);
        Serial.println(" devices.");

  // report parasite power requirements, really not that important, leave code for now.
        Serial.print("Parasite power is: "); 
        if (sensors.isParasitePowerMode()) Serial.println("ON");
        else Serial.println("OFF");

  // Must be called before search()?
        oneWire.reset_search();
      // assigns the addresses found to T1,2,3. GoodTrick, search and assign and if not found all in one ;-)
      
          if (!oneWire.search(DS18B20_1)) Serial.println("Unable to find address for Thermometer_1");
          if (!oneWire.search(DS18B20_2)) Serial.println("Unable to find address for Thermometer_2");
          if (!oneWire.search(DS18B20_3)) Serial.println("Unable to find address for Thermometer_3");
          if (!oneWire.search(DS18B20_4)) Serial.println("Unable to find address for Thermometer_4");
           // set the resolution to 12 bit!  (because we can)
  
          sensors.setResolution(DS18B20_1, TEMPERATURE_PRECISION);
          sensors.setResolution(DS18B20_2, TEMPERATURE_PRECISION);
          sensors.setResolution(DS18B20_3, TEMPERATURE_PRECISION);
          sensors.setResolution(DS18B20_4, TEMPERATURE_PRECISION);
          
  // nice to know the addresses if you need to label the sensors. Add one at the time to find it's address, or use a seperate sketch with only one.
  // but do label them!
  
  //  code below  can be removed when everything works. 
      Serial.print("Therm_1 Address: "); printAddress(DS18B20_1); Serial.println();
      Serial.print("Therm_2 Address: "); printAddress(DS18B20_2); Serial.println();
      Serial.print("Therm_3 Address: "); printAddress(DS18B20_3); Serial.println();   
      Serial.print("Therm_4 Address: "); printAddress(DS18B20_4); Serial.println();   
  
} // end of setup


//----------all the functions from the original code, only use what you actually need. But I guess the compiler is smart enought not to include stuff you don't need. 

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
   Serial.print("Temp C: ");
   Serial.print(tempC);
}


// function to actually return the temp to the outside world. (use the 'return'!)
float SensorTemperature (DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  return tempC;
}



// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();    
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
    Serial.print("DevAddress: ");
    printAddress(deviceAddress);
    Serial.print(" ");
    printTemperature(deviceAddress);
    Serial.println();
}

//------------------------------------------------------

void loop() 
{ 
  
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus

    Serial.print("Requesting temps...");
    sensors.requestTemperatures();    // This is all there is to it.
    Serial.println("DONE");
    
  
    // put the valuse into the array for transmitting
    montmp.T1=SensorTemperature (DS18B20_1);  
    montmp.T2=SensorTemperature (DS18B20_2);
    montmp.T3=SensorTemperature (DS18B20_3);
    montmp.T4=SensorTemperature (DS18B20_4);
    
    // for the human..
     Serial.print("T1=");
     Serial.println(montmp.T1); 
     Serial.print("T2=");
     Serial.println(montmp.T2);
     Serial.print("T3=");
     Serial.println(montmp.T3);
     Serial.print("T4=");
     Serial.println(montmp.T4);
    
    // Serial.println(sizeof montmp);   // in this instance we transmit 8 bytes (3 sensors)
     delay(1000);   // wait at least 5 secs between readings, otherwise the devices warm up too much and will affect the temp it reads.
                     // especially at high resolution and low temperatures. 
 
   send_rf_data();    // *SEND RF DATA* - see emontx_lib
   
   montmp_sleep(10);   // sleep or delay in seconds - see emontx_lib
  
}

