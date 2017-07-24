#include <Wire.h>
#include "Adafruit_MCP23017.h"

Adafruit_MCP23017 mcp;

enum Pins {
  SN4 = 0,
  DR4,
  SN3,
  DR3,
  SN7,
  DR7,
  SN6,
  DR6,
  DR8,
  SN8,
  DR5,
  SN5,
  DR1,
  SN1,
  DR2,
  SN2
 };

int output[] = {DR1, DR2, DR3, DR4, DR5, DR6, DR7, DR8};
int  input[] = {SN1, SN2, SN3, SN4, SN5, SN6, SN7, SN8};
int  inVal[] = {0,   0,   0,   0,   0,   0,   0,   0};
//              0 Door Closed
//              1 Door Open or not connected

    
// ... and this interrupt vector
byte intPin;
volatile boolean awakenByInterrupt = false;

void setup() {  
  mcp.begin();

  for(int i; i<sizeof(output); i++)
  {
    mcp.pinMode(output[i], OUTPUT);
  }

  // mirror INTA and INTB
  // The INTA/B will not be Floating 
  // INTs will be signaled with a LOW
  mcp.setupInterrupts(true,false,LOW);
  
  for(int i; i<sizeof(input); i++)
  {
    mcp.pinMode(input[i], INPUT);
    mcp.pullUp(input[i], HIGH);  // turn on a 100K pullup internall
    mcp.setupInterruptPin(input[i],CHANGE); 
    // Load array with deafult values
    inVal[i] = mcp.digitalRead(input[i]);
  }

  intPin = digitalPinToInterrupt(2);
}

// The int handler will just signal that the int has happen
// we will do the work from the main loop.
void intCallBack(){
  awakenByInterrupt=true;
}

void loop() {
  attachInterrupt(intPin,intCallBack,FALLING);

  // Simulate a deep sleep
  while(!awakenByInterrupt);
  // Or sleep the arduino, this lib is great, if you have it.
  //LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  
  detachInterrupt(intPin);

  if(awakenByInterrupt) handleInterrupt();
//  for(int i; i<sizeof(output); i++)
//  {
//    mcp.digitalWrite(output[i], HIGH);
//    delay(500);
//    mcp.digitalWrite(output[i], LOW);
//  }
}

void handleInterrupt(){
  uint8_t pin;
  uint8_t val;

  // Read each interupt pin
  while(true)
  {
    // Get more information from the MCP from the INT
    pin=mcp.getLastInterruptPin();
  
    if(MCP23017_INT_ERR != pin)
    {
      val=mcp.getLastInterruptPinValue();
      for(int i; i<sizeof(input); i++)
      {
        if (input[i] == pin)
        {
          inVal[i] = val;
          break;
        }
      }
      
      if(val == 0)
      {
        // Lets have some fun and find another closed door to trigger
        // this code biases the fist one but hey, what ever..
        for(int i; i<sizeof(input); i++)
        {
          if (0 == inVal[i] && pin != input[i])
          {
            mcp.digitalWrite(output[i], HIGH);
            delay(100);
            mcp.digitalWrite(output[i], LOW);
          }
        }
      }
    }
    else
    {
      break;
    }
  }

  EIFR=0x01;
  awakenByInterrupt=false;
}
