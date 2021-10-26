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

#ifndef __GPIO_H__
#define __GPIO_H__

bool gpio_Init();
bool switchFan(bool OnOff, double Temperatur);
bool switchLED(int myLED, bool OnOff);
bool readTaste(int dieserSchalter);

//***************************************************************************//

//                  WiringgPi         GPIO
// ---------------------=====---------====-----------------------------------//
// I²C
#define SDA_PIN                         2  /*  Pin  3 - I2C DATA             */
#define SCL_PIN                         3  /*  Pin  5 - I2C CLOCK            */

// 1-Wire
#define ONEWIRE                         4  /*  Pin  7 - !-Wire-Pin           */

// kWh-Zähler
#define PULS_PIN          0    /* GPIO 17 -    Pin 11 - Pulseingang          */

// kleine Aufsatzplatine
#define  LED_ROT          1    /* GPIO 18 -    Pin 12 - rote LED             */
#define  LED_GRUEN        2    /* GPIO 27 -    Pin 13 - grüne LED            */
#define  EINGANG_1        3    /* GPIO 22 -    Pin 15 -                      */
#define  UMSCHALT         4    /* GPIO 23 -    Pin 16 - Umschalter Display   */
#define  UNBESCHALTET     5    /* GPIO 24 -    Pin 18 -                      */

//  Lüfter
#define FANPIN           24    /* GPIO 19 -   Pin 35 - Lüfter                 */

// DHT22
#define DHT22_GPIO                     25  /*  Pin 22 - DATA                  */

// TM1638 Display Module
#define STROBE                         26  /*  Pin 37 - STROBE                */
#define CLOCK                          20  /*  Pin 38 - CLOCK                 */
#define DATA                           21  /*  Pin 40 - DATA                  */

//****************************************************************************//

#endif /* __GPIO_H__ */
