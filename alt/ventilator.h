//*********************************************************************************************//
//*                                                                                           *//
//* File:          ventilator.h                                                               *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2020-09-19;                                                                *//
//* Last change:   2020-09-20;                                                                *//
//* Description:   Ventilator - Lüftersteuerung für Raspi "Wetter"                            *//
//*                                                                                           *//
//* Copyright (C) 2020 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef _VENTILATOR_H
#define _VENTILATOR_H  1

//*********************************************************************************************//

#ifndef TRUE
#define TRUE        true
#define FALSE       false
#endif
#define ON          true
#define OFF         false 
#define BUFLEN       250

// allgemeine Konstanten
// -----------------------
#define SEC              1000
#define SLEEP             100           /* kleine Pause  [µsec]        	*/
#define ABFRAGETAKT        20*SEC     	/* Abfrage-Intervall [sec]  		*/

// Werte bei Verbindungsfehlern
// ----------------------------
#define VERSUCHE     60
#define PAUSE        12

// CPU-Temperaturgrenzen
// ------------------------
#define UNTERGRENZE		40  /* °C */
#define OBERGRENZE		50  /* °C */

//*********************************************************************************************//

#endif //_VENTILATOR_H

