//***************************************************************************//
//*                                                                         *//
//* File:          ds1820.h                                                 *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2021-04-07;                                              *//
//* Last change:   2021-09-10 - 13:49:15: neuer Ansatz                      *//
//* Description:   Treiber für Temperatur-Sensor DS18B20:                   *//
//*                Treiber für CPU-Temperatur                               *//
//*                                                                         *//
//* Copyright (C) 2021 by Wolfgang Keuch                                    *//
//*                                                                         *//
//*                                                                         *//
//***************************************************************************//

#ifndef _DS1820_H
#define _DS1820_H  1

int ds1820_Refresh(void);                     // alle Sensoren registrieren
float ds1820_Value(int SensorNummer);         // Temperatur lesen
bool ds1820_Name(int SensorNummer, char* SensorName);
float InternalTemperatur(void);               // interne CPU-Temperatur lesen

#define MAXDS1820         12                  /* max. Anzahl DS1820-Sensoren */

#endif //_DS1820_H

//***************************************************************************//
