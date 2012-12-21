/*
  GAS Pulse example
  
 Modified emonTX for gas meter

*/

#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 9;                                                  // emonTx RFM12B node ID
const int networkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD needs to be same as emonBase and emonGLCD

const int UNO = 1;                                                      // Set to 0 if your not using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader
#include <avr/wdt.h>                                                    // the UNO bootloader 

#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
ISR(WDT_vect) { Sleepy::watchdogEvent(); }
  
typedef struct { int gasPulse, gasPulseTime, DreamOn; } PayloadGas;
PayloadGas emongas;                                                       // neat way of packaging data for RF comms

int DreamOn = 1;       //var to hold dream state
const int redLED = 4;  //PD4! is port 1

const int AIOP1 = 14 ; // analog port 1. Sometimes it's a real pain to find the right port: 
int BattAdjust = 6585 ;   //if yuo don't know, use 6600 to give you somehting to work with.

// Pulse counting settings 
int pulseCount = 0;                                                     // Number of pulses, used to measure energy.
unsigned long pulseTime,lastTime;                                       // Used to measure times.


void setup() 
{
  pinMode (AIOP1, INPUT);  // a/d converter
  digitalWrite (AIOP1, LOW);  //disable pull up
  
  Serial.begin(9600);
  Serial.println("Gas-TX KB-24Nov 12:00. "); delay (500); 
  
  rf12_initialize(nodeID, freq, networkGroup);                          // initialize RF
  rf12_sleep(RF12_SLEEP);

  attachInterrupt(1, onPulse, FALLING);                                 // interrupt attached to IRQ 1  = pin3 - hardwired to emonTx pulse jackplug. For connections see: http://openenergymonitor.org/emon/node/208
  
  if (UNO) wdt_enable(WDTO_8S); 
  
  pinMode (redLED, OUTPUT); digitalWrite(redLED, LOW); delay(500); digitalWrite(redLED, HIGH); //flash on power up
}

void loop() 
{ 
   
  emongas.gasPulse = pulseCount; 
   
  if (pulseCount > 0 ) {
     //  emontx.gasLipo= map (analogRead(AIOP1),0,1023,0,BattAdjust);   //Read the bat volts                    
           emongas.gasPulseTime = ((pulseTime - lastTime)/100);
           emongas.DreamOn = DreamOn ;
           send_rf_data();    // *SEND RF DATA* - see emontx_lib
           pulseCount=0;      // reset the counter
                        }
      
          emontx_sleep(DreamOn);   // dream for a while- see emontx_lib
          Serial.println (emongas.gasPulseTime);
 }

// The interrupt routine - runs each time a falling edge of a pulse is detected

void onPulse()                  
{
 
   pulseCount++;                //pulseCounter               
   emongas.gasPulse = pulseCount ; 
   
   lastTime = pulseTime;        //used to measure time between pulses.
   pulseTime = millis();
   if ((pulseTime - lastTime) > 20000) { DreamOn = 20 ; }   //set dream time..
   if ((pulseTime - lastTime) > 40000) { DreamOn = 40 ; }
   DreamOn = 5;                                             //using gas, so wake up sooner
   
   /*
   Serial.print("Lastime: ");
   Serial.print(lastTime);
   Serial.print(" Pulsetime :");
   Serial.println(pulseTime);
   Serial.print("Dreamtime =");
   Serial.println(DreamOn);
   */
  
 digitalWrite(redLED, LOW);
  delay(50);
 digitalWrite(redLED, HIGH);
   
}

