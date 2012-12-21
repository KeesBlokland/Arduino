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
Counts are going to the wrong counter: T1_kW= 6255 actual, reads 6258. T2_kW actual 10349, reads 10346.
It's a weekend, so this does not work! Needs to figure out the day and count accordingly.
Sunday 0830: T2 (62xx) is increasing. Replaced all strings with T1_kW/T2_kW. Maybe it was just the final print to the GLCD.. Watch it.
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
-----23 nov: On power up T1_kW=6271, T2=6273.. looks like T2 gets stored in wrong location! /readback-loop swapped? 
Do check the T1_kW/T2>> locations in EEProm. It's Not Right Yet.
----24 nov:  No Weekend?? there's only 7 days: 0-6! so waiting for day 7 won't work.
node 9 still cause problems, on reception Power Use var is made 0.. 
First reduce emontx & gas data, remove redundant stuff. (lipo, misc)
--26 nov: dream time on the gas node is always 5 sec. Needs fixing.
--28 nov: it's falling asleep at night? why? Tarifs is wrong way: 1=10K 2=6k
Store every time we hear a new value, and add watchdog.
Listen to internet time node if needed, adjust!
---1Dec: reduced tx to 1/min to see if I can better correspondence between real and measured here.



ToDo
Make the m3 with a small '3'



*/
// This is node 13

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
int SetZero =0 ;  // when 1, write 255's to all 14 locations. (is 'zero') 
//---------End Debugging ---------

//--------Put initial meter readings here----------
// Nuon cheap tarif is 2300-0700 and official holidays. 1 januari, tweede paasdag, Hemelvaartsdag, tweede pinksterdag, eerste en tweede kerstdag.
// probably better to make a function to decide if it is weekend or a special day.

unsigned int T1_kW = 10417;  // T1_kW  inititally put the present meter reading here. Low tarif?
unsigned int T1_watt = 100 ;   // the x,000 bit that you can't see, but want to save anyway between restarts.
unsigned int T2_kW = 6329;   // T2_kW inititally put the present meter reading here.
unsigned int T2_watt = 641 ;
unsigned int TotalGas = 53423 ;   // 
int inc_gas_pulse      = 770;  // place for emongas.TotalGasPulse read at least 2  digits and enter xx0 here. 
boolean OverRide   =1;  // override EEprom values, force writing these.
boolean Weekend    = 0;  // Is it weekend? check now[3]. 0 =su 6=sa.
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
//int IncL         = 0;  // counter for keeping track of Watt pulses. 1 pulse = 2Watt. L= low tarif, H is hogh tarif.
//int IncH         = 0;   // See T1_watt

int Date4Store   = 0;  //place to store date in crushed format: y.mm.dd  2012=2, so value would be 21113  = (201)2 11(nov) 13(day). 21113 is an int word. 

byte node        = 0;  // node id trying to make is print
byte i           = 0;  //general counter.
byte Offset      = 0;  // offset in EEprom mem, so I can start at a different location easily.
byte z           = 0;
//--------------------------

// definitions for electricity Jeenode
typedef struct { int power, pulse ; } PayloadTX;   // this is the electricity one
  PayloadTX emontx;

// definitions for Gas Jeenode
typedef struct { int gasPulse,gasPulseTime, DreamOn ;  } PayloadGas;  // need to calculate usage locally
  PayloadGas emongas;   // each pulse is 0.001 m3. so a running total is ok, reset each day!

typedef struct { byte zero , hour, min, sec ; } IntnetTime;  // listen to the nanode for accurate time.
  IntnetTime nanodetime ;

