/*
Energy monitoring, basically designed to allow me to track down high power appliances.
Used together with OpenEnergyMonitor software for the all important long time store of data.
This code might work, or not, feel free to bend it your way.
Kees Blokland.

I don't have a spare jeenode, so TotalGas is not tested yet. 
Initial coding..

Added RTC, but RTC does not work on a single lipo, the rest of the stuff does, 
Added lipo voltage measurement.  
This code runs on a jeenode v6, with a GLCD board and RTC plug (other board might need fixing).
--------12Nov. happy with this code, now try to keep the T1/2 vals by writing them to a mem location.
EEProm write date, use ==0 to check a val, not = 0 !  NOOB
------- 14 nov. (18 works for sure, this was just a cleanup, so should be ok too. Don't want to stop the readouts now!
Fixed bug when x.xxx kW comes to 1000. It waited till it was 1002 before changing. Now comparing to ==1000, not > 1000
--------15 Nov: Once reading goes over 1000, it dus not reset the counter, but keeps incrementing. now up to 6554 in stead of 6.554, the '6' has been 
Added to the main amount, but it does not store the updated main value! 
--------16 Nov: added >= 1000 (in case of more then one pulse) to cure this, because it is still not working right. 
Added a 'set everything to 0 option'
--------17 nov:
Beware of too many print statements, you will run out of RAM and get weird results!
Running low on RAM, so you might have to disable some debug sections
Added override switch, in case you have to force writing new meter values. (as when you are running out of sync.)
--------18 Nov
Counts are going to the wrong counter: T1= 6255 actual, reads 6258. T2 actual 10349, reads 10346.
It's a weekend, so this does not work! Needs to figure out the day and count accordingly.
Sunday 0830: T2 (62xx) is increasing. Replaced all strings with T1/T2. Maybe it was just the final print to the GLCD.. Watch it.
Add weekend check, so that counts get added to right counter, Weekends is only low tariff.
Rewrote all code using functions. Looks a lot cleaner now. 
Lost node on the screen? Due to function use?
-------20 Nov
Small error in deciding when the tarif start/stops. now[3] < 23) || (now[3] > 6) is 22:59 --7:00 and ((now[3] > 22) || (now[3] < 7)
23:00-- 6:59
-------21 nov: TotalGas node is tx'ing 
Helps if you actualy define and process the Gas stuff.
After a packet is received only relevant node data is printed, I want it all!
Read lGas lipo seperately.. Or make an alarm. (flashing "L"?
Changed all voids' to static void.. Fixed the printing alright! also use fprint %u  %ul  %d  %s  where needed. Read the source!
.Lipo is not on the right port in the TX I guess.. Must be on the same port as LED. Shocks, I have not soldered anything on it!
-----23 nov: On power up T1=6271, T2=6273.. looks like T2 gets stored in wrong location! /readback-loop swapped? 
Do check the T1/T2>> locations in EEProm. It's Not Right Yet.

ToDo
Make the m3 with a small '3'
Add Flashing L to display when lipo is low.
node display mystery, can't get it to work form a 'void'
listen to internet time node and show difference, if really big, adjust!
Lipo is not on the right port in the TX I guess.. Must be on the same port as LED. Shocks, I have not soldered anything on it!


*/

//---------Libraries---------
#include <EEPROM.h>
#include <GLCD_ST7565.h>
#include <RTClib.h>
#include <Wire.h>                 // needed to avoid a linker error :(
#include <JeeLib.h>
#include <avr/pgmspace.h>
#include "utility/font_clR6x8.h"

GLCD_ST7565 glcd;
//---------end Libraries---------

//---------- Debugging ---------
int Debug= 0;  // debug flag, set 0 for final use. Be carefull, I'm running low on RAM, so expect funny things to happen.
int SetZero =0 ;  // when 1, write 255's to all 6 locations. (is 'zero') 
//---------End Debugging ---------

//--------Put initial meter readings here----------
// Nuon cheap tarif is 2300-0700 and official holidays. 1 januari, tweede paasdag, Hemelvaartsdag, tweede pinksterdag, eerste en tweede kerstdag.
// probably better to make a function to decide if it is weekend or a special day.

