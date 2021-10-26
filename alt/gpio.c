//*********************************************************************************************//
//*                                                                                           *//
//* File:          gpio.c                                                                     *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2018-08-17;                                                                *//
//* Last change:   2020-09-20;                                                                *//
//* Description:   Bedienung der Raspi-GPIOs                                                  *//
//*                                                                                           *//
//* Copyright (C) 2018-20 by Wolfgang Keuch                                                   *//
//*                                                                                           *//
//*********************************************************************************************//

#include <wiringPi.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
//#include <mysql.h>

#include "./gpio.h"
#include "./error.h"
#include "./datetime.h"

static bool FanState = false; 

#define _DEBUG_ 1

//*********************************************************************************************//

bool gpio_Init()
{
  if (wiringPiSetup () == -1)
    exit(EXIT_FAILURE);
  pinMode(FANPIN, OUTPUT);              // Lüfter-Pin als Ausgang

//  pinMode(LED_ROT, OUTPUT);             // rote LED
//  pinMode(LED_GRUEN, OUTPUT);           // grüne LED
//
//  pinMode(UMSCHALT, INPUT);             // Umschalter Display
//  pullUpDnControl(UMSCHALT, PUD_UP);    // .. auf Pullup

  return true ;
}
//*********************************************************************************************//
//
//void trim(char *input)
//{
//   char *dst = input, *src = input;
//   char *end;
//
//   // Skip whitespace at front...
//   // --------------------------
//   while (isspace((unsigned char)*src))  ++src;
//
//   // Trim at end...
//   // -----------
//   end = src + strlen(src) - 1;
//   while (end > src && isspace((unsigned char)*end))  *end-- = 0;
//
//   // Move if needed.
//   // ----------------
//   if (src != dst)  while ((*dst++ = *src++));
//}
//*********************************************************************************************//

bool switchFan(bool OnOff, double Temperatur)
{
  pinMode(FANPIN, OUTPUT);            // Lüfter-Pin als Ausgang
#ifdef _DEBUG_
  fprintf(stdout, "%s-%s#%d: '%i'\n",jetzt(cNow),__FUNCTION__,__LINE__, OnOff);
#endif

  if (FanState != OnOff)
  { // der Status hat sich geändert
  	// ----------------------------
    char Bemerkung[50] = {'\0'};
    sprintf(Bemerkung, ">%s< bei %.1f °C", (OnOff==true ? "On" : "Off" ), Temperatur);
#ifdef _DEBUG_
		fprintf(stdout, "%s-%s#%d - Lüfter: %s\n",jetzt(cNow),__FUNCTION__,__LINE__, Bemerkung);
#endif

    FanState = OnOff;
  }
  digitalWrite(FANPIN, OnOff);
  return true;
}
//*********************************************************************************************//

bool switchLED(int myLED, bool OnOff)
{
  if (myLED==LED_ROT || myLED==LED_GRUEN)
  {
    pinMode(myLED, OUTPUT);            // Lüfter-Pin als Ausgang
#ifdef _DEBUG_
    fprintf(stdout, "%s-%s#%d: LED '%i'=>'%i'\n",jetzt(cNow),__FUNCTION__,__LINE__, myLED, OnOff);
#endif
    digitalWrite(myLED, OnOff);
    return true;
  }
  else
    return false;
}
//*********************************************************************************************//

bool readTaste(int dieserSchalter)
{
  return(bool)(digitalRead(dieserSchalter));
}
//*********************************************************************************************//
