/*
  NanodeRF_Power_RTCrelay_GLCDtemp
  
 28 oct 19:15  added the gas pulses. Need to add intelligence on the sending part. 
               Also have to make sure that the Tx's don't interfere, they both run at 10 sec interval.
 
 27 oct 1700: changed tx back to 10 sec intervals and 7200000000 to sort out 500 pulses prob. 
              Reason was I did not calculate power properly over 60 secs. number pulses was ok, but I only 
s              got 130 odd watts. Need to get a better grasp on the math.
 27 oct 1200: added a total counter, so I can compare values I read with real meter values
         counter resets at 20:00
 4 Nov @15:45: added GasLipo values. Recieving a gas pulse as it happens, Need to add a timestamp with that!
               The el-monitor comes around avery 10 secs, so I'm sending the gas data at that rate .
 6 Nov: I'm loosing characters at the end of the transmitted string: check the length (make buffer larger) or reduce string.



  ******************************************************* 
  ToDo add a proper timestamp on receipt of a packet. (for both gas and el.)
  
 
  ***************************************************        
  Relay's data recieved by emontx up to emoncms
  Relay's data recieved by emonglcd up to emoncms
  Decodes reply from server to set software real time clock
  Relay's time data to emonglcd - and any other listening nodes.
  Looks for 'ok' reply from request to verify data reached emoncms

  emonBase Documentation: http://openenergymonitor.org/emon/emonbase

  Authors: Trystan Lea and Glyn Hudson
  Part of the: openenergymonitor.org project
  Licenced under GNU GPL V3
  http://openenergymonitor.org/emon/license

  EtherCard Library by Jean-Claude Wippler and Andrew Lindsay
  JeeLib Library by Jean-Claude Wippler

  THIS SKETCH REQUIRES:
  
  Libraries in the standard arduino libraries folder:
	- JeeLib		https://github.com/jcw/jeelib
	- EtherCard		https://github.com/jcw/ethercard/

  Other files in project directory (should appear in the arduino tabs above)
	- decode_reply.ino
	- dhcp_dns.ino
*/

#define UNO       //anti crash wachdog reset only works with Uno (optiboot) bootloader, comment out the line if using delianuova

#include <JeeLib.h>	     //https://github.com/jcw/jeelib
#include <avr/wdt.h>

#define MYNODE 15            
#define freq RF12_868MHZ     // frequency
#define group 210            // network group 


//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------

//typedef struct { int power1, power2, power3, voltage; } PayloadTX;
typedef struct { int power, npulse, misc; unsigned long tpulses ; } PayloadTX;
PayloadTX emontx;

typedef struct { int temperature; } PayloadGLCD ;
PayloadGLCD emonglcd;

typedef struct {int gasPulse, gasPulseTime, Dreamon;  } PayloadGas;  //go figure consumption. 1 pulse per 0.001 m3.
PayloadGas emongas;

int gasflow =0;

//---------------------------------------------------

//---------------------------------------------------------------------
// The PacketBuffer class is used to generate the json string that is send via ethernet - JeeLabs
//---------------------------------------------------------------------
class PacketBuffer : public Print {
public:
    PacketBuffer () : fill (0) {}
    const char* buffer() { return buf; }
    byte length() { return fill; }
    void reset()
    { 
      memset(buf,NULL,sizeof(buf));
      fill = 0; 
    }
    virtual size_t write (uint8_t ch)
        { if (fill < sizeof buf) buf[fill++] = ch; }
    byte fill;
    char buf[150];
    private:
};
PacketBuffer str;

//--------------------------------------------------------------------------
// Ethernet
//--------------------------------------------------------------------------
#include <EtherCard.h>		//https://github.com/jcw/ethercard 


// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x42,0x31,0x42,0x21,0x30,0x31 };

// 1) Set this to the domain name of your hosted emoncms - leave blank if posting to IP address 
char website[] PROGMEM = "emon.blokland.net";

static byte hisip[] = { 192,168,1,10 };

// change to true if you would like the sketch to use hisip
boolean use_hisip = false;  

// 2) If your emoncms install is in a subdirectory add details here i.e "/emoncms3"
char basedir[] = "";

// 3) Set to your account write apikey 
char apikey[] = "12c5c621a6a8da9517393015a05f86bc";

//IP address of remote sever, only needed when posting to a server that has not got a dns domain name (staticIP e.g local server) 

byte Ethernet::buffer[700];
static uint32_t timer;         

const int redLED = 6;                     // NanodeRF RED indicator LED
const int greenLED = 5;                   // NanodeRF GREEN indicator LED