unsigned int T2 = 10367;  // T2  inititally put the present meter reading here. Low tarif?
unsigned int T1 = 6291;   // T1 inititally put the present meter reading here.
unsigned int TotalGas = 53293 ;   // 
//unsigned TotalGasM = 530 ; // digits after komma.  store these too?
int inc_gas_pulse      = 0;  // place for emongas.TotalGasPulse read at least 2  digits and enter xx0 here. 
boolean OverRide   = 0;  // override EEprom values, force writing these.
boolean Weekend    = 0;  // Is it weekend? check now[6]. 0 =su 7=sa.

boolean SetDateNow = 0 ; //turn on to enable date setting, make sure you enter the proper values further down
//--------end initial variables here----------

char outBuf [25];                 // buffer for screen strings

//--------Lipo stuff if you use it-----------------
const byte AIOP3 = 16   ;    // pin to use: (org is a int!) see http://jeelabs.net/projects/hardware/wiki/JeeNode
int BattAdjust   = 6584 ; // fudge for accurate readings
int LipoV        = 3700 ; // default voltage. 
boolean Lipo     = 0 ;  //is a Lipo being used, not needed for 5 V supply. 
// end Lipo 

//--------Misc stuff---------
int IncL         = 0;  // counter for keeping track of Watt pulses. 1 pulse = 2Watt. L= low tarif, H is hogh tarif.
int IncH         = 0;
int Date4Store   = 0;  //place to store date in crushed format: y.mm.dd  2012=2, so value would be 21113  = (201)2 11(nov) 13(day). 21113 is an int word. 

byte node     = 0;  // node id trying to make is print
byte i           = 0;  //general counter.
byte Offset      = 0;  // offset in EEprom mem, so I can start at a different location easily.

//--------------------------

// definitions for electricity Jeenode
typedef struct { int power, pulse, Lipo, misc2; } PayloadTX;   // this is the electricity one
  PayloadTX emontx;

// definitions for Gas Jeenode
typedef struct { int gasPulse, gasLipo;  } PayloadGas;  // need to calculate usage locally
  PayloadGas emongas;   // each pulse is 0.001 m3. so a running total is ok, reset each day!

//-----------RTC--------------
byte now[7];  // get time RTC  
PortI2C myport (2 /*, PortI2C::KHZ400 */);  // here is my clock
DeviceI2C rtc (myport, 0x68);

//setting time on the chip: see RTC demo at Jeelab

static byte bin2bcd (byte val) {
    return val + 6 * (val / 10); }

static byte bcd2bin (byte val) {
    return val - 6 * (val >> 4); }

static void setDate (byte yy, byte mm, byte dd, byte d, byte h, byte m, byte s) {
    rtc.send();
    rtc.write(0);
    rtc.write(bin2bcd(s));
    rtc.write(bin2bcd(m));
    rtc.write(bin2bcd(h));
    rtc.write(bin2bcd(d));  // value depends on the day. Su=0 etc.
    rtc.write(bin2bcd(dd));
    rtc.write(bin2bcd(mm));
    rtc.write(bin2bcd(yy));
    rtc.write(0);
    rtc.stop();
}

static void getDate (byte* buf) {
	rtc.send();
	rtc.write(0);	
        rtc.stop();

    rtc.receive();
          buf[5] = bcd2bin(rtc.read(0));
          buf[4] = bcd2bin(rtc.read(0));
          buf[3] = bcd2bin(rtc.read(0));
          buf[6] = bcd2bin(rtc.read(0)); // this should return (DOW). Adjusted the [now] buffer size to 7
          buf[2] = bcd2bin(rtc.read(0));
          buf[1] = bcd2bin(rtc.read(0));
          buf[0] = bcd2bin(rtc.read(1));
    rtc.stop();
}
//-------end-RTC--------------
// end definitions.


//------------------------------------------------------------
// setup
//------------------------------------------------------------