void WriteGLCDHeader () {
             glcd.clear();
             glcd.drawLine(0,16,127,16,WHITE);
             glcd.drawString(0,0, "Energy consumption");
             glcd.drawString(0,8, "monitor.  Rev 0.56A");
             }
             
             
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
     
      rf12_initialize(13,RF12_868MHZ,210); // NodeID, Frequency, Group
     
      //---------------
      // Set flag 'setDate' to allow setting the date/time. Make sure you enter the right time there + 1 minute to allow for the compile/upload.
      // format:  yy mm dd dow hh mm ss  Note the addition of dow=day of the week. Sunday=0 Register 3, DS1307.
      if (SetDateNow) {
         setDate(12, 11, 25, 0, 16, 18, 0);  
      }
      //---------------
      
      //---------------
      // Only if you need to reset the eprom to a known blank state (first 6 bytes are 255)
      if (SetZero) { 
          ResetEEPROM ();
          } // end SetZero loop.
      //---------------
      
      //  Write data to eeprom, starting at byte 0. (forget about the 100k rewrite limit on writing cells. That's a long time!
      //  Format is 2 bytes for date/time, 2 bytes for T1_kW, 2 bytes T2_kW tarif, closed by 0
      //  y.mm.dd  2012=2, so value would be 21113  = (201)2 11(nov) 13(day). 21113 is an int word.  
      //  hh.mm 2359 is max time, which is less then 65000, so this can also be written as a word.( 2 bytes)
      //  therefore we can stuff all data in 6 bytes.
          
      getDate(now);                                       //get time from RTC.
      if ((EEPROM.read(0)== 255 ) || (OverRide == 1))  {  //if 255 it's empty, never been written to, so write the date in it.
           WriteIfEmptyEEprom () ;                        // if OverRide, I need to catch up with real values.
          }
   
       //------------------
       // read the stored values and put them into the T1_kW/T2_kW variables.
       ReadStoredData ();
       //--------------------
    
       //---------------
       #ifdef UNO  //  stop it falling asleep at unknown times?
          wdt_enable(WDTO_8S);
       #endif;
       //---------------
       
}  // end of setup


//------------------------------------------------------------
// loop
//------------------------------------------------------------

void loop () {    //main loop
       
           //---------------
           #ifdef UNO    //keep things alive.
              wdt_reset();
           #endif
           //---------------
           
       //------Waiting for a broadcast----------
       if (rf12_recvDone() && rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)  //listening for a broadcast
       {    // broadcast reception loop
       int node = (rf12_hdr & 0x1F);
       //--------somebody sent something-----Do all the processing in the loop, messages don't arrive that often.
       
       
       getDate(now);  //what is the date
           
       //do something depending on who sent the message.
       
       switch (node) {
       
        case 10:        //------ Electricity node is set to 10
        emontx = *(PayloadTX*) rf12_data;
             IncrementT12 ();    // increment counters etc.
        break;
     
       
        case 9:         //-----Gas node is set to 9
        emongas = *(PayloadGas*) rf12_data;
           IncrementGas ();
        break;
        
        case 15:        //--- nanode, sends data to internet, and transmits proper (server) time.
        nanodetime = *(IntnetTime*) rf12_data;
            boolean timeok = 1;
            // just checking hours mins for now, the rest I don't care for at the moment. 
            if  ( now[3] !=  nanodetime.hour) ; {
               timeok = 0;   // time is not right
            now[3] = nanodetime.hour ;  // fix it.
             }
            if  (now[4] != nanodetime.min) ; {
               timeok = 0;
            now[4] = nanodetime.min ;
             }
         // Feed this back to the clock, I'm assuming that year/month/day have not changed. This is just to take care of small drift.
         // why all this? because I can and it is fun.
         if (timeok = 0)  ; { 
           setDate (now[0], now[1], now[2], now[6], now[3], now[4], now[5]); 
         }
        break;
        
        
       }
   
       //--------Write everything to the screen-------
          
           WriteGLCDHeader ();   // sometimes things get stuck, better to clear all..
           IsItWeekend () ;      // hooray
           WriteElT12 ();        // electricity current rate on screen 
           WriteT1T2 ();         // update totals on screen
           WriteTotalGas ();      // node 9 
             sprintf(outBuf, "Node:%02d",node);  // can't get this to work yet in a 'void' putting it in a void makes it local!
             glcd.drawString(0,57, outBuf);
           WriteTime ();          // just so I know who last sent a message
           
           WriteGLCD () ;         // do it
           WriteIfEmptyEEprom () ; // having problems that things freeze up, try to see when that happens. Just call this every loop to save latest values.
    
          
          //send out some sign of life. This only works if the USB stays alive, which it does not!
          //next step is to log it in mem I guess.
         
          ++z ; 
        //  Serial.println (z);
          if (z > 10 ){
            byte data[] = { now[3],now[4],now[5], freeRam() } ;
              int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
              rf12_sendStart(0, data, sizeof data);
              rf12_sendWait(0);
              z = 0;
           
          // end of life..
          }  
            
         //---------------
       
     }  // end broadcast reception loop
  
}      // end main loop


