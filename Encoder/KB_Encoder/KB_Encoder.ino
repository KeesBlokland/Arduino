/*
Bits for Hotwire controller
 
 */

#include <LiquidCrystal.h>
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(2,3,4,5,6,7);

// Rotary setup, connected to, A0/1  chip pin 23/24

#define ENC_A 23
#define ENC_B 24
#define ENC_PORT PINC


//Blink plug pins to Dig 8/9 Chip pin 14/15
#define AIO 9
#define DIO 8

//pwm stuff
const int HotWire = 10; // Analog output pin that the LED is attached to
int HotWireOut = 0;

static word ledState;
static void setLED (byte pin, byte on) {
  if (on) {
    digitalWrite(pin, 0); 
    pinMode(pin, OUTPUT); 
  }  
  else {
    pinMode(pin, INPUT); 
    digitalWrite(pin, 1); 
  }
}

static byte getButton (byte pin) {
  setLED(pin, 0);
  byte pushed = digitalRead(pin) == 0 ;
  setLED(pin, bitRead(ledState, pin));
  return pushed;
}

static void redLED (byte on) {
  setLED(DIO, on); 
  bitWrite(ledState, DIO, on); 
}
static void greenLED (byte on) {
  setLED(AIO, on); 
  bitWrite(ledState, AIO, on); 
}
static byte upperButton () { 
  return getButton(DIO); 
}
static byte lowerButton () { 
  return getButton(AIO); 
}



void setup() {

  Serial.begin(9600);
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("HotWire Driver  "); 
  lcd.setCursor(0,1);
  lcd.print("Rev 06          ");
  delay (1500);

  lcd.setCursor(0,0);
  lcd.print("EncoderTest 0610");  
  lcd.setCursor(0,1); 
  lcd.print("Counter:   ");

  // encoder

  pinMode(ENC_A, INPUT);
  digitalWrite(ENC_A, HIGH);
  pinMode(ENC_B, INPUT);
  digitalWrite(ENC_B, HIGH);



  pinMode (DIO, OUTPUT); 
  redLED(0);
  pinMode (AIO, OUTPUT); 
  greenLED(0);
}


void loop() {

  static uint8_t counter = 127;  //this variable will be changed by encoder input. counter value is startign number
  int8_t tmpdata;
  /**/
  tmpdata = read_encoder();
  if( tmpdata ) 
  {
    lcd.setCursor(8,1); 
    lcd.print("   ");
    lcd.setCursor(8,1); 
    lcd.print(counter, DEC);
    
    if (counter > 250) {
    tmpdata =0;
    counter = 249;
    }
    if  (counter < 10) {
    tmpdata =0;
    counter=11;
    }  
    
    counter += tmpdata;
  }


  // Buttons
  if (upperButton()) { 
    lcd.setCursor(13,1); 
    lcd.print("Up "); 
  }
  if (lowerButton()) { 
    lcd.setCursor(13,1); 
    lcd.print("Rst"); 
    counter=127; 
    lcd.setCursor(0,1); 
    lcd.print("Counter:   "); 
    lcd.setCursor(8,1); 
    lcd.print(counter, DEC);
  }


  // To the wire

  HotWireOut = counter;
  //set some limits  

  if (counter > 10  || counter < 250) {
    lcd.setCursor(13,1); 
    lcd.print("   ");
    redLED(0);
    greenLED(0);
  }


  if (counter > 250) {
    HotWireOut = 250 ; 
    lcd.setCursor(13,1); 
    lcd.print("Max");
    redLED(1);
    delay (30);
  }

  if  (counter < 10) {
    HotWireOut = 10 ;
    lcd.setCursor(13,1); 
    lcd.print("Min");
    greenLED(1);
    delay (30);
  } 


  analogWrite(HotWire, HotWireOut);    



  // End of loop.
}




/* returns change in encoder state (-1,0,1) */
int8_t read_encoder()
{
  static int8_t enc_states[] = {
    0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0    };
  static uint8_t old_AB = 0;
  /**/
  old_AB <<= 2;                   //remember previous state
  old_AB |= ( ENC_PORT & 0x03 );  //add current state
  return ( enc_states[( old_AB & 0x0f )]);
}