void setup () {
 
      Serial.begin(9600);
     
      glcd.begin();
      glcd.backLight(200);  //with 100 I have 20 ma use, 17ma at 0
      GLCD_Init () ;
      
      SetADconv ();  //initialise the A/D conv.
     
      rf12_initialize(15,RF12_868MHZ,210); // NodeID, Frequency, Group
     
      //---------------
      // Set flag setDate to allow setting the date/time. Make sure you enter the right time there + 1 minute to allow for the compile/upload.
      // format:  yy mm dd dow hh mm ss  Note the addition of dow=day of the week. Su=0 Register 3, DS1307.
      if (SetDateNow) {
       setDate(12, 11, 18, 1, 10, 11, 0);  
      }
      //---------------
      
      //---------------
      // Only if you need to reset the eprom to a known blank state (first 6 bytes are 255)
      if (SetZero) { 
          ResetEEPROM ();
          } // end SetZero loop.
      //---------------
      
      //  Write data to eeprom, starting at byte 0. (forget about the 100k rewrite limit on writing cells. That's a long time!
      //  Format is 2 bytes for date/time, 2 bytes for T1, 2 bytes T2 tarif, closed by 0
      //  y.mm.dd  2012=2, so value would be 21113  = (201)2 11(nov) 13(day). 21113 is an int word.  
      //  hh.mm 2359 is max time, which is less then 65000, so this can also be written as a word.( 2 bytes)
      //  therefore we can stuff all data in 6 bytes.
          
      getDate(now);                                       //get time from RTC.
      if ((EEPROM.read(0)== 255 ) || (OverRide == 1))  {  //if 255 it's empty, never been written to, so write the date in it.
           WriteIfEmptyEEprom () ;                        // if OverRide, I need to catch up with real values.
          }
   
       //------------------
       // read the stored values and put them into the T1/T2 variables.
       ReadStoredT12 ();
       //--------------------
      
      // ---- nice one: found on Jeelabs----
       Serial.print("Free Ram: ");  // or put it somewhere in the loop to watch things.. I think I have 324 bytes left.
       Serial.println(freeRam());  
       
}  // end of setup

//------------------------------------------------------------
// loop
//------------------------------------------------------------

void loop () {    //main loop
       
       //---------------
       // clearing the whole screen, and write the header. actually it would be nicer to just overwrite the digits that change.
       WriteGLCDHeader ();
       //---------------
       
       getDate(now);  //what is the date
       
       //------Is it a weekend?------ Important because weekends are low tarif. 
       if ((now[6] == 0) || (now[6] ==7)) {
         Weekend = 1;
         glcd.drawString(120,0, "W");  //it's a weekend! Tell me so at top right screen!
         }
       //-------end weekend----------Always too soon!
 
       //------Waiting for a broadcast----------
       if (rf12_recvDone() && rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)  //listening for a broadcast
       {    // broadcast reception loop
       int node = (rf12_hdr & 0x1F);
       //--------somebody sent something-----Do all the processing in the loop, messages don't arrive that often.
     
       switch (node) {
       
         case 10:
       //------ Electricity node is set to 10
        // if (node == 10) {    //electricity processing loop
           //WriteNode10 ();     //actual use from EmonTX to the screen (calculates Power from time between pulses.)
           IncrementT12 ();    // increment counters etc.
        // }  // end electricity processing loop
        break;
     
       //-----Gas node is set to 9
        case 9:
        // if (node == 9){
          IncrementGas ();
          // see how mu lipo is doing:
          Serial.print("Gas-Lipo =");
          Serial.println(emongas.gasLipo);
         // }  // end of Gas calc.
          break;
        
        case 15: // nanode to ethernet..
        // nada for now.
          break;
       }
        
      //-----write lipo volts------------
      // running off the usb-wart redundant at the moment. 
         if (Lipo == 1) {
           WriteLipoV () ;
           }
      //---------------------
      
      // node 15 causes 5000 watt+ readouts to show and messes up the readings.
      // node 9 causes 0 readings..
      // not sure how to kill those. Need to accept and do nothing..
      // probably better to make a case = something.. structure.
      
      
     
       //--------Write everything to the screen-------
           WriteGLCDHeader (); // sometimes things get stuck, better to clear all..
           WriteNode10 (); // electricity
           WriteT1T2 ();
           WriteTotalGas ();  // node 9 
           sprintf(outBuf, "Node:%02d",node);  // can't this to work yet in a 'void'
           glcd.drawString(0,57, outBuf);
           WriteTime ();
           WriteGLCD () ;
          // Serial.println(emongas.gasLipo);
            
         //---------------
       
     }  // end broadcast reception loop
     
     //  Serial.print("FreeRam:");
     //  Serial.println(freeRam());
     
}      // end main loop


