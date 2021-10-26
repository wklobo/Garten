//***********************************************************************************************************//
//*                                                                                                         *//
//* File:          datetime.h                                                                               *//
//* Author:        Alexander Müller                                                                         *//
//*                aus 'http://www.a-m-i.de/tips/datetime/datetime.php'                                     *//
//* Creation date: 2018-11-01;                                                                              *//
//* Last change:   2021-09-10 - 13:49:15: neuer Ansatz                                                      *//
//* Description:   In diesem Artikel habe ich eine Sammlung verschiedener Funktionen zusammengestellt,      *//
//*                mit denen man alle möglichen Arten von Kalender- und Zeitberechnungen durchführen kann.  *//
//*                Es ist eine Sammlung einzelner Routinen. Es findet relativ wenig Fehlerbehandlung von    *//
//*                unsinnigen Eingangsdaten statt, nur soviel wie nötig ist, um stabil zu laufen.           *//
//*                Ansonsten gilt: "Garbage in, Garbage out!"                                               *//
//*                                                                                                         *//
//***********************************************************************************************************//

#ifndef _DATETIME_H
#define _DATETIME_H 1


#ifndef EXTERN
#ifdef _MODUL0
  #define EXTERN
#else
  #define EXTERN extern
#endif
#endif

#include <stdint.h>

#define UINT          u_int16_t
#define BYTE          uint8_t
#define SYSTEMTIME    time_t
#define _ASSERT       assert

#define DATETIME  char*   /* '0000-00-00 00:00:00' */
#define DATE      char*   /* '0000-00-00'          */

//*******************************************************************************************************************************************//

bool istSchaltjahr(const UINT uJahr);                                             // Berechnen, ob ein Jahr ein Schaltjahr ist
short getAnzahlTageImMonat(const UINT uMonat, const UINT uJahr);                  // Bestimmung der Anzahl Tage pro Monat
short getAnzahlTageImJahr(const UINT uJahr);                                      // Die Anzahl der Tage eines Jahres
short getWochentag(const UINT uTag, const UINT uMonat, const UINT uJahr);         // Bestimmung des Wochentags für ein Datum
short getTagDesJahres(const UINT uTag, const UINT uMonat, const UINT uJahr);      // Der wievielte Tag des Jahres ist ein Datum
short getKalenderwoche(short uTag, short uMonat, short uJahr);                    // Kalenderwoche nach Industrienorm DIN 1355 berechnen
void getOsterdatum(const UINT uJahr, UINT* uTag, UINT* uMonat);                   // Ostersonntag mathematisch berechnen
void getViertenAdvent(const UINT uJahr, UINT* uTag, UINT* uMonat);                // Bestimmung der Adventssonntage und des Buß- und Bettages
//long ZeitDifferenzInJahren(const SYSTEMTIME* Startzeit,  const SYSTEMTIME* Endezeit); // const;
//long ZeitDifferenzInSekunden(const SYSTEMTIME* Startzeit, const SYSTEMTIME* Endezeit);

bool nextDay(const DATETIME Thisday, DATE Nextday);                               // ermittelt den nächsten Tag (für MYSQL)

char* heute(char* buf);                                                           // heutiges Datum als String
char* jetzt(char* buf);                                                           // Uhrzeit als String
#define TIMLEN     20
EXTERN char Uhrzeitbuffer[TIMLEN];
#define __NOW__   jetzt(Uhrzeitbuffer)
#define __HEUTE__ heute(Uhrzeitbuffer)

char* mkdatum(time_t zeit, char* buf);        // Datum/Uhrzeit aus time_t(=long)

// Laufzeitmessung
// ---------------
#define T_GESAMT      0
#define T_ABSCHNITT   1
#define T_FOLDER      2
#define T_FILES       3
#define T_DBASE       4
#define TIMER 6                   /* Anzahl Zeitmess-Timer */
bool Startzeit(int Timer);        // Zeitmessung starten
long Zwischenzeit(int Timer);     // Zeit nehmen/auslesen

// liegt aktuelle Zeit in einem Zeitfenster?
// -----------------------------------------
// Format Beginn/Schluss: 'hh:mm'
bool Zeitfenster(char* Beginn, char* Schluss);

//*******************************************************************************************************************************************//

#endif //_DATETIME_H
