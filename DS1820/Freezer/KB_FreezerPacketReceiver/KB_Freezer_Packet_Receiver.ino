/*
Reading data, nothing fancy, just something that makes sure the Tx is running and sending it's stuff
You can run this code on a Jeelink, and wathc the data on a putty connection. Set putty to listen on Comx and baudrate accordingly.

*/

#include <JeeLib.h>             //https://github.com/jcw/jeelib

 
// Define the data structure of the packet to be received. Should be similar to the transmitting side!
 typedef struct { int power, pulse, Lipo, misc2; } PayloadTX;
// Create a variable to hold the received data of the defined structure .
   PayloadTX emontx;
 
typedef struct { int gasPulse, gasPulseTime,DreamOn ; } PayloadGas;  // need to calculate usage locally
   PayloadGas emongas; 
 
 
typedef struct {byte hour, min, sec, freeRam ;} PayloadEmonGLCD;  // trying to se ewhat the GLCD is doing.
   PayloadEmonGLCD emonglcd ;

typedef struct { float T1, T2, T3;  } PayloadTmp;     // can't be an int, since data is xx.x 
   PayloadTmp montmp;   


void setup ()
 {
   Serial.begin(9600);
   Serial.println("File: Packet_Receiver 28 Nov");
   rf12_initialize(15,RF12_868MHZ,210); // NodeID, Frequency, Group
  }
 
 
void loop ()
 {
   if (rf12_recvDone() && rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
   {
     int node_id = (rf12_hdr & 0x1F);
 
    // Emontx node id is set to 10
     
     switch (node_id){   // Power node id is set to 10
       
   /*
       case 9:
       emongas = *(PayloadGas*) rf12_data;
       Serial.print ("Gas: ");
       Serial.println(emongas.DreamOn);
       Serial.print ("TimeBetweenPulses: ");
       Serial.print(emongas.gasPulseTime/10);
       Serial.println(emongas.gasPulseTime%10);
       break;
       
       case 10: 
       emontx = *(PayloadTX*) rf12_data;
 
       Serial.print("Power:");
       Serial.print(emontx.power);
       Serial.print(" Pulses:");
       Serial.println(emontx.pulse);
       break ;
       
     
       case 13:
       emonglcd = *(PayloadEmonGLCD*) rf12_data;
       Serial.print("GLCD alive at ");
       Serial.print(emonglcd.hour);
       Serial.print(":");
       Serial.print(emonglcd.min);
       Serial.print(":");
       Serial.print(emonglcd.sec);
       Serial.print(". Mem ");
       Serial.println(emonglcd.freeRam);
       break;
       
       
       case 15:
       Serial.println("Nanode Transmission");
       break;
   */    
       case 16:
       montmp = *(PayloadTmp*) rf12_data;
      // Serial.println ("DS1820's calling");  //removed these statements to get a clean logfile. Use putty to log the data.
      // Serial.print(":");
       Serial.print(montmp.T1); 
       Serial.print(":");
       Serial.print(montmp.T2);
       Serial.print(":");
       Serial.println(montmp.T3);
       break;
       
    }
   }  
  } 
