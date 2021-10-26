//*********************************************************************************************//
//*                                                                                           *//
//* File:          motLED.h                                                                   *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2014-07-26;                                                                *//
//* Version:       $REV$                                                                      *//
//* Last change:   2021-02-05;                                                                *//
//* Description:   LEDS im MOTION-Kasten über GPIO schalten                                   *//
//*                                                                                           *//
//* Copyright (C) 2014 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef _MOTLED_H
#define _MOTLED_H  1

#define HIGH 1
#define LOW 0

#define GPSET0 7
#define GPSET1 8

#define GPCLR0 10
#define GPCLR1 11

#define GPLEV0 13
#define GPLEV1 14

#define GPPUD     37
#define GPPUDCLK0 38
#define GPPUDCLK1 39

#define PI_BANK (gpio>>5)
#define PI_BIT  (1<<(gpio&0x1F))

// gpio modes.  
// -------------
#define PI_INPUT  0
#define PI_OUTPUT 1
#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

// Values for pull-ups/downs off, pull-down and pull-up.  
// ----------------------------------------------------
#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2

// die physikalischen GPIO-Nummern
// -------------------------------
#define SQLRED     		18
#define SQLBLUE    		12
#define SQLGREEN   		16
#define FIFORED       22
#define FIFOBLUE      24
#define FIFOGREEN     26
#define TEMPRED       36
#define TEMPGREEN     38
#define MOTOR         12

enum LEDGrps
{
	ALLES=100,
  SQLMOTION,
  FIFOMOTION,
  SQLTEMP,
  LRESET,
  BLOWER,
  NOGROUP
};
 
enum LEDCmds
{
	NIX=200,
  ON,
  OFF,
  READ,
  NOCOMMAND
};

enum LEDCols
{
  AUS=300,
  ROT,
  GRUEN,
  ORANGE,
  NOCOLOR
};

enum LEDErrors
{
  OK=0,
  PARAMERR=-5,
  INITERR=-4,
  COLERR=-3,
  GRPERR=-2,
  CMDERR=-1,
};

int resetLED(enum LEDGrps);
int switchLED(enum LEDGrps, enum LEDCols);
int switchFan(enum LEDCmds);

//*********************************************************************************************//

//void gpioSetMode(unsigned gpio, unsigned mode);
//int gpioGetMode(unsigned gpio);
//void gpioSetPullUpDown(unsigned gpio, unsigned pud);
//void gpioWrite(unsigned gpio, unsigned level);
//int gpioRead(unsigned gpio);
//int gpioInitialise(void);

//*********************************************************************************************//

#endif //_MOTLED_H