//--------- all the functions------------
// yes, I know I should put the most often used ones at the top here.

static void WriteGLCD () { 
             glcd.refresh(); // write everything to the screen.
             delay (500);
             }  
        
static void WhichNode () {
             sprintf(outBuf, "Node:%02d",node);
             glcd.drawString(0,57, outBuf);
             Serial.print("Node ");
             Serial.println(node);
              }

static void WriteT1T2 () {
             //Writes the T1 and T2 values on the LCD
             sprintf(outBuf, "ET1: %05u.%03d kWatt",T1,IncH);  // chagend d > u 'unsigned' int 
             glcd.drawString(1,28, outBuf);
             sprintf(outBuf, "ET2: %05u.%03d kWatt",T2,IncL);
             glcd.drawString(1,37, outBuf);
             }  
 
static void WriteTime () {
             sprintf(outBuf, "@%02d",now[3]);
             glcd.drawString(73,57, outBuf);
             sprintf(outBuf, ":%02d",now[4]);
             glcd.drawString(91,57, outBuf);
             sprintf(outBuf, ":%02d",now[5]);
             glcd.drawString(109,57, outBuf);
             }
        
static void WriteTotalGas () {
             sprintf(outBuf, "Gas: %5u.%003d M3",TotalGas,inc_gas_pulse);  //use 'u' for unsigned int. 
             glcd.drawString(1,46, outBuf); 
             } 
       

             
static void WriteLipoV () {
             LipoV= map (analogRead(16),0,1023,0,BattAdjust);            
             sprintf(outBuf, "Lipo:%d.%d V",LipoV/1000,LipoV%1000 );
             glcd.drawString(0,49, outBuf);
             }
             
static void WriteNode10 () {
             //actual usage as seen by emontx. just pasisng the message here.           
             emontx = *(PayloadTX*) rf12_data;
             sprintf(outBuf,"Power use:%04d Watt",emontx.power);  //actual usage as seen by emontx. just pasisng the message here.
             glcd.drawString(7,19,outBuf);
             }


             
static void IncT1 () {
              IncH = IncH + (2*emontx.pulse);  //increment H because my meter does 500 pulses/kW I need to double them.
                if (IncH >= 1000 ) {             //just in case we don;t end on a nice number 1000   
                   T1=T1 + 1;   //add to the kW total.
                     //IncH = IncH -1000;           //incase we have some Watts left due to more then a few pulses.
                     IncH=0;  // changed that back to 0, random lulse trains were messing this up. I might loose a few pulses, but ok..
                     WriteDate2Eeprom ();
                     WriteT1_2EEprom ();
                   }
              }
                
static void IncT2 () {
              IncL = IncL + (2*emontx.pulse);  // trying to find a way to print the fraction nicely
                 if (IncL >= 1000 ) {           // it should roll over
                   T2=T2 + 1;
                     IncL = IncL-1000; // in case we have more then one pulse increment. 
                      IncL = 0;
                      WriteDate2Eeprom ();
                      WriteT2_2EEprom () ;
                    }      // end roll over loop
               }

static void IncrementGas () {
            ++inc_gas_pulse ;
            if (inc_gas_pulse >= 1000)  {
             TotalGas = TotalGas + 1 ;
             inc_gas_pulse = 0;  //reset counter
            }
            //Write TotalGas to EEPROM stub
             
            }
                
static void WriteGLCDHeader () {
             glcd.clear();
             glcd.drawLine(0,16,127,16,WHITE);
             glcd.drawString(7,0, "Energy consumption");
             glcd.drawString(7,8, "monitor.  Rev 0.52");
             }
              
static void GLCD_Init () {
            glcd.clear();
            glcd.setFont(font_clR6x8);
            glcd.drawString(10,20, "Initializing..");
            glcd.drawString(10,30, "All code GPL");
            glcd.drawString(10,40, "kees@blokland.net");
            glcd.refresh(); //write it to the screen.
            delay (500);
            }
      

