/*
  EmonTx Pulse
  Kees Blokland, dec 2012
  

ToDo: Not really happy with the amount of pulses and resultant power values. 
What happens if I do collect for a minute in stead of 10 sec?

1 Dec trying to improve accuracy, I'm measuring more here then on the meter. 
  Make measurement time 60 secs to get a few more pulses. Takes avg over n pulses. 
  might have tu=o use n-1 pulses though! 

The magic formula explained.
To measure pulse during a longer timeframe, I need to have take the lastTime outside the interrupt loop.
Then the interrupt can increase the pulseCount, and the time of the last pulseTime.
I now have x pulses within my 60 sec frame, with an accurate start and stop time.



Filename:  emonTx_KB_1DecA


*/

#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 10;                                                  // emonTx RFM12B node ID
const int networkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD needs to be same as emonBase and emonGLCD

const int UNO = 1;                                                      // Set to 0 if your not using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader
#include <avr/wdt.h>                                                    // the UNO bootloader 

#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
ISR(WDT_vect) { Sleepy::watchdogEvent(); }
  
typedef struct { int power, npulse; unsigned long tpulses; } PayloadTX;            // calculated power, number of pulses, time between first and last pulse in this frame .
PayloadTX emontx;                                                       // neat way of packaging data for RF comms

const int LEDpin = 9;  

// Pulse counting settings 
int pulseCount = 0;                                                     // Number of pulses, used to measure energy.
unsigned long pulseTime,lastTime,time_between_pulses ;   // Used to measure power.
int power  ;                                                          // power and energy
//int ppwh = 500; //   500 pulses/kwh = 0.5 pulse per wh - Number of pulses per wh 
int misc = 0;



void setup() 
{
  pulseTime = micros();  // don't start empty
  
  Serial.begin(9600);
  Serial.println("emonTx_KB_2Dec B "); delay (500);          
  
  rf12_initialize(nodeID, freq, networkGroup);   // initialize RF
  rf12_sleep(RF12_SLEEP);                        // measurement time just smoothes the curves, does not effect values!
                                                

 
  pinMode(LEDpin, OUTPUT);                       // Setup indicator LED
  digitalWrite(LEDpin, HIGH);
  
  attachInterrupt(1, onPulse, FALLING);          // KWH interrupt attached to IRQ 1  = pin3 - hardwired to emonTx pulse jackplug. For connections see: http://openenergymonitor.org/emon/node/208
  
  if (UNO) wdt_enable(WDTO_8S); 
 
}

void loop() 
{ 
  emontx.npulse = pulseCount; pulseCount=0;      //load pulsecount into pulse
  
  lastTime = pulseTime;                          //used to measure time between pulses. set counter to last pulse received.
   
  send_rf_data();                                // *SEND RF DATA* - see emontx_lib
  digitalWrite(LEDpin, LOW); delay(50); digitalWrite(LEDpin, HIGH);     
  
  emontx_sleep(60);                               // sleep or delay in seconds - see emontx_lib
  
}



// The interrupt routine - runs each time a falling edge of a pulse is detected
void onPulse()                  
{

  pulseTime = micros() ;  // time of interrupt.
  pulseCount++ ;                //pulseCounter , add one              
  emontx.tpulses = (pulseTime - lastTime) ;  // time between this one and last, in case I have to send.
  //emontx.power = int(3600000000.0 / ((emontx.tpulses/pulseCount-1)/2));  
     emontx.power = int(7200000000.0 / (emontx.tpulses/pulseCount-1));
  digitalWrite(LEDpin, LOW); delay(5); digitalWrite(LEDpin, HIGH); 
}

