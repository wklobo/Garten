//*********************************************************************************************//
//*                                                                                           *//
//* File:          motLED.c                                                                   *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2014-07-26;                                                                *//
//* Last change:   2021-02-05;                                                                *//
//* Description:   LEDS im MOTION-Kasten über GPIO schalten                                   *//
//*                                                                                           *//
//* Copyright (C) 2014 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//* 2016-01-26 - übernommen aus:                                                              *//
//*     tiny_gpio.c                                                                           *//
//*     2015-09-12                                                                            *//
//*     Public Domain                                                                         *//
//*                                                                                           *//
//*                                                                                           *//
//* Aufruf:                                                                                   *//
//*     ./Motled                                       - volles Programm                      *//
//*     ./Motled 0                                     - Reset                                *//
//*     ./Motled Gruppe(0-2) Farbe(1-3) Ein/Aus(0,1)   - Einzelansteuerung                    *//
//*                                                                                           *//
//* Kompilation (zum Testen):                                                                 *//
//*  gcc -o Motled -std=gnu99 -DMAIN motLED.c                                                 *//
//*                                                                                           *//
//*********************************************************************************************//

#define _BSD_SOURCE     /* wg. usleep()           */

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "./motLED.h"

#define BUFLEN   300

unsigned piModel;
unsigned piRev;

static volatile uint32_t  *gpioReg = MAP_FAILED;

#define _MAIN     0     /* Test als Standalone-Programm */
#define _DEBUG    0

//************************************************************************************//

void gpioSetMode(unsigned gpio, unsigned mode)
{
  int reg, shift;

  reg   =  gpio/10;
  shift = (gpio%10) * 3;

  gpioReg[reg] = (gpioReg[reg] & ~(7<<shift)) | (mode<<shift);
}
//************************************************************************************//

int gpioGetMode(unsigned gpio)
{
  int reg, shift;

  reg   =  gpio/10;
  shift = (gpio%10) * 3;

  return (*(gpioReg + reg) >> shift) & 7;
}
//************************************************************************************//

void gpioSetPullUpDown(unsigned gpio, unsigned pud)
{
  *(gpioReg + GPPUD) = pud;
  usleep(20);

  *(gpioReg + GPPUDCLK0 + PI_BANK) = PI_BIT;
  usleep(20);

  *(gpioReg + GPPUD) = 0;
  *(gpioReg + GPPUDCLK0 + PI_BANK) = 0;
}
//************************************************************************************//