static void WriteDate2Eeprom () {
             Date4Store= ((now[0]-10) *10000)+ ( now[1] * 100) +( now[2]);  //should be 21113 on the 13th Nov 2012
             EEPROM.write(0, (highByte(Date4Store)));
             EEPROM.write(1, (lowByte(Date4Store))); 
             }
                
static void WriteT1_2EEprom () {
             EEPROM.write(2, (highByte(T1)));
             EEPROM.write(3, (lowByte(T1)));
             }  
              
static void WriteT2_2EEprom () {
             EEPROM.write(4, (highByte(T2)));
             EEPROM.write(5, (lowByte(T2)));
             }  
 
             
static void IncrementT12 () {
       //-----------time between 0700 and 2300 and not weekend---- High tarif
           if (((now[3] < 23) || (now[3] > 6)) && (Weekend == 0 )) {
               IncT1 ();
             }  //do this stuff during the day, when not a weekend.
       
      
       //---------------time between 2300 and 0700 or weekend?----Low tarif
           if ((now[3] > 22) || (now[3] < 7) || (Weekend == 1 )) {
               IncT2 ();       
             }   // do this stuff during the night and weekends.
         }
         
static void ResetEEPROM () {
             Serial.println ("255 > 6 locs");  
                  for ( i = Offset; i < (Offset +6) ; i++){
                     EEPROM.write(i,255); 
                  }     
              // Read them back to be sure..
                 Serial.println("Read ");
                     for ( i = Offset; i < Offset+ 6; i++){
                 Serial.println(EEPROM.read(i));
                  }  
             }
             
static void WriteIfEmptyEEprom () {
            WriteDate2Eeprom () ;  // easier on the eyes, put the date in it
            EEPROM.write(Offset+2, (highByte(T1)));  // write the present meter values as read from the meter.
            EEPROM.write(Offset+3, (lowByte (T1)));
            EEPROM.write(Offset+4, (highByte(T2)));
            EEPROM.write(Offset+5, (lowByte (T2)));
            EEPROM.write(Offset+6, (lowByte(0))); 
           }   
           
static void  ReadStoredT12 () {
            T1 = ((256 * (EEPROM.read(Offset+2))) + (EEPROM.read(Offset+3)));
            T2 = ((256 * (EEPROM.read(Offset+4))) + (EEPROM.read(Offset+5)));
            }
        
static void SetADconv () {              
                pinMode (AIOP3, INPUT);  // a/d converter
                digitalWrite (AIOP3, LOW);  //disable pull up
                }
         
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


// ---just oods and ends that take up space.


       /*    //running out of RAM, need to disable sections of debug code.
       //-------Debug--------
       if (Debug ){                            // debug, set Debug to 0 to disable.
         Serial.print("Date=");
         Serial.print((256 * (EEPROM.read(Offset+0))) + (EEPROM.read(Offset+1)));   
         Serial.print(" L=");
         Serial.print((256 * (EEPROM.read(Offset+2))) + (EEPROM.read(Offset+3)));
         Serial.print(" H=");
         Serial.println((256 * (EEPROM.read(Offset+4))) + (EEPROM.read(Offset+5)));
         }     //end debug
       //------End Debug--------
       */    
       
       /*
       //-----Debug----------
       if (Debug) {             // debug, set Debug to 0 to disable.
         for ( i = Offset; i < (Offset +6) ; i++){
         Serial.print("Loc "); 
         Serial.print(i);
         Serial.print("=");
         Serial.print(EEPROM.read(i)); 
         } // end if loop 
         } // end debug
       //-----EndDebug----------
       */     
       
              
     /*  // running out of mem need to disable some debug bits. 
       // Or do it in one go as below. 
       // sprintf(outBuf, "Prev T2:%05d kWatt",T1);
       glcd.drawString(0,22, "Stored values are: ");
       sprintf(outBuf, "Prev T1:%05d kWatt",T2); 
       glcd.drawString(0,33, outBuf);
       sprintf(outBuf, "Prev T2:%05d kWatt",T1);
       glcd.drawString(0,41, outBuf);
        
       glcd.refresh(); //write it to the screen.
       delay (500);
       */
     
       
