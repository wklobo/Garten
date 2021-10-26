// Raspi-Ausgang ein- und ausschalten
// -----------------------------------
// nach: "http://wiringpi.com/examples/quick2wire-and-wiringpi/install-and-testing/"
//Compile this with:
//		gcc -o Pin pin.c -lwiringPi
//and run:
//		sudo ./Pin pinnr on/off
// pinnr:  WiringPi-Pin
// onoff:  0/1

#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>

int main(int argc, char* argv[])
{
	int pinnr = atoi(argv[1]);
	int onoff = atoi(argv[2]);

  wiringPiSetup();
  pinMode (pinnr, OUTPUT);
  digitalWrite (pinnr, onoff); 
  
  return 0;
}