int ethernet_error = 0;                   // Etherent (controller/DHCP) error flag
int rf_error = 0;                         // RF error flag - high when no data received 
int ethernet_requests = 0;                // count ethernet requests without reply                 

int dhcp_status = 0;
int dns_status = 0;
int emonglcd_rx = 0;                      // Used to indicate that emonglcd data is available
int emongas_rx=0;                         // Used to indicate that gas data is available
int data_ready=0;                         // Used to signal that emontx data is ready to be sent
unsigned long last_rf;                    // Used to check for regular emontx data - otherwise error
unsigned long last_gas_rf;                // not used yet
char line_buf[50];                        // Used to store line of http reply header

unsigned long time60s = -60000;

int totalToday =0;
int hnow = 0;
int mnow = 0;
//**********************************************************************************************************************
// SETUP
//**********************************************************************************************************************
void setup () {
  
  //Nanode RF LED indictor  setup - green flashing means good - red on for a long time means bad! 
  //High means off since NanodeRF tri-state buffer inverts signal 
  pinMode(redLED, OUTPUT); digitalWrite(redLED,LOW);            
  pinMode(greenLED, OUTPUT); digitalWrite(greenLED,LOW);       
  delay(100); digitalWrite(redLED,HIGH);                          // turn off redLED
  
  Serial.begin(9600);
  Serial.println("\n[webClient]");

  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {	//for use with NanodeRF
    Serial.println( "Failed to access Ethernet controller");
    ethernet_error = 1;  
  }

  dhcp_status = 0;
  dns_status = 0;
  ethernet_requests = 0;
  ethernet_error=0;
  rf_error=0;

  //For use with the modified JeeLib library to enable setting RFM12B SPI CS pin in the sketch. Download from: https://github.com/openenergymonitor/jeelib 
  rf12_set_cs(10); //emonTx, emonGLCD, NanodeRF, JeeNode
 
  rf12_initialize(MYNODE, freq,group);
  last_rf = millis()-40000;                                       // setting lastRF back 40s is useful as it forces the ethernet code to run straight away
   
  digitalWrite(greenLED,HIGH);                                    // Green LED off - indicate that setup has finished 
 
  #ifdef UNO
  wdt_enable(WDTO_8S);
  #endif;
}