//--------- all the functions------------
// yes, I know I should put the most often used ones at the top here.

void WriteGLCD () { 
             glcd.refresh(); // write everything to the screen.
             delay (500);
             }  
        
void WhichNode () {
             sprintf(outBuf, "Node:%02d", node);
             glcd.drawString(0,57, outBuf);
             //not working yet, need to learn more!
             }

void WriteT1T2 () {
             //Writes the T1_kW and T2_kW values on the LCD
             sprintf(outBuf, "ET1: %05u.%03d kWatt",T1_kW,T1_watt);  // changed d > u 'unsigned' int 
             glcd.drawString(1,28, outBuf);
             sprintf(outBuf, "ET2: %05u.%03d kWatt",T2_kW,T2_watt);
             glcd.drawString(1,37, outBuf);
             }  
 
void WriteTime () {
             sprintf(outBuf, "@%02d",now[3]);
             glcd.drawString(73,57, outBuf);
             sprintf(outBuf, ":%02d",now[4]);
             glcd.drawString(91,57, outBuf);
             sprintf(outBuf, ":%02d",now[5]);
             glcd.drawString(109,57, outBuf);
             }
        
void WriteTotalGas () {
             sprintf(outBuf, "Gas: %5u.%003d M3",TotalGas,inc_gas_pulse);  //use 'u' for unsigned int. 
             glcd.drawString(1,46, outBuf); 
             } 

void IncrementGas () {
            inc_gas_pulse = inc_gas_pulse + 1 ;
            if (inc_gas_pulse >= 1000)  {
               TotalGas = TotalGas + 1 ;
               inc_gas_pulse = 0;  //reset counter
            }
             WriteGas2EEprom ();
            }
            
int GasRate (int grate){  //not used yet, but this gives you the rate in l/hr. 1 m3=1000 l etc.. 
                 grate = (1/emongas.gasPulseTime )*3600 ;
              
            }
              
    
static void IsItWeekend () {
             
             Serial.print ("DOW "); Serial.print (now[6]);
             if ((now[6] == 6) || (now[6] == 7)) {     //7 days in a week: 1-7! now[6]=buf[6]= register 3!
             Weekend = 1;                          
             glcd.drawString(122,0, "W");  //Tell me so at top right screen!
                    }
                }
            //------Is it a weekend?------ Important because weekends are low tarif. You can add other days here ..

             
void WriteLipoV () {
             LipoV= map (analogRead(16),0,1023,0,BattAdjust);            
             sprintf(outBuf, "Lipo:%d.%d V",LipoV/1000,LipoV%1000 );
             glcd.drawString(0,49, outBuf);
             }
             
void WriteElT12 () {
             //actual usage as seen by emontx. just passing the message here.           
             sprintf(outBuf,"Power use:%04d Watt",emontx.power); 
             glcd.drawString(0,19,outBuf);
             }


             
void IncT1 () {
              T1_watt = T1_watt + (emontx.pulse);     //increment H because my meter does 500 pulses/kW I need to double them.
                if (T1_watt >= 1000 ) {              //just in case we don't end on a nice number 1000   
                   T1_kW=T1_kW + 1;               //add to the kW total.
                     T1_watt = T1_watt -1000;           //in case we have some Watts left due.
                     WriteT1_2EEprom ();
                   }
              }
                
void IncT2 () {
              T2_watt = T2_watt + (emontx.pulse);     // trying to find a way to print the fraction nicely
                 if (T2_watt >= 1000 ) {             // it should roll over
                   T2_kW=T2_kW + 1;
                     T2_watt = T2_watt-1000;            // in case we have more then one pulse increment. 
                     WriteT2_2EEprom () ;
                    }                             // end roll over loop
               }


 /*               
void WriteGLCDHeader () {
             glcd.clear();
             glcd.drawLine(0,16,127,16,WHITE);
             glcd.drawString(0,0, "Energy consumption");
             glcd.drawString(0,8, "monitor.  Rev 0.55D");
             }
 */
 
