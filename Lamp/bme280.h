//******************************************************************************//
//*                                                                            *//
//* File:          bme280.h                                                    *//
//* Author:        übernommen 2018-08-03 - an LampMotion angepasst -- wkh      *//
//* Creation date: 2018-08-03;                                                 *//
//* Last change:   2021-09-20 - 09:54:31                                       *//
//* Description:   BME 280 - Temperatur- und Luftdrucksensor lesen             *//
//*                                                                            *//
//*                                                                            *//
//******************************************************************************//
/*

Modified BSD License
====================

Copyright © 2016, Andrei Vainik
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the organization nor the
   names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This piece of code was combined from several sources
https://github.com/adafruit/Adafruit_BME280_Library
https://cdn-shop.adafruit.com/datasheets/BST-BME280_DS001-10.pdf
https://projects.drogon.net/raspberry-pi/wiringpi/i2c-library/


*****************************************************************************/

#ifndef __BME280_H__
#define __BME280_H__

// Sensortypen
// -----------
#define TYP_BME280_Temperatur       "BME280_T"
#define TYP_BME280_Luftdruck        "BME280_P"
#define TYP_BME280_Feuchtigkeit     "BME280_H"

bool bme280_Init();                   // Initialisierung des Sensors
int bme280_Read(float* Pressure,
                float* Temperature,   // Werte lesen
                float* Humidity);
int bme280_Close();                   // Sensor schließen

//**************************************************************************//

#endif /* __BME280_H__ */
