//**************************************************************************//
//*                                                                        *//
//* File:          version.h                                               *//
//* Author:        Wolfgang Keuch                                          *//
//* Creation date: 2014-03-31;                                             *//
//* Last change:   2021-11-16 - 16:01:42                                   *//
//* Description:   Versions-Verwaltung                                     *//
//*                Die 'BUILD'-Nummer wird über ein Python-Programm erhöht *//
//* Copyright (C) 2014-2922 by Wolfgang Keuch                              *//
//*                                                                        *//
//**************************************************************************//

#ifndef _VERSION_H
#define _VERSION_H  1

#ifdef _MODUL0
  #define EXTERN static
#else
  #define EXTERN extern
#endif

EXTERN char Version[50];

#define MAXVERS 5
#define MINVERS 0
#define BUILD 15

#endif //_VERSION_H

//***************************************************************************//