void GLCD_Init () {
            glcd.clear();
            glcd.setFont(font_clR6x8);
            glcd.drawString(10,20, "Initializing..");
            glcd.drawString(10,30, "All code GPL");
            glcd.drawString(10,40, "kees@blokland.net");
            glcd.refresh(); //write it to the screen.
            delay (500);
            }
      

void WriteDate2Eeprom () {
             Date4Store= ((now[0]-10) *10000)+ ( now[1] * 100) +( now[2]);  //should be 21113 on the 13th Nov 2012
             EEPROM.write(Offset+0, (highByte(Date4Store)));
             EEPROM.write(Offset+1, (lowByte(Date4Store))); 
             }
                
void WriteT1_2EEprom () {
             //WriteDate2Eeprom ();           
             EEPROM.write(Offset+2, (highByte(T1_kW)));
             EEPROM.write(Offset+3, (lowByte(T1_kW)));
             EEPROM.write(Offset+4, (highByte(T1_watt)));
             EEPROM.write(Offset+5, ( lowByte(T1_watt)));
             }  
              
void WriteT2_2EEprom () {
             //WriteDate2Eeprom ();  
             EEPROM.write(Offset+6, (highByte(T2_kW)));
             EEPROM.write(Offset+7, (lowByte (T2_kW)));
             EEPROM.write(Offset+8, (highByte(T2_watt)));
             EEPROM.write(Offset+9, ( lowByte(T2_watt)));
             }  

void WriteGas2EEprom () {
             //WriteDate2Eeprom ();             
             EEPROM.write(Offset+10, (highByte(TotalGas)));
             EEPROM.write(Offset+11, (lowByte(TotalGas)));
             EEPROM.write(Offset+12, (highByte(inc_gas_pulse)));
             EEPROM.write(Offset+13, (lowByte(inc_gas_pulse)));
              }
             
void IncrementT12 () {
       //-----------time between 0700 and 2300 and not weekend---- High tarif
           if (((now[3] < 23) || (now[3] > 6)) && (Weekend == 0 )) {
               IncT1 ();
             }  //do this stuff during the day, when not a weekend.
       
      
       //---------------time between 2300 and 0700 or weekend?----Low tarif
           if ((now[3] > 22) || (now[3] < 7) || (Weekend == 1 )) {
               IncT2 ();       
             }   // do this stuff during the night and weekends.
         }
         
void ResetEEPROM () {
             Serial.println ("255 > 14 locs");  
                  for ( i = Offset; i < (Offset +14) ; i++){
                     EEPROM.write(i,255); 
                  }     
              // Read them back to be sure..
                 Serial.println("Read ");
                     for ( i = Offset; i < Offset+ 14; i++){
                 Serial.println(EEPROM.read(i));
                  }  
             }
             
void WriteIfEmptyEEprom () {                      
            WriteDate2Eeprom () ;            // easier on the eyes, put the date in it
            WriteT1_2EEprom ();
            WriteT2_2EEprom ();
            WriteGas2EEprom ();
            EEPROM.write(Offset+14, (lowByte(0)));
            //Serial.println("EEprom updated");
           }   
            
           
void  ReadStoredData () {
            T1_kW = ((256 * (EEPROM.read(Offset+2))) + (EEPROM.read(Offset+3)));
            T1_watt = ((256 * (EEPROM.read(Offset+4))) + (EEPROM.read(Offset+5)));
            T2_kW = ((256 * (EEPROM.read(Offset+6))) + (EEPROM.read(Offset+7)));
            T2_watt = ((256 * (EEPROM.read(Offset+8))) + (EEPROM.read(Offset+9)));
            TotalGas =  ((256 * (EEPROM.read(Offset+10))) + (EEPROM.read(Offset+11))); 
            inc_gas_pulse = ((256 * (EEPROM.read(Offset+12))) + (EEPROM.read(Offset+13))); 
            //Serial.println("Reloaded Mem");
          }
        
        
void SetADconv () {              
                pinMode (AIOP3, INPUT);              // a/d converter
                digitalWrite (AIOP3, LOW);           //disable pull up
                }
         
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


