#define PI  3.1415

//Set up the Arduino Pin locations of the AD5330 Control Pins
#define  CS  18
#define  WR  17
#define  LDAC  16
#define  CLR  15
#define  PD  10
#define  BUF  9
#define GAIN  14

#define FREQ  3831	//FREQ defines the delay to create frequency of the tone played. The frequency of 'middle C' is ~261Hz.
					// 1/261 equals a period of .003831 seconds, or 3831 micro-seconds
#define VOL 0xFF	//VOL defines the voltage to be sent to the AD5330. 0xFF is the maximum voltage.

#include "WProgram.h"
void setup();
void loop();
double deg2rad(int degree);
unsigned int note=0;	//Use if the 'generate sinewave' portion of the code is enabled.

void setup()
{
  //All of the digital pins on the Arduino will be outputs to the AD5330
  for(int pin=0; pin<19; pin++)
  {
    pinMode(pin, OUTPUT);
  } 
  
  digitalWrite(PD, HIGH);  //Enable the AD5330
  digitalWrite(GAIN, LOW);  //Set Gain to 1
  digitalWrite(BUF, HIGH);  //Don't buffer the input
  digitalWrite(CS, HIGH);  //Set the CS high by default
  digitalWrite(WR, HIGH);  //Set the WR pin high by default
  digitalWrite(CLR, HIGH);  //Make sure Clear pin is disabled
  digitalWrite(LDAC, LOW);
  
  //Clock in Gain and Buffer Values
  digitalWrite(CS, LOW);
  delayMicroseconds(10);
  digitalWrite(WR, LOW);
  delayMicroseconds(10);
  
  digitalWrite(CS, LOW);
  delayMicroseconds(10);
  digitalWrite(WR, LOW);
  delayMicroseconds(10); 
  
}

void loop()
{
  //Generate a Square Wave. The tone is set by the value of FREQ. The amplitude of the wave is set by VOL.
  digitalWrite(CS, LOW);		//Ready the AD5330 for input

  digitalWrite(WR, LOW);		//Enable Writes on the AD5330
  PORTD = VOL;					//Set the voltage on the AD5330
  digitalWrite(WR, HIGH);		//Clock in the new data
  delayMicroseconds(FREQ/2);	//Wait for the specified amount of time
  digitalWrite(WR, LOW);		//Repeat for the 'low' portion of the wave.
  PORTD = 0x00;
  digitalWrite(WR, HIGH);
  delayMicroseconds(FREQ/2);  

  digitalWrite(CS, HIGH); 
  
  /*
  //Generate a Sine Wave (pretty slow...probably because of sine() function).
  digitalWrite(CS, LOW);
  for(int sinewave=0; sinewave < 360; sinewave++)
  {
    digitalWrite(WR, LOW);
    note = sin(deg2rad(sinewave));
    note *= 127;
    note += 127;
    PORTD = (unsigned char)note;
    digitalWrite(WR, HIGH);
    delayMicroseconds(FREQ);
  }  
  digitalWrite(CS, HIGH);
  */
}

double deg2rad(int degree)
{
  return (double)degree*(PI/180.0);
}

int main(void)
{
	init();

	setup();
    
	for (;;)
		loop();
        
	return 0;
}