void gpioWrite(unsigned gpio, unsigned level)
{
  if (level == 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
  else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;
}
//************************************************************************************//

int gpioRead(unsigned gpio)
{
  if ((*(gpioReg + GPLEV0 + PI_BANK) & PI_BIT) != 0) return 1;
  else                                               return 0;
}
//************************************************************************************//

unsigned gpioHardwareRevision(void)
{
  static unsigned rev = 0;

  FILE * filp;
  char buf[512];
  char term;
  int chars=4; /* number of chars in revision string */

  if (rev) return rev;

  piModel = 0;

  filp = fopen ("/proc/cpuinfo", "r");

  if (filp != NULL)
  {
    while (fgets(buf, sizeof(buf), filp) != NULL)
    {
      if (piModel == 0)
      {
        if (!strncasecmp("model name", buf, 10))
        {
          if (strstr (buf, "ARMv6") != NULL)
          {
            piModel = 1;
            chars = 4;
          }
          else if (strstr (buf, "ARMv7") != NULL)
          {
            piModel = 2;
            chars = 6;
          }
        }
      }

      if (!strncasecmp("revision", buf, 8))
      {
        if (sscanf(buf+strlen(buf)-(chars+1),
           "%x%c", &rev, &term) == 2)
        {
          if (term != '\n') rev = 0;
        }
      }
    }

    fclose(filp);
  }
  return rev;
}
//************************************************************************************//

int gpioInitialise(void)
{
   int fd;

   piRev = gpioHardwareRevision(); /* sets piModel and piRev */

   fd = open("/dev/gpiomem", O_RDWR | O_SYNC) ;

   if (fd < 0)
   {
      fprintf(stderr, "failed to open /dev/gpiomem\n");
      return -1;
   }

   gpioReg = (uint32_t *)mmap(NULL, 0xB4, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   close(fd);

   if (gpioReg == MAP_FAILED)
   {
      fprintf(stderr, "Bad, mmap failed\n");
      return -1;
   }
   return 0;
}
//************************************************************************************//

void gpioTrigger(unsigned gpio, unsigned pulseLen, unsigned level)
{
   if (level == 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;

   usleep(pulseLen);

   if (level != 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;
}
//************************************************************************************//

/* Bit (1<<x) will be set if gpio x is high. */

uint32_t gpioReadBank1(void) { return (*(gpioReg + GPLEV0)); }
uint32_t gpioReadBank2(void) { return (*(gpioReg + GPLEV1)); }

/* To clear gpio x bit or in (1<<x). */

void gpioClearBank1(uint32_t bits) { *(gpioReg + GPCLR0) = bits; }
void gpioClearBank2(uint32_t bits) { *(gpioReg + GPCLR1) = bits; }

/* To set gpio x bit or in (1<<x). */

void gpioSetBank1(uint32_t bits) { *(gpioReg + GPSET0) = bits; }
void gpioSetBank2(uint32_t bits) { *(gpioReg + GPSET1) = bits; }

//*********************************************************************************************//

int switchLED(enum LEDGrps Group, enum LEDCols Color)
{
  enum LEDErrors ReturnVal = OK;
  enum LEDCols RoteLED;
  enum LEDCols GrunLED;
#if _DEBUG
  fprintf (stdout, "%s(): Group:%i, Color:%i\n",__FUNCTION__, Group, Color);
#endif

  switch(Group)
  {
    case SQLMOTION:
      RoteLED = FIFORED;
      GrunLED = FIFOGREEN;
      break;

    case FIFOMOTION:
      RoteLED = FIFORED;
      GrunLED = FIFOGREEN;
      break;

    case SQLTEMP:
      RoteLED = TEMPRED;
      GrunLED = TEMPGREEN;
      break;

    default:
      ReturnVal = GRPERR;
  }
  if (ReturnVal == OK)
  {
    switch(Color)
    {
      case AUS:
        gpioWrite(RoteLED, LOW);
        gpioWrite(GrunLED, LOW);
        break;
      case ROT:
        gpioWrite(RoteLED, HIGH);
        gpioWrite(GrunLED, LOW);
        break;
      case GRUEN:
        gpioWrite(RoteLED, LOW);
        gpioWrite(GrunLED, HIGH);
        break;
      case ORANGE:
        gpioWrite(RoteLED, HIGH);
        gpioWrite(GrunLED, HIGH);
        break;
      default:
        ReturnVal = COLERR;
    }
  }
#if _DEBUG
  if (ReturnVal < OK)
  {
    char ErrText[BUFLEN];                                        // Buffer für Fehlermeldungen
    sprintf(ErrText, "Error %s(): <%i> (Group:%i, Color:%i)\n",__FUNCTION__, ReturnVal, (int)Group, (int)Color);
    fprintf (stderr, ErrText);
  }
#endif
  return (int)ReturnVal;
}
//*********************************************************************************************//

int resetLED(enum LEDGrps Group)
{
  enum LEDErrors ReturnVal = OK;
#if _DEBUG
    fprintf (stdout, "%s(): Group:%i\n",__FUNCTION__, Group);
#endif
  int gpioStatus = gpioInitialise();
  if (gpioStatus < 0)
    ReturnVal = INITERR;
  else
  {
    switch(Group)
    {
      case SQLMOTION:
        gpioSetMode(FIFORED, PI_OUTPUT);       // Set as output.
        gpioSetMode(FIFOGREEN, PI_OUTPUT);     // Set as output.
        break;

      case FIFOMOTION:
        gpioSetMode(FIFORED, PI_OUTPUT);         // Set as output.
        gpioSetMode(FIFOGREEN, PI_OUTPUT);       // Set as output.
        break;

      case SQLTEMP:
        gpioSetMode(TEMPRED, PI_OUTPUT);         // Set as output.
        gpioSetMode(TEMPGREEN, PI_OUTPUT);       // Set as output.
        gpioSetMode(MOTOR, PI_OUTPUT);           // Set as output.
        break;

      default:
        return GRPERR;
    }
  }
#if _DEBUG
  if (ReturnVal < OK)
  {
    char ErrText[BUFLEN];                                        // Buffer für Fehlermeldungen
    sprintf(ErrText, "Error %s(): <%i> (Grp:%i)\n",__FUNCTION__, ReturnVal, Group);
    fprintf (stderr, ErrText);
  }
#endif
  return (int)ReturnVal;
}
//*********************************************************************************************//

int switchFan(enum LEDCmds EinAus)
{
  enum LEDErrors ReturnVal = OK;

#if _DEBUG
    fprintf (stdout, "%s(): - Command:%i\n",__FUNCTION__, EinAus);
#endif

  if (EinAus==ON)
    gpioWrite(MOTOR, HIGH);
  else if (EinAus==OFF)
    gpioWrite(MOTOR, LOW);
  else
    ReturnVal = CMDERR;

#if _DEBUG
  if (ReturnVal < OK)
  {
    char ErrText[BUFLEN];                                        // Buffer für Fehlermeldungen
    sprintf(ErrText, "Error %s(): <%i> (Cmd:%i)\n",__FUNCTION__, ReturnVal, EinAus);
    fprintf (stderr, ErrText);
  }
#endif
  return (int)ReturnVal;
}
//*********************************************************************************************//

#if _MAIN

// Testprogramm
// --------------------------------

int main (int argc, char **argv)
{
  enum LEDErrors ReturnVal = OK;
  enum LEDGrps Group = ALLES;
  enum LEDCols Color = AUS;
  enum LEDCmds Command = NIX;

#if _DEBUG
  fprintf(stdout, "argc=%d\n", argc);
#endif

  if (argc > 1)
  {
    if (argc == 3)
    {
      Group = atoi(argv[1])+ALLES;
      Color = atoi(argv[2])+AUS;
      if (Group==BLOWER)
        Command = atoi(argv[2])+NIX;
      else if((Group<ALLES) || (Group>LRESET))
        ReturnVal = GRPERR;
      else if((Color<AUS) || (Color>=NOCOLOR))
        ReturnVal = COLERR;
    }
    else
      ReturnVal = PARAMERR;
  }
#if _DEBUG
  if (Command==NIX)
    fprintf(stdout, "%s(Group=%i Color=%i): %i\n",__FUNCTION__, Group, Color, ReturnVal);
  else
    fprintf(stdout, "%s(Group=%i Command=%i): %i\n",__FUNCTION__, Group, Command, ReturnVal);
#endif

  if (ReturnVal==OK)
  {
    if (Group==LRESET)
    {
      ReturnVal=resetLED(SQLMOTION);
      if (ReturnVal==OK) ReturnVal=resetLED(FIFOMOTION);
      if (ReturnVal==OK) ReturnVal=resetLED(SQLTEMP);
    }

    else if (Group==ALLES)
    { // volles Programm
      // ---------------
      if (ReturnVal==OK) resetLED(SQLMOTION);
      if (ReturnVal==OK) resetLED(FIFOMOTION);
      if (ReturnVal==OK) resetLED(SQLTEMP);
      for (enum LEDGrps grp=SQLMOTION; grp < SQLTEMP+1; grp++)
      {
        for (enum LEDCols col=ROT; col < ORANGE+1; col++)
        {
          if (ReturnVal==OK) ReturnVal=switchLED( grp, col);
          sleep(2);
        }
      }
      switchFan(ON);
      sleep(3);
      for (enum LEDGrps grp=SQLMOTION; grp < SQLTEMP+1; grp++)
      {
        if (ReturnVal==OK) ReturnVal=switchLED( grp, AUS);
        sleep(2);
      }
      if (ReturnVal==OK) ReturnVal=switchFan(OFF);
    }

    else
    { // Einzelansteuerung
      // -----------------
      int gpioStatus = gpioInitialise();
      if (gpioStatus < 0)
        ReturnVal = INITERR;
      else
      {
        if (Group==BLOWER)
        { // Lüfter
          if (ReturnVal==OK) ReturnVal=switchFan(Command);
        }
        else
        { // LEDs
          if (ReturnVal==OK) ReturnVal=switchLED( Group, Color);
        }
      }
    }
  }

  return (int)ReturnVal;
}
//*********************************************************************************************//
#endif