//**********************************************************************************************************************
// LOOP
//**********************************************************************************************************************
void loop () {
  
  #ifdef UNO
  wdt_reset();
  #endif

  dhcp_dns();   // handle dhcp and dns setup - see dhcp_dns tab
  
  // Display error states on status LED
  if  (ethernet_error==1 || rf_error==1 || ethernet_requests > 1) digitalWrite(redLED,LOW);  
    
     else delay(200); digitalWrite(redLED,HIGH);      // turn off redLED
    
       
  //-----------------------------------------------------------------------------------------------------------------
  // 1) On RF recieve
  //-----------------------------------------------------------------------------------------------------------------
  if (rf12_recvDone()){      
      if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
      {
        int node_id = (rf12_hdr & 0x1F);
        
         if (node_id == 10)    
          {                                           // EMONTX
        
          emontx = *(PayloadTX*) rf12_data;                              // get emontx data
          Serial.println();                                              // print emontx data to serial
          Serial.print("El rcvd ");
          last_rf = millis();                                            // reset lastRF timer
          Serial.print (last_rf/1000); Serial.println(" sec ago.");
          delay(50);                                                     // make sure serial printing finished
          
          digitalWrite(greenLED,LOW); delay(50); digitalWrite(greenLED,HIGH);                     
          
        // JSON creation: JSON sent are of the format: {key1:value1,key2:value2} and so on
          
          if (emontx.power < 0) (emontx.power = 0);    // make sure we don't get too much rubbish.
          if (emontx.power > 7000) (emontx.power = 0);
          if (emontx.npulse < 0) (emontx.npulse =0);
          if (emontx.npulse > 50) (emontx.npulse = 0);
          
          // try to count pulses for 24 hrs and compare to meter reading.
          //Serial.print("H:"); Serial.print(hnow); Serial.print(" M:");Serial.println(mnow);
          if (hnow ==00 && mnow==00) {
                  totalToday=0;    //reset the counter at a known time. Best at 20:00 so I can compare real life values.
                //  Serial.print("Reset counter:");Serial.print (" Today ="); Serial.println(totalToday);
          } 
           totalToday =  totalToday + emontx.npulse ; //add pulses to total.
         //  emontx.totalPulses= totalToday;       
          
          
          
          str.reset();          // Reset json string      
          str.print(basedir); str.print("/api/post.json?");
          str.print("apikey="); str.print(apikey);
          str.print("&json={rf_fail:0");                                 // RF recieved so no failure
          str.print(",P:");        str.print(emontx.power);          // Add power reading, 
          str.print(",np:");        str.print(emontx.npulse);          // Add power reading
          str.print(",T2D:");     str.print(totalToday);     // Add total since 00:00 reading
    
          data_ready = 1;                                                // data is ready
          rf_error = 0;
          }

         if (node_id == 13)    { 
          emonglcd = *(PayloadGLCD*) rf12_data;                          // get emonglcd data
          Serial.print("emonGLCD temp recv: ");                        // print output
          Serial.println(emonglcd.temperature);  
          emonglcd_rx = 0;        //always nought to disable.
          }
         

        if (node_id == 9)    { 
          emongas = *(PayloadGas*) rf12_data;
          last_gas_rf = millis() ;
          Serial.println();                                              // print emontx data to serial
          gasflow = 36000/emongas.gasPulseTime ;  
          Serial.print ("GasFlow:");
          Serial.println (gasflow);
          digitalWrite(redLED,LOW); delay(50); digitalWrite(redLED,HIGH); 
          emongas_rx = 1; 
          }          
        } 
      }
    

  //-----------------------------------------------------------------------------------------------------------------
  // 2) If no data is recieved from rf12 module the server is updated every 30s with RFfail = 1 indicator for debugging
  //-----------------------------------------------------------------------------------------------------------------
  if ((millis()-last_rf)>120000)
  {
    last_rf = millis();                                                 // reset lastRF timer
    str.reset();                                                        // reset json string
    str.print(basedir); str.print("/api/post.json?");
    str.print("apikey="); str.print(apikey);
    str.print("&json={rf_fail:1");                                      // No RF received in 30 seconds so send failure 
    data_ready = 1;                                                     // Ok, data is ready
    rf_error=1;
  }


  //-----------------------------------------------------------------------------------------------------------------
  // 3) Send data via ethernet
  //-----------------------------------------------------------------------------------------------------------------
  ether.packetLoop(ether.packetReceive());
  
  if (data_ready) {
    
    // include temperature data from emonglcd if it has been recieved
    if (emonglcd_rx) {
      str.print(",t:");  
      str.print(emonglcd.temperature/100.0);
      emonglcd_rx = 0;
    }
    
     if (emongas_rx) {
      str.print(",gf:");  
      str.print(gasflow);
     // str.print(",gasTime:");
     // str.print(last_gas_rf);
     // str.print(",gasLipo:");
     // str.print(emongas.gasLipo);
      emongas_rx = 0;
    }
    
    str.print("}\0");  //  End of json string
    
    Serial.print("Data sent: "); Serial.print(str.buf); // print to serial json string
    Serial.println("Done, waiting for ack frm web");
    ethernet_requests ++;
    ether.browseUrl(PSTR(""),str.buf, website, my_callback);
   
    data_ready =0;
  }
 
 

 
  if (ethernet_requests > 10) delay(10000); // Reset the nanode if more than 10 request attempts have been tried without a reply

  if ((millis()-time60s)>80000)
  {
    time60s = millis();                                                 // reset lastRF timer
    str.reset();
    str.print(basedir); str.print("/time/local.json?"); str.print("apikey="); str.print(apikey);
    Serial.print("Time request sent: ");
    ether.browseUrl(PSTR(""),str.buf, website, my_callback);
    
  }
  
} // end rf12 recDone loop.
//**********************************************************************************************************************


//-----------------------------------------------------------------------------------
// Ethernet callback
// recieve reply and decode
//-----------------------------------------------------------------------------------
static void my_callback (byte status, word off, word len) {
  int lsize =   get_reply_data(off);
  
  if (strcmp(line_buf,"ok")==0)
  {
   
    Serial.println(". Ack received."); ethernet_requests = 0; ethernet_error = 0;
  }
  else if(line_buf[0]=='t')       // is it time?
  {
    Serial.print("Time is ");
    Serial.println(line_buf);
    
    char tmp[] = {line_buf[1],line_buf[2],0};
    byte hour = atoi(tmp);
    hnow=hour;  //fudge to get this value outside this call, by using a globally defined value 
   
    tmp[0] = line_buf[4]; tmp[1] = line_buf[5];
    byte minute = atoi(tmp);
    mnow=minute;  //fudge to get this value outside this call, by using a globally defined value 
   
    tmp[0] = line_buf[7]; tmp[1] = line_buf[8];
    byte second = atoi(tmp);
    if (hour>0 || minute>0 || second>0) 
    {  
      char data[] = {'t',hour,minute,second};
      int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
      rf12_sendStart(0, data, sizeof data);
      rf12_sendWait(0);
    }
  }
  else Serial.println(line_buf);    //if all else fails, lets see why.
}

